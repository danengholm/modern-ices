#include "AudioDecoder.hpp"

#include "Error.hpp"
#include "FlacDecoder.hpp"
#include "Mp3Decoder.hpp"
#include "VorbisDecoder.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace modern_ices {
namespace {

// Only formats that can be transcoded to MP3 are modeled here. Passthrough mode does not use this
// path.
enum class AudioKind { Mp3, OggVorbis, Flac };

std::string lowercase_extension(const std::string& path) {
    const auto slash{path.find_last_of("/\\")};
    const auto dot{path.find_last_of('.')};
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return {};
    }

    std::string extension{path.substr(dot)};
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return extension;
}

AudioKind detect_audio_kind(const std::string& path) {
    const auto extension{lowercase_extension(path)};
    if (extension == ".flac") {
        return AudioKind::Flac;
    }
    if (extension == ".ogg" || extension == ".oga") {
        return AudioKind::OggVorbis;
    }
    // Unknown input falls back to MP3 so mpg123 can report a detailed decoder error.
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
