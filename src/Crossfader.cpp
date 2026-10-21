#include "Crossfader.hpp"

#include "Error.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>

namespace modern_ices {
namespace {

std::int16_t clamp_sample(double sample) {
    const auto clipped{std::clamp(sample,
                                  static_cast<double>(std::numeric_limits<std::int16_t>::min()),
                                  static_cast<double>(std::numeric_limits<std::int16_t>::max()))};
    return static_cast<std::int16_t>(std::lrint(clipped));
}

void append_sample(PcmBlock& block, std::int16_t left, std::int16_t right) {
    block.left.push_back(left);
    block.right.push_back(right);
}

std::int16_t left_at(const PcmBlock& tail, const PcmBlock& block, std::size_t block_offset,
                     std::size_t index) {
    if (index < tail.left.size()) {
        return tail.left[index];
    }
    return block.left[block_offset + index - tail.left.size()];
}

std::int16_t right_at(const PcmBlock& tail, const PcmBlock& block, std::size_t block_offset,
                      std::size_t index) {
    if (index < tail.right.size()) {
        return tail.right[index];
    }
    return block.right[block_offset + index - tail.right.size()];
}

} // namespace

Crossfader::Crossfader(int seconds) : seconds_{seconds} {
    if (seconds_ < 0) {
        throw Error("crossfade length cannot be negative");
    }
}

PcmBlock Crossfader::start_track(const PcmBlock& first_block) {
    if (first_block.empty()) {
        return {};
    }

    if (sample_rate_ == 0) {
        sample_rate_ = first_block.sample_rate;
        channels_ = first_block.channels;
        fade_samples_ = static_cast<std::size_t>(seconds_) * static_cast<std::size_t>(sample_rate_);
        return {};
    }

    validate_format(first_block);
    if (tail_.empty()) {
        return {};
    }

    if (fade_samples_ == 0 || tail_.samples() < static_cast<int>(fade_samples_)) {
        return finish();
    }

    fade_total_ = std::min<std::size_t>(tail_.samples(), fade_samples_);
    fade_remaining_ = fade_total_;
    fade_offset_ = 0;
    return {};
}

std::vector<PcmBlock> Crossfader::process(const PcmBlock& block) {
    if (block.empty()) {
        return {};
    }
    validate_format(block);

    std::vector<PcmBlock> output;
    std::size_t offset{0};
    if (fade_remaining_ > 0) {
        const auto count{std::min<std::size_t>(fade_remaining_, block.samples())};
        output.push_back(mix_fade(block, count));
        offset += count;
        fade_remaining_ -= count;
        fade_offset_ += count;
        if (fade_remaining_ == 0) {
            tail_ = {};
            fade_total_ = 0;
            fade_offset_ = 0;
        }
    }

    auto delayed{process_tail_buffer(block, offset)};
    if (!delayed.empty()) {
        output.push_back(std::move(delayed));
    }
    return output;
}

PcmBlock Crossfader::finish() {
    PcmBlock block{std::move(tail_)};
    tail_ = {};
    fade_total_ = 0;
    fade_remaining_ = 0;
    fade_offset_ = 0;
    return block;
}

void Crossfader::validate_format(const PcmBlock& block) const {
    if (block.sample_rate != sample_rate_ || block.channels != channels_) {
        throw Error(
            "crossfade requires all decoded tracks to have the same sample rate and channel count");
    }
}

PcmBlock Crossfader::process_tail_buffer(const PcmBlock& block, std::size_t offset) {
    if (offset >= block.left.size()) {
        return {};
    }
    if (fade_samples_ == 0) {
        PcmBlock passthrough;
        passthrough.sample_rate = sample_rate_;
        passthrough.channels = channels_;
        passthrough.left.assign(block.left.begin() + static_cast<std::ptrdiff_t>(offset),
                                block.left.end());
        passthrough.right.assign(block.right.begin() + static_cast<std::ptrdiff_t>(offset),
                                 block.right.end());
        return passthrough;
    }

    const auto incoming{block.left.size() - offset};
    const auto total{tail_.left.size() + incoming};
    const auto output_count{total > fade_samples_ ? total - fade_samples_ : 0};
    PcmBlock output;
    output.sample_rate = sample_rate_;
    output.channels = channels_;
    output.left.reserve(output_count);
    output.right.reserve(output_count);

    for (std::size_t i{0}; i < output_count; ++i) {
        append_sample(output, left_at(tail_, block, offset, i), right_at(tail_, block, offset, i));
    }

    PcmBlock new_tail;
    new_tail.sample_rate = sample_rate_;
    new_tail.channels = channels_;
    const auto tail_count{total - output_count};
    new_tail.left.reserve(tail_count);
    new_tail.right.reserve(tail_count);
    for (std::size_t i{output_count}; i < total; ++i) {
        append_sample(new_tail, left_at(tail_, block, offset, i),
                      right_at(tail_, block, offset, i));
    }
    tail_ = std::move(new_tail);
    return output;
}

PcmBlock Crossfader::mix_fade(const PcmBlock& block, std::size_t count) {
    PcmBlock mixed;
    mixed.sample_rate = sample_rate_;
    mixed.channels = channels_;
    mixed.left.reserve(count);
    mixed.right.reserve(count);

    for (std::size_t i{0}; i < count; ++i) {
        const auto position{fade_offset_ + i};
        const auto old_weight{static_cast<double>(fade_total_ - position) /
                              static_cast<double>(fade_total_)};
        const auto new_weight{1.0 - old_weight};
        append_sample(
            mixed, clamp_sample(tail_.left[position] * old_weight + block.left[i] * new_weight),
            clamp_sample(tail_.right[position] * old_weight + block.right[i] * new_weight));
    }
    return mixed;
}

} // namespace modern_ices
