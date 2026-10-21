#pragma once

#include "Config.hpp"

#include <string>

namespace modern_ices {

class Streamer {
  public:
    explicit Streamer(Config config);
    void run();

  private:
    Config config_;

    void stream_file(const std::string& path);
    void stream_passthrough(const std::string& path);
    void stream_transcoded(const std::string& path);
};

} // namespace modern_ices
