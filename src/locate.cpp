#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <set>

#include <boost/filesystem.hpp>
#include <boost/filesystem/exception.hpp>
#include <cstdlib>

#include "args.hpp"
#include "locate.hpp"


namespace fs  = boost::filesystem;
namespace sys = boost::system;


int main(int argc, char** argv) {
  std::string pattern;
  std::string index_path;
  if (!read_locate_args(argc, argv, pattern, index_path)) {
    return EXIT_FAILURE;
  }

  sys::error_code err;

  std::ifstream in(index_path, std::ifstream::binary);
  try {
    locate loc(in);
    if (!loc.good()) {
      report_error("Could not read index");
      return EXIT_FAILURE;
    }

    loc.find(pattern);
  } catch (std::exception& e) {
    report_error(e.what());
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}


locate::locate(std::ifstream& in)
: in_(in), paths_(), files_(), suffixes_()
{
  size_t paths_count = read_num();
  paths_.resize(paths_count);
  for (size_t i = 0; i < paths_count; ++i) 
    paths_[i] = read_str();
  
  size_t files_count = read_num();
  files_.resize(files_count);
  for (size_t i = 0; i < files_count; ++i) 
    files_[i] = read_fd();
  
  size_t suffix_count = read_num();
  suffixes_.resize(suffix_count);
  for (size_t i = 0; i < suffix_count; ++i) 
    suffixes_[i] = read_sd();
}


void locate::find(const std::string& pattern) {
  std::function<int(const suffix_descriptor&, const std::string&)> 
  cmp = [this](const suffix_descriptor& sd, const std::string& p) {
    const std::string& name = sd.full_name(this->files_);
    return name.compare(sd.offset(), std::string::npos, p);
  };

  suf_it low = std::lower_bound(suffixes_.begin(), suffixes_.end(), pattern, 
    [cmp](const suffix_descriptor& sd, const std::string& p) -> bool {return cmp(sd, p) < 0;});

  if (low == suffixes_.end()
    || !starts_with(low->suffix(files_), pattern)) 
  { return; }

  suf_it hi = std::upper_bound(suffixes_.begin(), suffixes_.end(), pattern, 
    [cmp, this](const std::string& p, const suffix_descriptor& sd) -> bool {
      return cmp(sd, p) > 0 && !starts_with(sd.suffix(this->files_), p); 
    });

  traverse_suffixes(low, hi);
}


void locate::traverse_suffixes(suf_it low, suf_it hi) {
  std::set<size_t> file_ids;
  for(;low != hi; ++low) {
    file_ids.insert(low->file());
  }

  sys::error_code err;
  for (size_t file_id : file_ids) {
    size_t path_id = files_[file_id].path();
    fs::path entry(paths_[path_id]);
    
    if (fs::exists(entry, err))
      std::cout << entry.string() << std::endl;
  }
}