#include "AudioDecoder.hpp"

#include "Error.hpp"
#include "FlacDecoder.hpp"
#include "Mp3Decoder.hpp"
#include "VorbisDecoder.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <string>

namespace modern_ices {
namespace {

enum class AudioKind { Mp3, OggVorbis, Flac };

std::string lowercase_extension(const std::string& path) {
  const auto slash = path.find_last_of("/\\");
  const auto dot = path.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
    return {};
  }

  std::string extension = path.substr(dot);
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return extension;
}

AudioKind detect_audio_kind(const std::string& path) {
  std::array<unsigned char, 12> header{};
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw Error("unable to open input file: " + path);
  }
  input.read(reinterpret_cast<char*>(header.data()), static_cast<std::streamsize>(header.size()));
  const auto bytes = input.gcount();
  if (input.bad()) {
    throw Error("unable to read input file header: " + path);
  }

  if (bytes >= 4 && header[0] == 'f' && header[1] == 'L' && header[2] == 'a' && header[3] == 'C') {
    return AudioKind::Flac;
  }
  if (bytes >= 4 && header[0] == 'O' && header[1] == 'g' && header[2] == 'g' && header[3] == 'S') {
    return AudioKind::OggVorbis;
  }
  if (bytes >= 3 && header[0] == 'I' && header[1] == 'D' && header[2] == '3') {
    return AudioKind::Mp3;
  }
  if (bytes >= 2 && header[0] == 0xff && (header[1] & 0xe0) == 0xe0) {
    return AudioKind::Mp3;
  }

  const auto extension = lowercase_extension(path);
  if (extension == ".flac") {
    return AudioKind::Flac;
  }
  if (extension == ".ogg" || extension == ".oga") {
    return AudioKind::OggVorbis;
  }
  return AudioKind::Mp3;
}

} // namespace

std::unique_ptr<PcmDecoder> open_audio_decoder(const std::string& path) {
  switch (detect_audio_kind(path)) {
  case AudioKind::Flac:
    return std::make_unique<FlacDecoder>(path);
  case AudioKind::OggVorbis:
    return std::make_unique<VorbisDecoder>(path);
  case AudioKind::Mp3:
    return std::make_unique<Mp3Decoder>(path);
  }
  throw Error("unsupported audio input type: " + path);
}

} // namespace modern_ices
