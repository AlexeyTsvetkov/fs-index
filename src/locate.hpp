#ifndef LOCATE_HPP
#define LOCATE_HPP

#include <fstream>
#include <cstddef>
#include <vector>
#include <string>

#include "structs.hpp"
#include "utils.hpp"


class locate {
  typedef std::vector<suffix_descriptor>::iterator suf_it;
public:
  locate(std::ifstream& in);
  locate(const locate& other) = delete;
  const locate& operator=(const locate& other) = delete;
  ~locate() {}

  void find(const std::string& pattern);
  bool good() { return in_.good(); }
private:
  size_t read_num() { return read_number(in_); }
  std::string read_str() { return read_string(in_); }
  file_descriptor read_fd() { return read_file(in_); }
  suffix_descriptor read_sd() { return read_suffix(in_); }
  void traverse_suffixes(suf_it it, suf_it end);

  std::ifstream& in_;
  std::vector<std::string> paths_;
  std::vector<file_descriptor> files_;
  std::vector<suffix_descriptor> suffixes_;
};

#endif