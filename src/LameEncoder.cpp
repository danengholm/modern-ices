#include "LameEncoder.hpp"

#include "Error.hpp"

#include <algorithm>
#include <string>

namespace modern_ices {

LameEncoder::LameEncoder(long sample_rate, int channels, int bitrate_kbps) : flags_{lame_init()} {
    if (!flags_) {
        throw Error("lame_init failed");
    }
    check_set(lame_set_in_samplerate(flags_, static_cast<int>(sample_rate)),
              "set input sample rate");
    check_set(lame_set_num_channels(flags_, channels == 1 ? 1 : 2), "set channel count");
    check_set(lame_set_brate(flags_, bitrate_kbps), "set bitrate");
    check_set(lame_set_original(flags_, 0), "clear original flag");
    if (channels == 1) {
        check_set(lame_set_mode(flags_, MONO), "set mono mode");
    }
    if (lame_init_params(flags_) < 0) {
        throw Error("lame_init_params failed");
    }
}

LameEncoder::~LameEncoder() {
    if (flags_) {
        lame_close(flags_);
    }
}

// LAME accepts separate left/right buffers, which matches the decoder-facing PcmBlock layout.
std::vector<unsigned char> LameEncoder::encode(const PcmBlock& pcm) {
    if (pcm.empty()) {
        return {};
    }
    const int samples{pcm.samples()};
    std::vector<unsigned char> output(7200 + static_cast<std::size_t>(samples) * 5 / 4);
    const int bytes{lame_encode_buffer(flags_, pcm.left.data(), pcm.right.data(), samples,
                                       output.data(), static_cast<int>(output.size()))};
    if (bytes < 0) {
        throw Error("lame_encode_buffer failed with code " + std::to_string(bytes));
    }
    output.resize(static_cast<std::size_t>(bytes));
    return output;
}

std::vector<unsigned char> LameEncoder::flush() {
    std::vector<unsigned char> output(7200);
    const int bytes{lame_encode_flush(flags_, output.data(), static_cast<int>(output.size()))};
    if (bytes < 0) {
        throw Error("lame_encode_flush failed with code " + std::to_string(bytes));
    }
    output.resize(static_cast<std::size_t>(bytes));
    return output;
}

void LameEncoder::check_set(int rc, const char* action) {
    if (rc < 0) {
        throw Error(std::string("LAME failed to ") + action + " (code " + std::to_string(rc) + ")");
    }
}

} // namespace modern_ices
