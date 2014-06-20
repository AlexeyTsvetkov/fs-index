#ifndef STRUCTS_HPP
#define STRUCTS_HPP

#include <string>
#include <vector>


struct file_descriptor {
  file_descriptor() {}
  file_descriptor(size_t path_id, std::string file_name)
  : path_id_(path_id), file_name_(file_name) {}

  size_t path() const { return path_id_; }
  const std::string& name() const { return file_name_; } 

private:
  size_t path_id_;
  std::string file_name_;
};


struct suffix_descriptor {
  suffix_descriptor() {}
  suffix_descriptor(size_t file_id, size_t offset)
  : file_id_(file_id), offset_(offset) {}

  const std::string& full_name(const std::vector<file_descriptor>& files) const {
    return files[file_id_].name();
  }

  std::string suffix(const std::vector<file_descriptor>& files) const {
    return files[file_id_].name().substr(offset_);
  }

  char get_char(const std::vector<file_descriptor>& files, size_t index) const {
    return files[file_id_].name()[offset_ + index];
  }

  size_t size(const std::vector<file_descriptor>& files) const {
    return files[file_id_].name().size() - offset_;
  }

  size_t file() const { return file_id_; }
  size_t offset() const { return offset_; }

private:
  size_t file_id_;
  size_t offset_;
};


inline bool suffix_less(
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

#endif