#include "Config.hpp"

#include "Error.hpp"

#include <charconv>
#include <iostream>
#include <limits>
#include <string_view>

namespace modern_ices {
namespace {

std::string require_value(int& index, int argc, char** argv, std::string_view option) {
    if (index + 1 >= argc) {
        throw Error("missing value for " + std::string(option));
    }
    return argv[++index];
}

std::string optional_value(int& index, int argc, char** argv, std::string_view default_value) {
    if (index + 1 >= argc) {
        return std::string(default_value);
    }

    std::string_view next{argv[index + 1]};
    if (!next.empty() && next.front() == '-') {
        return std::string(default_value);
    }

    ++index;
    return std::string(next);
}

int parse_int(const std::string& value, std::string_view option, int min, int max) {
    int result{0};
    const char* begin{value.data()};
    const char* end{value.data() + value.size()};
    auto [ptr, ec] = std::from_chars(begin, end, result);
    if (ec != std::errc{} || ptr != end || result < min || result > max) {
        throw Error("invalid numeric value for " + std::string(option) + ": " + value);
    }
    return result;
}

Protocol parse_protocol(const std::string& value) {
    if (value == "http") {
        return Protocol::Http;
    }
    if (value == "icy") {
        return Protocol::Icy;
    }
    throw Error("unsupported protocol: " + value);
}

} // namespace

void Config::print_usage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options]\n\n"
        << "Options:\n"
        << "  -F, --playlist FILE      playlist file [playlist.txt]\n"
        << "  -h, --host HOST          stream server host [127.0.0.1]\n"
        << "  -p, --port PORT          stream server port [8000]\n"
        << "  -P, --password PASS      source password [hackme]\n"
        << "  -m, --mount MOUNT        mount point [/ices]\n"
        << "  -u, --url URL            stream URL metadata\n"
        << "  -n, --name NAME          stream name metadata\n"
        << "  -g, --genre GENRE        stream genre metadata\n"
        << "  -d, --description TEXT   stream description metadata\n"
        << "  -t, --protocol http|icy  stream protocol [http]\n"
        << "  -b, --bitrate KBPS       output bitrate for transcoding [128]\n"
        << "  -x, --crossfade SECONDS  crossfade length; requires --reencode [0]\n"
        << "  -R, --reencode           decode MP3, Ogg Vorbis, or FLAC and re-encode to bitrate\n"
        << "  -r, --randomize          randomize playlist order at startup\n"
        << "      --once               stream each playlist entry once, then exit\n"
        << "  -s[FORMAT], --timestamp[=FORMAT]\n"
        << "                           prepend strftime timestamps to log messages [%c]\n"
        << "  -v, --verbose            verbose logging\n"
        << "      --help               show this help\n";
}

Config Config::parse(int argc, char** argv) {
    Config config{};
    for (int i{1}; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "-F" || arg == "--playlist") {
            config.playlist_path = require_value(i, argc, argv, arg);
        } else if (arg == "-h" || arg == "--host") {
            config.host = require_value(i, argc, argv, arg);
        } else if (arg == "-p" || arg == "--port") {
            auto value{require_value(i, argc, argv, arg)};
            config.port = static_cast<std::uint16_t>(parse_int(value, arg, 1, 65535));
        } else if (arg == "-P" || arg == "--password") {
            config.password = require_value(i, argc, argv, arg);
        } else if (arg == "-m" || arg == "--mount") {
            config.mount = require_value(i, argc, argv, arg);
            if (config.mount.empty() || config.mount.front() != '/') {
                throw Error("mount point must start with '/'");
            }
        } else if (arg == "-u" || arg == "--url") {
            config.url = require_value(i, argc, argv, arg);
        } else if (arg == "-n" || arg == "--name") {
            config.name = require_value(i, argc, argv, arg);
        } else if (arg == "-g" || arg == "--genre") {
            config.genre = require_value(i, argc, argv, arg);
        } else if (arg == "-d" || arg == "--description") {
            config.description = require_value(i, argc, argv, arg);
        } else if (arg == "-t" || arg == "--protocol") {
            config.protocol = parse_protocol(require_value(i, argc, argv, arg));
        } else if (arg == "-b" || arg == "--bitrate") {
            config.bitrate_kbps = parse_int(require_value(i, argc, argv, arg), arg, 8, 512);
        } else if (arg == "-x" || arg == "--crossfade") {
            config.crossfade_seconds = parse_int(require_value(i, argc, argv, arg), arg, 0, 3600);
        } else if (arg == "-R" || arg == "--reencode") {
            config.reencode = true;
        } else if (arg == "-r" || arg == "--randomize") {
            config.randomize_playlist = true;
        } else if (arg == "--once") {
            config.once = true;
        } else if (arg == "-s" || arg == "--timestamp") {
            config.log_timestamps = true;
            config.log_timestamp_format = optional_value(i, argc, argv, "%c");
        } else if (arg.starts_with("-s") && arg.size() > 2) {
            config.log_timestamps = true;
            config.log_timestamp_format = std::string(arg.substr(2));
        } else if (arg.starts_with("--timestamp=")) {
            config.log_timestamps = true;
            config.log_timestamp_format = std::string(arg.substr(12));
        } else if (arg == "--timestamp-format") {
            config.log_timestamps = true;
            config.log_timestamp_format = require_value(i, argc, argv, arg);
        } else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        } else if (arg == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            throw Error("unknown option: " + std::string(arg));
        }
    }
    return config;
}

} // namespace modern_ices
