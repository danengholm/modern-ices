#pragma once

#include "AudioDecoder.hpp"

#include <cstddef>
#include <vector>

namespace modern_ices {

// Holds back the end of each track so it can be mixed with the beginning of the next one.
class Crossfader {
  public:
    explicit Crossfader(int seconds);

    PcmBlock start_track(const PcmBlock& first_block);
    std::vector<PcmBlock> process(const PcmBlock& block);
    PcmBlock finish();

  private:
    int seconds_{0};
    std::size_t fade_samples_{0};
    long sample_rate_{0};
    int channels_{0};
    PcmBlock tail_;
    std::size_t fade_total_{0};
    std::size_t fade_remaining_{0};
    std::size_t fade_offset_{0};

    void validate_format(const PcmBlock& block) const;
    PcmBlock process_tail_buffer(const PcmBlock& block, std::size_t offset);
    PcmBlock mix_fade(const PcmBlock& block, std::size_t count);
};

} // namespace modern_ices
