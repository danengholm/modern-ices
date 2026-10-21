#pragma once

#include "Config.hpp"

#include <string>

namespace modern_ices {

class Playlist;

// Coordinates playlist iteration, source connections, optional decoding, and MP3 output.
class Streamer {
  public:
    explicit Streamer(Config config);
    void run();

  private:
    Config config_;

    void stream_file(const std::string& path);
    void stream_passthrough(const std::string& path);
    void stream_transcoded(const std::string& path);
    void stream_crossfaded(Playlist& playlist);
};

} // namespace modern_ices
