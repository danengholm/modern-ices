#pragma once

#include <iostream>
#include <string>

namespace modern_ices {

class Logger {
  public:
    static void set_verbose(bool value) { verbose_ = value; }

    static void info(const std::string& message) { std::cerr << message << '\n'; }

    static void debug(const std::string& message) {
        if (verbose_) {
            std::cerr << message << '\n';
        }
    }

    static void error(const std::string& message) { std::cerr << "error: " << message << '\n'; }

  private:
    inline static bool verbose_{false};
};

} // namespace modern_ices
