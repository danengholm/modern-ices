#pragma once

#include <sstream>
#include <stdexcept>
#include <string>

namespace modern_ices {

// Expected operational failures are reported with this type so main can keep messages tidy.
class Error : public std::runtime_error {
  public:
    explicit Error(const std::string& message) : std::runtime_error(message) {}
    explicit Error(const std::string& message, int val) : std::runtime_error(fmtmsg(message, val)) {}

  private:
    static std::string fmtmsg(const std::string& message, int val)
    {
        std::ostringstream os;
        os << message << " (return=" << val << ")";
        return os.str();
    }
};

} // namespace modern_ices
