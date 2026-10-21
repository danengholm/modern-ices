#pragma once

#include "AudioDecoder.hpp"

#include <FLAC/stream_decoder.h>
#include <string>

namespace modern_ices {

// FLAC uses callbacks for decoded frames; pending_ stores the frame until read() returns it.
class FlacDecoder final : public PcmDecoder {
  public:
    explicit FlacDecoder(const std::string& path);
    ~FlacDecoder() override;

    FlacDecoder(const FlacDecoder&) = delete;
    FlacDecoder& operator=(const FlacDecoder&) = delete;

    PcmBlock read() override;

  private:
    FLAC__StreamDecoder* decoder_{nullptr};
    PcmBlock pending_;
    // FLAC callbacks cannot throw safely through the C API, so errors are captured and checked
    // later.
    std::string callback_error_;
    std::string decoder_error_;

    PcmBlock take_pending();
    void set_callback_error(std::string message);
    FLAC__StreamDecoderWriteStatus handle_write(const FLAC__Frame* frame,
                                                const FLAC__int32* const buffer[]);
    void handle_metadata(const FLAC__StreamMetadata* metadata);
    void check_callback_error() const;

    static FLAC__StreamDecoderWriteStatus write_callback(const FLAC__StreamDecoder* decoder,
                                                         const FLAC__Frame* frame,
                                                         const FLAC__int32* const buffer[],
                                                         void* client_data);
    static void metadata_callback(const FLAC__StreamDecoder* decoder,
                                  const FLAC__StreamMetadata* metadata, void* client_data);
    static void error_callback(const FLAC__StreamDecoder* decoder,
                               FLAC__StreamDecoderErrorStatus status, void* client_data);
};

} // namespace modern_ices
