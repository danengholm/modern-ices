#pragma once

#include <cstdint>
#include <string>

namespace modern_ices {

enum class Protocol { Http, Icy };

// Runtime settings parsed from the command line. Defaults mirror a local Icecast setup.
struct Config {
    std::string playlist_path{"playlist.txt"};
    std::string host{"127.0.0.1"};
    std::uint16_t port{8000};
    std::string password{"hackme"};
    std::string mount{"/ices"};
    std::string user{"source"};
    std::string name{"modern-ices"};
    std::string genre{"misc"};
    std::string description{"modern-ices stream"};
    std::string url{"https://www.icecast.org/"};
    Protocol protocol{Protocol::Http};
    int bitrate_kbps{128};
    int crossfade_seconds{0};
    bool reencode{false};
    bool once{false};
    bool verbose{false};

    static Config parse(int argc, char** argv);
    static void print_usage(const char* argv0);
};

} // namespace modern_ices
