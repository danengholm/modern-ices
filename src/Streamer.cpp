#include "Streamer.hpp"

#include "AudioDecoder.hpp"
#include "Crossfader.hpp"
#include "Error.hpp"
#include "IcecastClient.hpp"
#include "LameEncoder.hpp"
#include "Logger.hpp"
#include "Mp3Decoder.hpp"
#include "Playlist.hpp"
#include "TrackMetadata.hpp"

#include <array>
#include <cstddef>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace modern_ices {
namespace {

std::ifstream open_binary(const std::string& path) {
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        throw Error("unable to open input file: " + path);
    }
    return input;
}

void update_stream_title(IcecastClient& client, const TrackMetadata& metadata,
                         std::string& current_title) {
    const auto title{metadata.stream_title()};
    if (title.empty() || title == current_title) {
        return;
    }
    client.set_stream_title(title);
    current_title = title;
    Logger::debug("Updated StreamTitle to: " + title);
}

} // namespace

Streamer::Streamer(Config config) : config_{std::move(config)} {
}

// In continuous mode this loops forever; --once makes a single pass over the playlist.
void Streamer::run() {
    ShoutRuntime shout_runtime{};
    std::unique_ptr<Mpg123Runtime> mpg123_runtime{};
    if (config_.reencode) {
        mpg123_runtime = std::make_unique<Mpg123Runtime>();
    }
    if (config_.crossfade_seconds > 0 && !config_.reencode) {
        throw Error("crossfade requires --reencode");
    }

    Playlist playlist(config_.playlist_path);
    Logger::info("Loaded " + std::to_string(playlist.size()) + " playlist entries");
    if (config_.crossfade_seconds > 0) {
        stream_crossfaded(playlist);
        return;
    }

    std::size_t successes{0};
    std::size_t attempts{0};
    for (;;) {
        if (config_.once && attempts >= playlist.size()) {
            if (successes == 0) {
                throw Error("no playlist entries streamed successfully");
            }
            return;
        }

        const auto path{playlist.next()};
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

// Passthrough sends file bytes unchanged, so it is appropriate only for MP3 source files.
void Streamer::stream_passthrough(const std::string& path) {
    auto input{open_binary(path)};
    IcecastClient client(config_);
    client.connect();
    TrackMetadata metadata{};
    metadata.fallback = metadata_fallback_from_path(path);
    std::vector<unsigned char> metadata_probe;
    std::string current_title;
    constexpr std::size_t max_metadata_probe_bytes{65536};

    std::array<unsigned char, 16384> buffer{};
    while (input) {
        input.read(reinterpret_cast<char*>(buffer.data()),
                   static_cast<std::streamsize>(buffer.size()));
        const auto bytes{input.gcount()};
        if (bytes > 0) {
            if (metadata.artist.empty() || metadata.title.empty()) {
                const auto probe_room{max_metadata_probe_bytes -
                                      std::min(metadata_probe.size(), max_metadata_probe_bytes)};
                const auto probe_bytes{std::min(static_cast<std::size_t>(bytes), probe_room)};
                metadata_probe.insert(metadata_probe.end(), buffer.data(),
                                      buffer.data() + probe_bytes);
                auto parsed{read_mp3_metadata(metadata_probe)};
                if (!parsed.artist.empty()) {
                    metadata.artist = std::move(parsed.artist);
                }
                if (!parsed.title.empty()) {
                    metadata.title = std::move(parsed.title);
                }
            }
            update_stream_title(client, metadata, current_title);
            client.send(buffer.data(), static_cast<std::size_t>(bytes));
            client.sync();
        }
    }
    if (input.bad()) {
        throw Error("read failed for input file: " + path);
    }
}

// Transcoding opens a decoder based on input format, then lazily creates LAME after PCM format is
// known.
void Streamer::stream_transcoded(const std::string& path) {
    auto decoder{open_audio_decoder(path)};
    IcecastClient client(config_);
    client.connect();
    std::string current_title;
    update_stream_title(client, decoder->metadata(), current_title);

    std::unique_ptr<LameEncoder> encoder{};
    long encoded_sample_rate{0};
    int encoded_channels{0};

    // A single MP3 stream cannot change sample rate or channel count mid-file.
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

        auto encoded{encoder->encode(pcm)};
        if (!encoded.empty()) {
            client.send(encoded.data(), encoded.size());
            client.sync();
        }
    };

    for (;;) {
        auto pcm{decoder->read()};
        update_stream_title(client, decoder->metadata(), current_title);
        if (pcm.empty()) {
            break;
        }
        send_pcm(pcm);
    }

    if (encoder) {
        auto tail{encoder->flush()};
        if (!tail.empty()) {
            client.send(tail.data(), tail.size());
            client.sync();
        }
    }
}

void Streamer::stream_crossfaded(Playlist& playlist) {
    IcecastClient client(config_);
    client.connect();

    Crossfader crossfader(config_.crossfade_seconds);
    std::unique_ptr<LameEncoder> encoder{};
    long encoded_sample_rate{0};
    int encoded_channels{0};

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
            throw Error("input stream changed audio format during crossfade transcoding");
        }

        auto encoded{encoder->encode(pcm)};
        if (!encoded.empty()) {
            client.send(encoded.data(), encoded.size());
            client.sync();
        }
    };

    auto send_blocks = [&](std::vector<PcmBlock> blocks) {
        for (const auto& block : blocks) {
            send_pcm(block);
        }
    };

    std::size_t successes{0};
    std::size_t attempts{0};
    for (;;) {
        if (config_.once && attempts >= playlist.size()) {
            if (successes == 0) {
                throw Error("no playlist entries streamed successfully");
            }
            send_pcm(crossfader.finish());
            break;
        }

        const auto path{playlist.next()};
        ++attempts;
        try {
            Logger::info("Playing " + path);
            auto decoder{open_audio_decoder(path)};
            std::string current_title;
            update_stream_title(client, decoder->metadata(), current_title);
            bool started{false};
            for (;;) {
                auto pcm{decoder->read()};
                update_stream_title(client, decoder->metadata(), current_title);
                if (pcm.empty()) {
                    break;
                }
                if (!started) {
                    send_pcm(crossfader.start_track(pcm));
                    started = true;
                }
                send_blocks(crossfader.process(pcm));
            }
            if (!started) {
                throw Error("decoded no audio samples");
            }
            ++successes;
        } catch (const std::exception& e) {
            Logger::error("skipping " + path + ": " + e.what());
        }
    }

    if (encoder) {
        auto tail{encoder->flush()};
        if (!tail.empty()) {
            client.send(tail.data(), tail.size());
            client.sync();
        }
    }
}

} // namespace modern_ices
