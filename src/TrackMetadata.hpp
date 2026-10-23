#pragma once

#include <string>

namespace modern_ices {

struct TrackMetadata {
    std::string artist;
    std::string title;
    std::string fallback;

    std::string stream_title() const;
};

TrackMetadata read_track_metadata(const std::string& path);

} // namespace modern_ices
