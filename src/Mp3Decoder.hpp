#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include <mpg123.h>

namespace modern_ices {

class Mpg123Runtime {
public:
  Mpg123Runtime();
  ~Mpg123Runtime();

  Mpg123Runtime(const Mpg123Runtime&) = delete;
  Mpg123Runtime& operator=(const Mpg123Runtime&) = delete;
};

struct PcmBlock {
  std::vector<std::int16_t> left;
  std::vector<std::int16_t> right;
  long sample_rate = 0;
  int channels = 0;

  bool empty() const { return left.empty(); }
  int samples() const { return static_cast<int>(left.size()); }
};

class Mp3Decoder {
public:
  Mp3Decoder();
  ~Mp3Decoder();

  Mp3Decoder(const Mp3Decoder&) = delete;
  Mp3Decoder& operator=(const Mp3Decoder&) = delete;

  PcmBlock decode(const unsigned char* data, std::size_t length);
  PcmBlock drain();

private:
  mpg123_handle* handle_ = nullptr;
  long sample_rate_ = 0;
  int channels_ = 0;
  int encoding_ = 0;

  void configure_formats();
  void refresh_format();
  PcmBlock convert_pcm(const std::vector<std::int16_t>& interleaved, std::size_t bytes_done) const;
  [[noreturn]] void throw_decoder_error(const char* action) const;
};

} // namespace modern_ices
