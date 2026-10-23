#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace modern_ices {

struct TrackMetadata {
    std::string artist;
    std::string title;
    std::string fallback;

    std::string stream_title() const;
};

std::string metadata_fallback_from_path(const std::string& path);
void apply_vorbis_comment(TrackMetadata& metadata, std::string_view comment);
TrackMetadata read_mp3_metadata(const std::vector<unsigned char>& data);

} // namespace modern_ices
