#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "TrackMetadata.hpp"

namespace modern_ices {

// Planar signed 16-bit PCM shared between decoders and the LAME encoder.
struct PcmBlock {
    std::vector<std::int16_t> left;
    std::vector<std::int16_t> right;
    long sample_rate{0};
    int channels{0};

    bool empty() const { return left.empty(); }
    int samples() const { return static_cast<int>(left.size()); }
};

// Common pull interface for file decoders. Each read returns one block or an empty block at EOF.
class PcmDecoder {
  public:
    virtual ~PcmDecoder() = default;

    PcmDecoder(const PcmDecoder&) = delete;
    PcmDecoder& operator=(const PcmDecoder&) = delete;

    virtual PcmBlock read() = 0;
    virtual const TrackMetadata& metadata() const = 0;

  protected:
    PcmDecoder() = default;
};

// Opens the path, identifies the audio container/codec, and returns the matching decoder.
std::unique_ptr<PcmDecoder> open_audio_decoder(const std::string& path);

} // namespace modern_ices
