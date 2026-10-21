#include "FlacDecoder.hpp"

#include "Error.hpp"

#include <algorithm>
#include <cstdint>
#include <string>

namespace modern_ices {
namespace {

std::int16_t flac_to_i16(FLAC__int32 sample, unsigned bits_per_sample) {
  std::int64_t value = sample;
  if (bits_per_sample > 16) {
    value >>= (bits_per_sample - 16);
  } else if (bits_per_sample < 16) {
    value <<= (16 - bits_per_sample);
  }
  value = std::clamp<std::int64_t>(value, -32768, 32767);
  return static_cast<std::int16_t>(value);
}

std::string decoder_state_name(FLAC__StreamDecoderState state) {
  return FLAC__StreamDecoderStateString[state];
}

} // namespace

FlacDecoder::FlacDecoder(const std::string& path) {
  decoder_ = FLAC__stream_decoder_new();
  if (!decoder_) {
    throw Error("unable to allocate FLAC decoder");
  }

  const auto init = FLAC__stream_decoder_init_file(decoder_, path.c_str(), write_callback,
                                                   metadata_callback, error_callback, this);
  if (init != FLAC__STREAM_DECODER_INIT_STATUS_OK) {
    const std::string message = FLAC__StreamDecoderInitStatusString[init];
    FLAC__stream_decoder_delete(decoder_);
    decoder_ = nullptr;
    throw Error("unable to initialize FLAC decoder for " + path + ": " + message);
  }

  if (!FLAC__stream_decoder_process_until_end_of_metadata(decoder_)) {
    check_callback_error();
    throw Error("unable to read FLAC metadata for " + path + ": " +
                decoder_state_name(FLAC__stream_decoder_get_state(decoder_)));
  }
  check_callback_error();
}

FlacDecoder::~FlacDecoder() {
  if (decoder_) {
    FLAC__stream_decoder_finish(decoder_);
    FLAC__stream_decoder_delete(decoder_);
  }
}

PcmBlock FlacDecoder::read() {
  if (!pending_.empty()) {
    return take_pending();
  }

  for (;;) {
    check_callback_error();
    const auto state = FLAC__stream_decoder_get_state(decoder_);
    if (state == FLAC__STREAM_DECODER_END_OF_STREAM) {
      return {};
    }

    if (!FLAC__stream_decoder_process_single(decoder_)) {
      check_callback_error();
      const auto failed_state = FLAC__stream_decoder_get_state(decoder_);
      if (failed_state == FLAC__STREAM_DECODER_END_OF_STREAM) {
        return {};
      }
      if (!decoder_error_.empty()) {
        throw Error("FLAC decode failed: " + decoder_error_);
      }
      throw Error("FLAC decode failed: " + decoder_state_name(failed_state));
    }

    if (!pending_.empty()) {
      return take_pending();
    }
  }
}

PcmBlock FlacDecoder::take_pending() {
  PcmBlock block = std::move(pending_);
  pending_ = {};
  return block;
}

void FlacDecoder::set_callback_error(std::string message) {
  if (callback_error_.empty()) {
    callback_error_ = std::move(message);
  }
}

FLAC__StreamDecoderWriteStatus FlacDecoder::handle_write(const FLAC__Frame* frame,
                                                          const FLAC__int32* const buffer[]) {
  if (!frame || !buffer) {
    set_callback_error("FLAC decoder provided an empty frame");
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  const auto channels = frame->header.channels;
  const auto bits_per_sample = frame->header.bits_per_sample;
  const auto sample_rate = frame->header.sample_rate;
  const auto samples = frame->header.blocksize;

  if (channels != 1 && channels != 2) {
    set_callback_error("FLAC stream has unsupported channel count: " + std::to_string(channels));
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (bits_per_sample == 0 || bits_per_sample > 32) {
    set_callback_error("FLAC stream has unsupported bit depth: " + std::to_string(bits_per_sample));
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }
  if (sample_rate == 0) {
    set_callback_error("FLAC stream has invalid sample rate");
    return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
  }

  PcmBlock block;
  block.sample_rate = static_cast<long>(sample_rate);
  block.channels = static_cast<int>(channels);
  block.left.reserve(samples);
  block.right.reserve(samples);

  for (unsigned i = 0; i < samples; ++i) {
    const auto left = flac_to_i16(buffer[0][i], bits_per_sample);
    block.left.push_back(left);
    block.right.push_back(channels == 1 ? left : flac_to_i16(buffer[1][i], bits_per_sample));
  }

  pending_ = std::move(block);
  return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}

void FlacDecoder::handle_metadata(const FLAC__StreamMetadata* metadata) {
  if (!metadata || metadata->type != FLAC__METADATA_TYPE_STREAMINFO) {
    return;
  }
  const auto& stream_info = metadata->data.stream_info;
  if (stream_info.channels != 1 && stream_info.channels != 2) {
    set_callback_error("FLAC stream has unsupported channel count: " +
                       std::to_string(stream_info.channels));
  } else if (stream_info.bits_per_sample == 0 || stream_info.bits_per_sample > 32) {
    set_callback_error("FLAC stream has unsupported bit depth: " +
                       std::to_string(stream_info.bits_per_sample));
  } else if (stream_info.sample_rate == 0) {
    set_callback_error("FLAC stream has invalid sample rate");
  }
}

void FlacDecoder::check_callback_error() const {
  if (!callback_error_.empty()) {
    throw Error(callback_error_);
  }
}

FLAC__StreamDecoderWriteStatus FlacDecoder::write_callback(const FLAC__StreamDecoder*,
                                                           const FLAC__Frame* frame,
                                                           const FLAC__int32* const buffer[],
                                                           void* client_data) {
  auto* self = static_cast<FlacDecoder*>(client_data);
  return self->handle_write(frame, buffer);
}

void FlacDecoder::metadata_callback(const FLAC__StreamDecoder*,
                                    const FLAC__StreamMetadata* metadata,
                                    void* client_data) {
  auto* self = static_cast<FlacDecoder*>(client_data);
  self->handle_metadata(metadata);
}

void FlacDecoder::error_callback(const FLAC__StreamDecoder*, FLAC__StreamDecoderErrorStatus status,
                                 void* client_data) {
  auto* self = static_cast<FlacDecoder*>(client_data);
  self->decoder_error_ = FLAC__StreamDecoderErrorStatusString[status];
}

} // namespace modern_ices
