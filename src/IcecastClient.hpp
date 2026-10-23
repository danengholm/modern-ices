#pragma once

#include "Config.hpp"

#include <cstddef>
#include <shout/shout.h>
#include <string>

namespace modern_ices {

// libshout also has process-global setup/teardown separate from per-connection state.
class ShoutRuntime {
  public:
    ShoutRuntime();
    ~ShoutRuntime();

    ShoutRuntime(const ShoutRuntime&) = delete;
    ShoutRuntime& operator=(const ShoutRuntime&) = delete;
};

// Owns one source connection and turns libshout return codes into Error exceptions.
class IcecastClient {
  public:
    explicit IcecastClient(const Config& config);
    ~IcecastClient();

    IcecastClient(const IcecastClient&) = delete;
    IcecastClient& operator=(const IcecastClient&) = delete;

    void connect();
    void close() noexcept;
    void send(const unsigned char* data, std::size_t length);
    void set_stream_title(const std::string& title);
    void sync() noexcept;

  private:
    shout_t* shout_{nullptr};

    void check(int rc, const std::string& action) const;
    std::string last_error(const std::string& action) const;
};

} // namespace modern_ices
