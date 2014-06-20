#include <algorithm>
#include <functional>
#include <iostream>
#include <thread>

#include <boost/filesystem/exception.hpp>
#include <cstdlib>

#include "updatedb.hpp"
#include "args.hpp"


namespace fs  = boost::filesystem;
namespace sys = boost::system;


int main(int argc, char** argv) {
  std::string root;
  std::string index_path;
  if (!read_update_args(argc, argv, root, index_path)) {
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
: max_threads_(std::max((unsigned int) 2,  std::thread::hardware_concurrency())), 
  dirs_(), files_(), 
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
  for (unsigned int i = 0; i < max_threads_; ++i) {
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

      try {
        process_dir(dir);
      } catch (std::exception& e) {
        report_error("Could not process directory " + dir);
      }
      
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


size_t update_db::write_path(const fs::path& entry) {
  std::lock_guard<std::mutex> lock(out_m_);
  write_str(entry.string());
  return paths_count_++;
}


size_t update_db::add_file(file_descriptor fd) {
  std::lock_guard<std::mutex> lock(file_m_);
  size_t file_id = files_.size();
  files_.push_back(fd);
  return file_id;
}


void update_db::add_suffixes(size_t file_id, size_t name_size) {
  std::lock_guard<std::mutex> lock(suffixes_m_);
  for (size_t offset = 0; offset < name_size; ++offset) {
    suffixes_.push_back(suffix_descriptor(file_id, offset));
  }
}


void update_db::add_to_index(const fs::path& entry) {
  std::string fname = entry.filename().string();
  
  size_t path_id = write_path(entry);
  size_t file_id = add_file(file_descriptor(path_id, fname));
  add_suffixes(file_id, fname.size());
}


void update_db::sort_suffixes() {
  std::vector<suffix_descriptor> buckets[ALPHABET];
  for (suffix_descriptor suffix : suffixes_) {
    char first_letter = suffix.get_char(files_, 0);
    buckets[(unsigned char) first_letter].push_back(suffix);
  }


  std::vector<std::thread> threads;
  for (unsigned int i = 0; i < max_threads_; ++i) {
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