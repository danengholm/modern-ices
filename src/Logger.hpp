#pragma once

#include <array>
#include <chrono>
#include <ctime>
#include <iostream>
#include <string>
#include <utility>

namespace modern_ices {

// Minimal stderr logger; Icecast source clients usually run under a service manager.
class Logger {
  public:
    static void set_verbose(bool value) { verbose_ = value; }
    static void set_timestamp_format(bool enabled, std::string format) {
        timestamps_enabled_ = enabled;
        timestamp_format_ = std::move(format);
    }

    static void info(const std::string& message) { write("", message); }

    static void debug(const std::string& message) {
        if (verbose_) {
            write("", message);
        }
    }

    static void error(const std::string& message) { write("error: ", message); }

  private:
    static std::string timestamp() {
        if (!timestamps_enabled_) {
            return {};
        }

        const auto now{std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())};
        std::tm local_time{};
        if (localtime_r(&now, &local_time) == nullptr) {
            return {};
        }

        std::array<char, 256> buffer{};
        if (std::strftime(buffer.data(), buffer.size(), timestamp_format_.c_str(), &local_time) ==
            0) {
            return {};
        }
        return buffer.data();
    }

    static void write(const std::string& prefix, const std::string& message) {
        const auto stamp{timestamp()};
        if (!stamp.empty()) {
            std::cerr << stamp << ' ';
        }
        std::cerr << prefix << message << '\n';
    }

    inline static bool verbose_{false};
    inline static bool timestamps_enabled_{false};
    inline static std::string timestamp_format_{"%G"};
};

} // namespace modern_ices
