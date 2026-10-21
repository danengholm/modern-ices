#pragma once

#include <stdexcept>
#include <string>

namespace modern_ices {

// Expected operational failures are reported with this type so main can keep messages tidy.
class Error : public std::runtime_error {
  public:
    explicit Error(const std::string& message) : std::runtime_error(message) {}
};

} // namespace modern_ices
