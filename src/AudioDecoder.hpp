#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace modern_ices {

struct PcmBlock {
    std::vector<std::int16_t> left;
    std::vector<std::int16_t> right;
    long sample_rate{0};
    int channels{0};

    bool empty() const { return left.empty(); }
    int samples() const { return static_cast<int>(left.size()); }
};

class PcmDecoder {
  public:
    virtual ~PcmDecoder() = default;

    PcmDecoder(const PcmDecoder&) = delete;
    PcmDecoder& operator=(const PcmDecoder&) = delete;

    virtual PcmBlock read() = 0;

  protected:
    PcmDecoder() = default;
};

std::unique_ptr<PcmDecoder> open_audio_decoder(const std::string& path);

} // namespace modern_ices
