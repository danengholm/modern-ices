#pragma once

#include "AudioDecoder.hpp"

#include <string>

// vorbisfile declares callback presets as static variables in the public header.
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include <vorbis/vorbisfile.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace modern_ices {

// Ogg Vorbis decoder built on vorbisfile; it emits normalized signed 16-bit PCM blocks.
class VorbisDecoder final : public PcmDecoder {
  public:
    explicit VorbisDecoder(const std::string& path);
    ~VorbisDecoder() override;

    VorbisDecoder(const VorbisDecoder&) = delete;
    VorbisDecoder& operator=(const VorbisDecoder&) = delete;

    PcmBlock read() override;
    const TrackMetadata& metadata() const override;

  private:
    OggVorbis_File file_{};
    bool opened_{false};
    int current_section_{0};
    TrackMetadata metadata_;

    void read_comments();
    static const char* error_name(long rc);
};

} // namespace modern_ices
