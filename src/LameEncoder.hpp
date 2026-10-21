#pragma once

#include "AudioDecoder.hpp"

#include <cstdint>
#include <vector>
#include <lame/lame.h>

namespace modern_ices {

class LameEncoder {
public:
  LameEncoder(long sample_rate, int channels, int bitrate_kbps);
  ~LameEncoder();

  LameEncoder(const LameEncoder&) = delete;
  LameEncoder& operator=(const LameEncoder&) = delete;

  std::vector<unsigned char> encode(const PcmBlock& pcm);
  std::vector<unsigned char> flush();

private:
  lame_global_flags* flags_ = nullptr;

  void check_set(int rc, const char* action);
};

} // namespace modern_ices
