#include "Streamer.hpp"

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

Streamer::Streamer(Config config) : config_(std::move(config)) {}

void Streamer::run() {
  ShoutRuntime shout_runtime;
  std::unique_ptr<Mpg123Runtime> mpg123_runtime;
  if (config_.reencode) {
    mpg123_runtime = std::make_unique<Mpg123Runtime>();
  }

  Playlist playlist(config_.playlist_path);
  Logger::info("Loaded " + std::to_string(playlist.size()) + " playlist entries");

  std::size_t played = 0;
  do {
    const auto path = playlist.next();
    try {
      stream_file(path);
      ++played;
    } catch (const std::exception& e) {
      Logger::error("skipping " + path + ": " + e.what());
      if (config_.once && played + 1 >= playlist.size()) {
        throw;
      }
    }
  } while (!config_.once);
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
    input.read(reinterpret_cast<char*>(buffer.data()), static_cast<std::streamsize>(buffer.size()));
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
  auto input = open_binary(path);
  IcecastClient client(config_);
  client.connect();

  Mp3Decoder decoder;
  std::unique_ptr<LameEncoder> encoder;
  std::array<unsigned char, 8192> input_buffer{};

  auto send_pcm = [&](const PcmBlock& pcm) {
    if (pcm.empty()) {
      return;
    }
    if (!encoder) {
      encoder = std::make_unique<LameEncoder>(pcm.sample_rate, pcm.channels, config_.bitrate_kbps);
    }
    auto encoded = encoder->encode(pcm);
    if (!encoded.empty()) {
      client.send(encoded.data(), encoded.size());
      client.sync();
    }
  };

  while (input) {
    input.read(reinterpret_cast<char*>(input_buffer.data()),
               static_cast<std::streamsize>(input_buffer.size()));
    const auto bytes = input.gcount();
    if (bytes > 0) {
      send_pcm(decoder.decode(input_buffer.data(), static_cast<std::size_t>(bytes)));
    }
  }
  if (input.bad()) {
    throw Error("read failed for input file: " + path);
  }

  for (;;) {
    auto pcm = decoder.drain();
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
