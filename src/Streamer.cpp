#include "Streamer.hpp"

#include "AudioDecoder.hpp"
#include "Error.hpp"
#include "IcecastClient.hpp"
#include "LameEncoder.hpp"
#include "Logger.hpp"
#include "Mp3Decoder.hpp"
#include "Playlist.hpp"

#include <array>
#include <fstream>
#include <memory>

namespace modern_ices {
namespace {

std::ifstream open_binary(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw Error("unable to open input file: " + path);
    }
    return input;
}

} // namespace

Streamer::Streamer(Config config) : config_(std::move(config)) {
}

void Streamer::run() {
    ShoutRuntime shout_runtime;
    std::unique_ptr<Mpg123Runtime> mpg123_runtime;
    if (config_.reencode) {
        mpg123_runtime = std::make_unique<Mpg123Runtime>();
    }

    Playlist playlist(config_.playlist_path);
    Logger::info("Loaded " + std::to_string(playlist.size()) + " playlist entries");

    std::size_t successes = 0;
    std::size_t attempts = 0;
    for (;;) {
        if (config_.once && attempts >= playlist.size()) {
            if (successes == 0) {
                throw Error("no playlist entries streamed successfully");
            }
            return;
        }

        const auto path = playlist.next();
        ++attempts;
        try {
            stream_file(path);
            ++successes;
        } catch (const std::exception& e) {
            Logger::error("skipping " + path + ": " + e.what());
        }
    }
}

void Streamer::stream_file(const std::string& path) {
    Logger::info("Playing " + path);
    if (config_.reencode) {
        stream_transcoded(path);
    } else {
        stream_passthrough(path);
    }
}

void Streamer::stream_passthrough(const std::string& path) {
    auto input = open_binary(path);
    IcecastClient client(config_);
    client.connect();

    std::array<unsigned char, 16384> buffer{};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(buffer.size()));
        const auto bytes = input.gcount();
        if (bytes > 0) {
            client.send(buffer.data(), static_cast<std::size_t>(bytes));
            client.sync();
        }
    }
    if (input.bad()) {
        throw Error("read failed for input file: " + path);
    }
}

void Streamer::stream_transcoded(const std::string& path) {
    IcecastClient client(config_);
    client.connect();

    auto decoder = open_audio_decoder(path);
    std::unique_ptr<LameEncoder> encoder;
    long encoded_sample_rate = 0;
    int encoded_channels = 0;

    auto send_pcm = [&](const PcmBlock& pcm) {
        if (pcm.empty()) {
            return;
        }
        if (!encoder) {
            encoder =
                std::make_unique<LameEncoder>(pcm.sample_rate, pcm.channels, config_.bitrate_kbps);
            encoded_sample_rate = pcm.sample_rate;
            encoded_channels = pcm.channels;
        } else if (pcm.sample_rate != encoded_sample_rate || pcm.channels != encoded_channels) {
            throw Error("input stream changed audio format during transcoding");
        }

        auto encoded = encoder->encode(pcm);
        if (!encoded.empty()) {
            client.send(encoded.data(), encoded.size());
            client.sync();
        }
    };

    for (;;) {
        auto pcm = decoder->read();
        if (pcm.empty()) {
            break;
        }
        send_pcm(pcm);
    }

    if (encoder) {
        auto tail = encoder->flush();
        if (!tail.empty()) {
            client.send(tail.data(), tail.size());
            client.sync();
        }
    }
}

} // namespace modern_ices
