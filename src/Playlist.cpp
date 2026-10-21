#include "Playlist.hpp"

#include "Error.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace modern_ices {
namespace {

std::string trim(std::string line) {
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  line.erase(line.begin(), std::find_if(line.begin(), line.end(), not_space));
  line.erase(std::find_if(line.rbegin(), line.rend(), not_space).base(), line.end());
  return line;
}

} // namespace

Playlist::Playlist(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw Error("unable to open playlist: " + path);
  }

  std::string line;
  while (std::getline(in, line)) {
    line = trim(line);
    if (!line.empty() && line.front() != '#') {
      entries_.push_back(line);
    }
  }

  if (in.bad()) {
    throw Error("error while reading playlist: " + path);
  }
  if (entries_.empty()) {
    throw Error("playlist contains no playable entries: " + path);
  }
}

const std::string& Playlist::next() {
  if (entries_.empty()) {
    throw Error("playlist is empty");
  }
  const auto& item = entries_[cursor_];
  cursor_ = (cursor_ + 1) % entries_.size();
  return item;
}

} // namespace modern_ices
