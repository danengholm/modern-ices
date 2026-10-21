#pragma once

#include "Config.hpp"

#include <cstddef>
#include <shout/shout.h>
#include <string>

namespace modern_ices {

class ShoutRuntime {
  public:
    ShoutRuntime();
    ~ShoutRuntime();

    ShoutRuntime(const ShoutRuntime&) = delete;
    ShoutRuntime& operator=(const ShoutRuntime&) = delete;
};

class IcecastClient {
  public:
    explicit IcecastClient(const Config& config);
    ~IcecastClient();

    IcecastClient(const IcecastClient&) = delete;
    IcecastClient& operator=(const IcecastClient&) = delete;

    void connect();
    void close() noexcept;
    void send(const unsigned char* data, std::size_t length);
    void sync() noexcept;

  private:
    shout_t* shout_ = nullptr;

    void check(int rc, const std::string& action) const;
    std::string last_error(const std::string& action) const;
};

} // namespace modern_ices
