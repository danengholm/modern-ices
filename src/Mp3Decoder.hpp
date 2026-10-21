#pragma once

#include "AudioDecoder.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <mpg123.h>
#include <string>
#include <vector>

namespace modern_ices {

class Mpg123Runtime {
  public:
    Mpg123Runtime();
    ~Mpg123Runtime();

    Mpg123Runtime(const Mpg123Runtime&) = delete;
    Mpg123Runtime& operator=(const Mpg123Runtime&) = delete;
};

class Mp3Decoder final : public PcmDecoder {
  public:
    explicit Mp3Decoder(std::string path);
    ~Mp3Decoder() override;

    Mp3Decoder(const Mp3Decoder&) = delete;
    Mp3Decoder& operator=(const Mp3Decoder&) = delete;

    PcmBlock read() override;

  private:
    std::ifstream input_;
    std::array<unsigned char, 8192> input_buffer_{};
    mpg123_handle* handle_{nullptr};
    long sample_rate_{0};
    int channels_{0};
    int encoding_{0};
    bool input_done_{false};
    bool decoder_drained_{false};

    PcmBlock decode(const unsigned char* data, std::size_t length);
    PcmBlock drain();
    void configure_formats();
    void refresh_format();
    PcmBlock convert_pcm(const std::vector<std::int16_t>& interleaved,
                         std::size_t bytes_done) const;
    [[noreturn]] void throw_decoder_error(const char* action) const;
};

} // namespace modern_ices
