#include "TrackMetadata.hpp"

#include "Error.hpp"

#include <FLAC/metadata.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif
#include <vorbis/vorbisfile.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace modern_ices {
namespace {

enum class AudioKind { Mp3, OggVorbis, Flac };

std::string trim(std::string value) {
    if (const auto nul{value.find('\0')}; nul != std::string::npos) {
        value.erase(nul);
    }
    const auto not_space{[](unsigned char c) { return !std::isspace(c); }};
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

std::string clean_filename(const std::string& path) {
    const auto slash{path.find_last_of("/\\")};
    const auto start{slash == std::string::npos ? 0 : slash + 1};
    auto name{path.substr(start)};
    const auto dot{name.find_last_of('.')};
    if (dot != std::string::npos) {
        name.erase(dot);
    }
    return name.empty() ? path : name;
}

std::string lowercase_extension(const std::string& path) {
    const auto slash{path.find_last_of("/\\")};
    const auto dot{path.find_last_of('.')};
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return {};
    }
    return lowercase(path.substr(dot));
}

std::vector<unsigned char> read_file(const std::string& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw Error("unable to open input file for metadata: " + path);
    }
    input.seekg(0, std::ios::end);
    const auto end{input.tellg()};
    if (end < 0) {
        throw Error("unable to size input file for metadata: " + path);
    }
    input.seekg(0, std::ios::beg);
    std::vector<unsigned char> data(static_cast<std::size_t>(end));
    if (!data.empty()) {
        input.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
        if (!input) {
            throw Error("unable to read input file for metadata: " + path);
        }
    }
    return data;
}

AudioKind detect_audio_kind(const std::string& path, const std::vector<unsigned char>& data) {
    if (data.size() >= 4 && data[0] == 'f' && data[1] == 'L' && data[2] == 'a' && data[3] == 'C') {
        return AudioKind::Flac;
    }
    if (data.size() >= 4 && data[0] == 'O' && data[1] == 'g' && data[2] == 'g' && data[3] == 'S') {
        return AudioKind::OggVorbis;
    }
    if (data.size() >= 3 && data[0] == 'I' && data[1] == 'D' && data[2] == '3') {
        return AudioKind::Mp3;
    }
    if (data.size() >= 2 && data[0] == 0xff && (data[1] & 0xe0) == 0xe0) {
        return AudioKind::Mp3;
    }

    const auto extension{lowercase_extension(path)};
    if (extension == ".flac") {
        return AudioKind::Flac;
    }
    if (extension == ".ogg" || extension == ".oga") {
        return AudioKind::OggVorbis;
    }
    return AudioKind::Mp3;
}

bool starts_with_ci(std::string_view value, std::string_view prefix) {
    if (value.size() < prefix.size()) {
        return false;
    }
    for (std::size_t i{0}; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(value[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) {
            return false;
        }
    }
    return true;
}

void apply_vorbis_comment(TrackMetadata& metadata, std::string_view comment) {
    // Match complete Vorbis comment keys, including the '='. This preserves the pkgsrc fixes that
    // avoid treating keys such as ARTISTSORT as ARTIST.
    if (starts_with_ci(comment, "artist=")) {
        metadata.artist = trim(std::string(comment.substr(7)));
    } else if (starts_with_ci(comment, "title=")) {
        metadata.title = trim(std::string(comment.substr(6)));
    }
}

std::uint32_t synchsafe32(const unsigned char* data) {
    return (static_cast<std::uint32_t>(data[0]) << 21) |
           (static_cast<std::uint32_t>(data[1]) << 14) |
           (static_cast<std::uint32_t>(data[2]) << 7) | static_cast<std::uint32_t>(data[3]);
}

std::uint32_t synchsafe24(const unsigned char* data) {
    return (static_cast<std::uint32_t>(data[0]) << 14) |
           (static_cast<std::uint32_t>(data[1]) << 7) | static_cast<std::uint32_t>(data[2]);
}

std::uint32_t unsafe32(const unsigned char* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24) |
           (static_cast<std::uint32_t>(data[1]) << 16) |
           (static_cast<std::uint32_t>(data[2]) << 8) | static_cast<std::uint32_t>(data[3]);
}

std::string decode_id3_text(const unsigned char* data, std::size_t size) {
    if (size <= 1) {
        return {};
    }
    const auto encoding{data[0]};
    const auto* text{data + 1};
    const auto text_size{size - 1};
    if (encoding == 0 || encoding == 3) {
        return trim(std::string(reinterpret_cast<const char*>(text), text_size));
    }

    std::string result;
    result.reserve(text_size / 2);
    std::size_t offset{0};
    if (text_size >= 2 &&
        ((text[0] == 0xff && text[1] == 0xfe) || (text[0] == 0xfe && text[1] == 0xff))) {
        offset = 2;
    }
    for (std::size_t i{offset}; i + 1 < text_size; i += 2) {
        const auto lo{text[i]};
        const auto hi{text[i + 1]};
        if (lo == 0 && hi == 0) {
            break;
        }
        if (hi == 0 && lo < 0x80) {
            result.push_back(static_cast<char>(lo));
        } else if (lo == 0 && hi < 0x80) {
            result.push_back(static_cast<char>(hi));
        }
    }
    return trim(result);
}

void read_id3v1(TrackMetadata& metadata, const std::vector<unsigned char>& data) {
    if (data.size() < 128) {
        return;
    }
    const auto offset{data.size() - 128};
    if (std::string_view(reinterpret_cast<const char*>(data.data() + offset), 3) != "TAG") {
        return;
    }
    if (metadata.title.empty()) {
        metadata.title =
            trim(std::string(reinterpret_cast<const char*>(data.data() + offset + 3), 30));
    }
    if (metadata.artist.empty()) {
        metadata.artist =
            trim(std::string(reinterpret_cast<const char*>(data.data() + offset + 33), 30));
    }
}

void read_id3v2(TrackMetadata& metadata, const std::vector<unsigned char>& data) {
    if (data.size() < 10 ||
        std::string_view(reinterpret_cast<const char*>(data.data()), 3) != "ID3") {
        return;
    }

    const auto version{data[3]};
    if (version < 2 || version > 4) {
        return;
    }
    const auto tag_size{synchsafe32(data.data() + 6)};
    const auto tag_end{std::min<std::size_t>(data.size(), 10 + tag_size)};
    std::size_t pos{10};

    if (version > 2 && (data[5] & (1 << 6)) != 0 && pos + 4 <= tag_end) {
        const auto extended_size{version == 3 ? unsafe32(data.data() + pos)
                                              : synchsafe32(data.data() + pos)};
        pos += std::min<std::size_t>(extended_size, tag_end - pos);
    }

    while (pos < tag_end && (metadata.artist.empty() || metadata.title.empty())) {
        const auto header_size{version == 2 ? 6U : 10U};
        if (tag_end - pos < header_size || data[pos] == 0) {
            break;
        }

        std::string frame_id;
        std::uint32_t frame_size{0};
        if (version == 2) {
            frame_id.assign(reinterpret_cast<const char*>(data.data() + pos), 3);
            frame_size = synchsafe24(data.data() + pos + 3);
        } else {
            frame_id.assign(reinterpret_cast<const char*>(data.data() + pos), 4);
            frame_size =
                version == 3 ? unsafe32(data.data() + pos + 4) : synchsafe32(data.data() + pos + 4);
        }

        pos += header_size;
        if (frame_size == 0 || frame_size > tag_end - pos) {
            break;
        }

        if ((frame_id == "TPE1" || frame_id == "TP1") && metadata.artist.empty()) {
            metadata.artist = decode_id3_text(data.data() + pos, frame_size);
        } else if ((frame_id == "TIT2" || frame_id == "TT2") && metadata.title.empty()) {
            metadata.title = decode_id3_text(data.data() + pos, frame_size);
        }
        pos += frame_size;
    }
}

TrackMetadata read_mp3_metadata(const std::vector<unsigned char>& data) {
    TrackMetadata metadata;
    read_id3v1(metadata, data);
    read_id3v2(metadata, data);
    return metadata;
}

TrackMetadata read_ogg_vorbis_metadata(const std::string& path) {
    TrackMetadata metadata;
    OggVorbis_File file{};
    const auto rc{ov_fopen(path.c_str(), &file)};
    if (rc != 0) {
        return metadata;
    }
    const auto guard{std::unique_ptr<OggVorbis_File, decltype(&ov_clear)>(&file, &ov_clear)};
    const auto* comments{ov_comment(&file, -1)};
    if (!comments) {
        return metadata;
    }
    for (int i{0}; i < comments->comments; ++i) {
        apply_vorbis_comment(metadata, comments->user_comments[i]);
    }
    return metadata;
}

TrackMetadata read_flac_metadata(const std::string& path) {
    TrackMetadata metadata;
    FLAC__StreamMetadata* comments{nullptr};
    if (!FLAC__metadata_get_tags(path.c_str(), &comments) || !comments) {
        return metadata;
    }
    const auto guard{std::unique_ptr<FLAC__StreamMetadata, decltype(&FLAC__metadata_object_delete)>(
        comments, &FLAC__metadata_object_delete)};
    const auto& vorbis_comments{comments->data.vorbis_comment};
    for (FLAC__uint32 i{0}; i < vorbis_comments.num_comments; ++i) {
        const auto& entry{vorbis_comments.comments[i]};
        apply_vorbis_comment(
            metadata, std::string_view(reinterpret_cast<const char*>(entry.entry), entry.length));
    }
    return metadata;
}

} // namespace

std::string TrackMetadata::stream_title() const {
    if (!title.empty()) {
        if (!artist.empty()) {
            return artist + " - " + title;
        }
        return title;
    }
    return fallback;
}

TrackMetadata read_track_metadata(const std::string& path) {
    const auto data{read_file(path)};
    auto metadata{[&]() {
        switch (detect_audio_kind(path, data)) {
        case AudioKind::Flac:
            return read_flac_metadata(path);
        case AudioKind::OggVorbis:
            return read_ogg_vorbis_metadata(path);
        case AudioKind::Mp3:
            return read_mp3_metadata(data);
        }
        return TrackMetadata{};
    }()};
    metadata.fallback = clean_filename(path);
    return metadata;
}

} // namespace modern_ices
