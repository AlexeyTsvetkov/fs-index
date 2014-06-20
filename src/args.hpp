#ifndef ARGS_HPP
#define ARGS_HPP

#include <string>
#include "utils.hpp"

bool read_locate_args(int argc, char** argv, std::string& pattern, std::string& index_path) {
  bool pattern_set = false;
  bool index_set   = false;

  for (int i = 1; i < argc; ++i) {
    std::string arg = *(argv + i);

    if (arg == "--database") {
      if (++i < argc) {
        index_set = true;
        index_path.assign(*(argv + i));
      }

      continue;
    } 

    if (!pattern_set) { 
      pattern_set = true;
      pattern.assign(arg);
    } else {
      report_error("Unexpected argument " + arg);
      return false;
    }
  }

  bool all_set = pattern_set && index_set;
  if (!all_set) 
    report_error("Not enough arguments");
  
  return all_set;
}


bool read_update_args(int argc, char** argv, std::string& root, std::string& index_path) {
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

#endif