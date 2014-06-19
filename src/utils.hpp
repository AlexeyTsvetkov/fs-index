#ifndef UTILS_HPP
#define UTILS_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <cstddef>

#include "structs.hpp"

static const size_t BITS_IN_BYTE = 8;

inline void write_number(std::ofstream& out, size_t num) {
  for(size_t i = 0; i < 4; i++) {
    unsigned char byte = (num >> (i * BITS_IN_BYTE)) & 0xff;
    out.put(byte);
  }
}

inline void write_string(std::ofstream& out, const std::string& str) {
  write_number(out, str.size());
  for (size_t i = 0; i < str.size(); ++i) {
    out.put(str[i]);
  }
}

inline void write_file(std::ofstream& out, const file_descriptor& fd) {
  write_number(out, fd.path());
  write_string(out, fd.name());
}

inline void write_suffix(std::ofstream& out, const suffix_descriptor& sd) {
  write_number(out, sd.file());
  write_number(out, sd.offset());
}

inline size_t read_number(std::ifstream& in) {
  size_t num = 0;
  
  for (size_t i = 0; i < 4; i++) {
    unsigned char byte;
    in.get(reinterpret_cast<char&>(byte));

    num |= ((size_t)byte << (i * BITS_IN_BYTE));
  }

  return num;
}

inline std::string read_string(std::ifstream& in) {
  size_t size = read_number(in);
  std::string result(size, '\0');
  
  for (size_t i = 0; i < size; ++i) {
    in.get(result[i]);
  }

  return result;
}

inline file_descriptor read_file(std::ifstream& in) {
  size_t path_id        = read_number(in);
  std::string file_name = read_string(in);

  return file_descriptor(path_id, file_name);
}

inline suffix_descriptor read_suffix(std::ifstream& in) {
  size_t file_id = read_number(in);
  size_t offset  = read_number(in);

  return suffix_descriptor(file_id, offset);
}

inline void report_error(const std::string& msg) {
  std::cerr << "Error: " << msg << std::endl;
}

inline bool starts_with(const std::string& str, const std::string& prefix) {
  if (prefix.size() > str.size()) 
    return false;

  for (size_t i = 0; i < prefix.size(); ++i) {
    if (prefix[i] != str[i])
      return false;
  }

  return true;
}

#endif