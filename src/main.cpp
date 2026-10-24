#include "Config.hpp"
#include "Error.hpp"
#include "Logger.hpp"
#include "Streamer.hpp"

#include <exception>

// Keep top-level control flow small: parse, run, and translate failures into shell status.
int main(int argc, char** argv) {
    try {
        auto config{modern_ices::Config::parse(argc, argv)};
        modern_ices::Logger::set_verbose(config.verbose);
        modern_ices::Logger::set_timestamp_format(config.log_timestamps,
                                                  config.log_timestamp_format);
        modern_ices::Streamer streamer(std::move(config));
        streamer.run();
        return 0;
    } catch (const modern_ices::Error& e) {
        modern_ices::Logger::error(e.what());
        return 1;
    } catch (const std::exception& e) {
        modern_ices::Logger::error(std::string("unexpected failure: ") + e.what());
        return 1;
    } catch (...) {
        modern_ices::Logger::error("unexpected non-standard exception");
        return 1;
    }
}
