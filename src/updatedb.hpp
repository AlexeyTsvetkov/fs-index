#ifndef UPDATE_DB_HPP
#define UPDATE_DB_HPP

#include <atomic>
#include <fstream>
#include <mutex>
#include <queue>
#include <string>
#include <vector>

#include <boost/filesystem.hpp>
#include <cstddef>

#include "structs.hpp"
#include "utils.hpp"

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
  void add_to_queue(const boost::filesystem::path& entry);
  void add_to_index(const boost::filesystem::path& entry);
  size_t write_path(const boost::filesystem::path& entry);
  size_t add_file(file_descriptor fd);
  void add_suffixes(size_t file_id, size_t name_size);
  void write_index();
  void sort_suffixes();
  void sort_thread(std::vector<suffix_descriptor>* buckets);
  void sort_bucket(std::vector<suffix_descriptor>& bucket);

  void write_str(const std::string& str) { 
    write_string(out_, str); 
  }

  void write_num(size_t num) { 
    write_number(out_, num); 
  }

  void write_fd(const file_descriptor& fd) { 
    write_file(out_, fd); 
  }

  void write_sd(const suffix_descriptor& sd) { 
    write_suffix(out_, sd); 
  }

  static const unsigned int ALPHABET = 256;

  const unsigned int max_threads_;
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

#endif