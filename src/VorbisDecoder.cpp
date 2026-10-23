#include "VorbisDecoder.hpp"

#include "Error.hpp"
#include "TrackMetadata.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace modern_ices {
namespace {

// vorbisfile can decode directly to floats; clamp before converting to 16-bit PCM.
std::int16_t float_to_i16(float sample) {
    const float clipped{std::clamp(sample, -1.0F, 1.0F)};
    return static_cast<std::int16_t>(std::lrint(clipped * 32767.0F));
}

} // namespace

VorbisDecoder::VorbisDecoder(const std::string& path) {
    const int rc{ov_fopen(path.c_str(), &file_)};
    if (rc != 0) {
        throw Error("unable to open Ogg Vorbis input file " + path + ": " + error_name(rc));
    }
    opened_ = true;
    metadata_.fallback = metadata_fallback_from_path(path);
    read_comments();

    const vorbis_info* info{ov_info(&file_, -1)};
    if (!info) {
        throw Error("Ogg Vorbis stream did not provide stream info: " + path);
    }
    if (info->channels != 1 && info->channels != 2) {
        throw Error("Ogg Vorbis stream has unsupported channel count: " +
                    std::to_string(info->channels));
    }
    if (info->rate <= 0) {
        throw Error("Ogg Vorbis stream has invalid sample rate");
    }
}

const TrackMetadata& VorbisDecoder::metadata() const {
    return metadata_;
}

void VorbisDecoder::read_comments() {
    const auto* comments{ov_comment(&file_, -1)};
    if (!comments) {
        return;
    }
    for (int i{0}; i < comments->comments; ++i) {
        if (comments->user_comments[i] && comments->comment_lengths[i] > 0) {
            apply_vorbis_comment(metadata_, std::string_view(comments->user_comments[i],
                                                             comments->comment_lengths[i]));
        }
    }
}

VorbisDecoder::~VorbisDecoder() {
    if (opened_) {
        ov_clear(&file_);
    }
}

// Ogg can contain chained logical bitstreams, so validate the active section each read.
PcmBlock VorbisDecoder::read() {
    constexpr int max_samples{4096};

    for (;;) {
        float** pcm{nullptr};
        const long samples = ov_read_float(&file_, &pcm, max_samples, &current_section_);
        if (samples == 0) {
            return {};
        }
        if (samples < 0) {
            throw Error(std::string("Ogg Vorbis decode failed: ") + error_name(samples));
        }

        const vorbis_info* info{ov_info(&file_, current_section_)};
        if (!info) {
            throw Error("Ogg Vorbis stream changed to an invalid logical bitstream");
        }
        if (info->channels != 1 && info->channels != 2) {
            throw Error("Ogg Vorbis stream has unsupported channel count: " +
                        std::to_string(info->channels));
        }

        PcmBlock block{};
        block.sample_rate = info->rate;
        block.channels = info->channels;
        block.left.reserve(static_cast<std::size_t>(samples));
        block.right.reserve(static_cast<std::size_t>(samples));

        for (long i{0}; i < samples; ++i) {
            const auto left{float_to_i16(pcm[0][i])};
            block.left.push_back(left);
            block.right.push_back(info->channels == 1 ? left : float_to_i16(pcm[1][i]));
        }
        return block;
    }
}

const char* VorbisDecoder::error_name(long rc) {
    switch (rc) {
    case OV_FALSE:
        return "not Vorbis data";
    case OV_EOF:
        return "end of file";
    case OV_HOLE:
        return "interrupted data or missing page";
    case OV_EREAD:
        return "read error";
    case OV_EFAULT:
        return "internal fault";
    case OV_EIMPL:
        return "unsupported feature";
    case OV_EINVAL:
        return "invalid argument or corrupt stream";
    case OV_ENOTVORBIS:
        return "not a Vorbis stream";
    case OV_EBADHEADER:
        return "bad Vorbis header";
    case OV_EVERSION:
        return "Vorbis version mismatch";
    case OV_ENOTAUDIO:
        return "packet does not contain audio";
    case OV_EBADPACKET:
        return "bad Vorbis packet";
    case OV_EBADLINK:
        return "bad logical bitstream link";
    case OV_ENOSEEK:
        return "stream is not seekable";
    default:
        return "unknown libvorbisfile error";
    }
}

} // namespace modern_ices
