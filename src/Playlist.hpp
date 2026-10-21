#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace modern_ices {

class Playlist {
  public:
    explicit Playlist(const std::string& path);

    const std::string& next();
    bool empty() const { return entries_.empty(); }
    std::size_t size() const { return entries_.size(); }

  private:
    std::vector<std::string> entries_;
    std::size_t cursor_ = 0;
};

} // namespace modern_ices
