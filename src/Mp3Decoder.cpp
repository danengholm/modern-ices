#include "Mp3Decoder.hpp"

#include "Error.hpp"

#include <string>

namespace modern_ices {

Mpg123Runtime::Mpg123Runtime() {
    int rc = mpg123_init();
    if (rc != MPG123_OK) {
        throw Error(std::string("mpg123_init failed: ") + mpg123_plain_strerror(rc));
    }
}

Mpg123Runtime::~Mpg123Runtime() {
    mpg123_exit();
}

Mp3Decoder::Mp3Decoder(std::string path) : input_(path, std::ios::binary) {
    if (!input_) {
        throw Error("unable to open MP3 input file: " + path);
    }

    int err = MPG123_OK;
    handle_ = mpg123_new(nullptr, &err);
    if (!handle_) {
        throw Error(std::string("mpg123_new failed: ") + mpg123_plain_strerror(err));
    }
    configure_formats();
    if (mpg123_open_feed(handle_) != MPG123_OK) {
        throw_decoder_error("open feed decoder");
    }
}

Mp3Decoder::~Mp3Decoder() {
    if (handle_) {
        (void)mpg123_close(handle_);
        mpg123_delete(handle_);
    }
}

PcmBlock Mp3Decoder::read() {
    for (;;) {
        if (!input_done_) {
            input_.read(reinterpret_cast<char*>(input_buffer_.data()),
                        static_cast<std::streamsize>(input_buffer_.size()));
            const auto bytes = input_.gcount();
            if (bytes > 0) {
                auto block = decode(input_buffer_.data(), static_cast<std::size_t>(bytes));
                if (!block.empty()) {
                    return block;
                }
                continue;
            }
            if (input_.bad()) {
                throw Error("read failed for MP3 input file");
            }
            input_done_ = true;
        }

        if (!decoder_drained_) {
            auto block = drain();
            if (!block.empty()) {
                return block;
            }
            decoder_drained_ = true;
        }

        return {};
    }
}

void Mp3Decoder::configure_formats() {
    if (mpg123_format_none(handle_) != MPG123_OK) {
        throw_decoder_error("clear output formats");
    }

    const long* rates = nullptr;
    std::size_t count = 0;
    mpg123_rates(&rates, &count);
    if (!rates || count == 0) {
        throw Error("mpg123 reported no supported sample rates");
    }

    for (std::size_t i = 0; i < count; ++i) {
        if (mpg123_format(handle_, rates[i], MPG123_MONO | MPG123_STEREO, MPG123_ENC_SIGNED_16) !=
            MPG123_OK) {
            throw_decoder_error("enable signed 16-bit output format");
        }
    }
}

PcmBlock Mp3Decoder::decode(const unsigned char* data, std::size_t length) {
    constexpr std::size_t max_samples = 8192;
    std::vector<std::int16_t> pcm(max_samples * 2);
    std::size_t done = 0;

    int rc =
        mpg123_decode(handle_, data, length, pcm.data(), pcm.size() * sizeof(std::int16_t), &done);
    if (rc == MPG123_NEW_FORMAT) {
        refresh_format();
    } else if (rc != MPG123_OK && rc != MPG123_NEED_MORE && rc != MPG123_DONE) {
        throw_decoder_error("decode MP3 data");
    }

    return convert_pcm(pcm, done);
}

PcmBlock Mp3Decoder::drain() {
    return decode(nullptr, 0);
}

void Mp3Decoder::refresh_format() {
    if (mpg123_getformat(handle_, &sample_rate_, &channels_, &encoding_) != MPG123_OK) {
        throw_decoder_error("read decoder output format");
    }
    if (encoding_ != MPG123_ENC_SIGNED_16) {
        throw Error("mpg123 selected an unsupported PCM encoding");
    }
    if (channels_ != 1 && channels_ != 2) {
        throw Error("mpg123 selected an unsupported channel count: " + std::to_string(channels_));
    }
}

PcmBlock Mp3Decoder::convert_pcm(const std::vector<std::int16_t>& interleaved,
                                 std::size_t bytes_done) const {
    if (bytes_done == 0) {
        return {};
    }
    if (channels_ != 1 && channels_ != 2) {
        throw Error("decoder produced PCM before reporting a usable format");
    }
    const std::size_t values = bytes_done / sizeof(std::int16_t);
    const std::size_t samples = values / static_cast<std::size_t>(channels_);
    if (samples == 0) {
        return {};
    }

    PcmBlock block;
    block.sample_rate = sample_rate_;
    block.channels = channels_;
    block.left.reserve(samples);
    block.right.reserve(samples);

    for (std::size_t i = 0; i < samples; ++i) {
        if (channels_ == 1) {
            const auto sample = interleaved[i];
            block.left.push_back(sample);
            block.right.push_back(sample);
        } else {
            block.left.push_back(interleaved[i * 2]);
            block.right.push_back(interleaved[i * 2 + 1]);
        }
    }
    return block;
}

void Mp3Decoder::throw_decoder_error(const char* action) const {
    throw Error(std::string("mpg123 failed to ") + action + ": " + mpg123_strerror(handle_));
}

} // namespace modern_ices
