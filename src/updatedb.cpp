#include <atomic>
#include <fstream>
#include <functional>
#include <iostream>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <boost/filesystem.hpp>
#include <boost/filesystem/exception.hpp>
#include <cstdlib>

#include "structs.hpp"
#include "utils.hpp"

namespace fs  = boost::filesystem;
namespace sys = boost::system;

class update_db {
public:
  update_db(const std::string& root, std::ofstream& out);
  update_db(const update_db& other) = delete;
  const update_db& operator=(const update_db& other) = delete;
  ~update_db();

  void update();
private:

  void traverse();
  void process_dir(const std::string& dir);
  void add_to_queue(const fs::path& entry);
  void add_to_index(const fs::path& entry);

  void write_str(const std::string& str) { write_string(out_, str); }
  void write_num(size_t num) { write_number(out_, num); }
  void write_fd(const file_descriptor& fd) { write_file(out_, fd); }
  void write_sd(const suffix_descriptor& sd) { write_suffix(out_, sd); }
  void write_index();
  void sort_suffixes();
  void sort_thread(std::vector<suffix_descriptor>* buckets);
  void sort_bucket(std::vector<suffix_descriptor>& bucket);

  static const unsigned int MAX_THREADS = 8;
  static const unsigned int ALPHABET = 256;

  std::queue<std::string>  dirs_;
  std::vector<file_descriptor> files_;
  std::vector<suffix_descriptor> suffixes_;
  std::ofstream& out_;
  std::atomic<int> processing_count_;
  unsigned int buckets_processed_;
  size_t paths_count_;
  std::mutex dir_m_;
  std::mutex file_m_;
  std::mutex suffixes_m_;
  std::mutex out_m_;
  std::mutex buckets_m_;
};


bool read_args(int argc, char** argv, std::string& root, std::string& index_path) {
  bool root_set  = false;
  bool index_set = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = *(argv + i);

    if (arg == "--database-root") {
      if (++i < argc) {
        root_set = true;
        root.assign(*(argv + i));
      }

      continue;
    } 

    if (arg == "--output") {
      if (++i < argc) {
        index_set = true;
        index_path.assign(*(argv + i));
      }

      continue;
    } 

    report_error("Unexpected argument " + arg);
    return false;
  }

  bool all_set = root_set && index_set;
  if (!all_set) 
    report_error("Not enough arguments");
  
  return all_set;
}


int main(int argc, char** argv) {
  std::string root;
  std::string index_path;
  if (!read_args(argc, argv, root, index_path)) {
    return EXIT_FAILURE;
  }

  sys::error_code err;
  if (!fs::is_directory(root, err)) {
    report_error("Expected ROOT to be an existing directory");
    return EXIT_FAILURE;
  }

  std::ofstream out(index_path, std::ofstream::binary);  
  if (!out.good()) {
    report_error("Unable to open index file for writing");
    return EXIT_FAILURE;
  }
  
  update_db udb(root, out);
  udb.update();
  
  return EXIT_SUCCESS;
}


update_db::update_db(const std::string& root, std::ofstream& out)
: dirs_(), files_(), 
  suffixes_(), out_(out), 
  processing_count_(0),
  buckets_processed_(0),
  paths_count_(0),
  dir_m_(), file_m_(),
  suffixes_m_(), out_m_()
{
  const size_t paths_counter_placeholder = 0;
  write_num(paths_counter_placeholder);
  dirs_.push(root);
  add_to_index(root);
}


update_db::~update_db() {
  out_.clear();       
  out_.seekp(0, std::ios::beg);
  write_num(paths_count_);
}


void update_db::update() {
  std::vector<std::thread> threads;
  for (unsigned int i = 0; i < MAX_THREADS; ++i) {
    threads.push_back(std::thread(&update_db::traverse, this));
  }

  for (std::thread& thread : threads) {
    thread.join();
  }

  sort_suffixes();
  write_index();
}


void update_db::traverse() {
  while (true) {
    dir_m_.lock();

    if (!dirs_.empty()) {
      std::string dir = dirs_.front();
      dirs_.pop();

      processing_count_++;
      dir_m_.unlock();

      process_dir(dir);
      processing_count_--;
    } else {
      dir_m_.unlock();
      if (processing_count_.load() <= 0) 
        break;
    }
  }
}


void update_db::process_dir(const std::string& dir) {
  sys::error_code err;
  fs::path path(dir);

  fs::directory_iterator end;
  fs::directory_iterator it(path, err);
  
  if (err != 0) {
    report_error("Could not traverse: " + dir);
    return;
  }

  for (; it != end; ++it) {
    const fs::path& entry = *it;
    fs::path canonical = fs::canonical(entry, err);
    if (err != 0) 
      continue;

    if (fs::is_directory(entry, err)) 
      add_to_queue(canonical);
    
    add_to_index(canonical);
  }
}


void update_db::add_to_queue(const fs::path& entry) {
  std::lock_guard<std::mutex> lock(dir_m_);
  dirs_.push(entry.string());
}


void update_db::add_to_index(const fs::path& entry) {
  std::string fname = entry.filename().string();
  
  out_m_.lock();
  write_str(entry.string());
  size_t path_id = paths_count_++;
  out_m_.unlock();

  file_m_.lock();
  size_t file_id = files_.size();
  files_.push_back(file_descriptor(path_id, fname));
  file_m_.unlock();

  suffixes_m_.lock();
  for (size_t offset = 0; offset < fname.size(); ++offset) {
    suffixes_.push_back(suffix_descriptor(file_id, offset));
  }
  suffixes_m_.unlock();
}


void update_db::sort_suffixes() {
  std::vector<suffix_descriptor> buckets[ALPHABET];
  for (suffix_descriptor suffix : suffixes_) {
    char first_letter = suffix.get_char(files_, 0);
    buckets[(unsigned char) first_letter].push_back(suffix);
  }


  std::vector<std::thread> threads;
  for (unsigned int i = 0; i < MAX_THREADS; ++i) {
    threads.push_back(std::thread(&update_db::sort_thread, this, buckets));
  }

  for (std::thread& thread : threads) {
    thread.join();
  }

  size_t i = 0;
  for (unsigned int buc = 0; buc < ALPHABET; ++buc) {
    for (size_t suf = 0; suf < buckets[buc].size(); suf++) 
      suffixes_[i++] = buckets[buc][suf]; 
  }
}


void update_db::sort_thread(std::vector<suffix_descriptor>* buckets) {
  while(true) {
    buckets_m_.lock();
    
    if (buckets_processed_ >= ALPHABET) {
      buckets_m_.unlock();
      break;
    } 

    unsigned int i = buckets_processed_++;
    buckets_m_.unlock();

    sort_bucket(*(buckets + i));
  }
}


bool suffix_less(
  const std::vector<file_descriptor>& files, 
  const suffix_descriptor& sd1, 
  const suffix_descriptor& sd2
) {
  const std::string& n1 = sd1.full_name(files); 
  const std::string& n2 = sd2.full_name(files); 
  int cmp = n1.compare(sd1.offset(), std::string::npos, 
                       n2, sd2.offset(), std::string::npos);

  return cmp < 0;
}


void update_db::sort_bucket(std::vector<suffix_descriptor>& bucket) {
  typedef suffix_descriptor sd;
  std::sort(bucket.begin(), bucket.end(), 
    [this](const sd& sd1, const sd& sd2) { 
      return suffix_less(this->files_, sd1, sd2); 
    });
}


void update_db::write_index() {
  size_t fsize = files_.size();
  write_num(fsize);
  for (size_t i = 0; i < fsize; ++i) {
    write_fd(files_[i]);
  }

  size_t ssize = suffixes_.size();
  write_num(ssize);
  for (size_t i = 0; i < ssize; ++i) {
    write_sd(suffixes_[i]);
  }
}