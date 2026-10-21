#pragma once

#include "AudioDecoder.hpp"

#include <cstdint>
#include <lame/lame.h>
#include <vector>

namespace modern_ices {

// RAII wrapper around a LAME encoder configured from the first decoded PCM block.
class LameEncoder {
  public:
    LameEncoder(long sample_rate, int channels, int bitrate_kbps);
    ~LameEncoder();

    LameEncoder(const LameEncoder&) = delete;
    LameEncoder& operator=(const LameEncoder&) = delete;

    std::vector<unsigned char> encode(const PcmBlock& pcm);
    std::vector<unsigned char> flush();

  private:
    lame_global_flags* flags_{nullptr};

    void check_set(int rc, const char* action);
};

} // namespace modern_ices
