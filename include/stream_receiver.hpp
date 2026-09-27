#ifndef STREAM_RECEIVER_HPP
#define STREAM_RECEIVER_HPP

#include "sensor_pipeline.hpp"
#include <atomic>
#include <array>
#include <cstdint>

// Exactly one producer and one consumer. Both live for the entire queue lifetime.
// The producer owns tail, the consumer owns head; capacity is HUB_QUEUE_CAPACITY.
class SpscQueue {
public:
    bool push(hub_sample_t sample);
    bool pop(hub_sample_t &sample);
private:
    std::array<hub_sample_t, HUB_QUEUE_CAPACITY> slots_{};
    std::atomic<std::uint32_t> head_{0};
    std::atomic<std::uint32_t> tail_{0};
};

struct ReceiverStats {
    unsigned bad_frames = 0;
    unsigned missing_packets = 0;
    unsigned out_of_order = 0;
    unsigned queue_drops = 0;
    unsigned accepted = 0;
};

class StreamReceiver {
public:
    explicit StreamReceiver(SpscQueue &queue) : queue_(queue) {}
    // Call from one producer only; feed the bytes of a UART-like stream.
    void receive(std::uint8_t byte);
    ReceiverStats stats() const { return stats_; } // Producer side only or after joining threads.
private:
    void discard_one();
    SpscQueue &queue_;
    std::array<std::uint8_t, HUB_PACKET_SIZE> frame_{};
    std::size_t length_ = 0;
    bool have_sequence_ = false;
    std::uint16_t previous_sequence_ = 0;
    ReceiverStats stats_{};
};

// Scripted ADC-like input. Values are already quantized Q8.8; no analog modeling.
class ScriptedAdc : public SampleSource {
public:
    ScriptedAdc(const std::int16_t *values, std::size_t count) : values_(values), count_(count) {}
    bool read(std::int16_t &out) override;
private:
    const std::int16_t *values_;
    std::size_t count_;
    std::size_t next_ = 0;
};

// Caller supplies monotonically increasing ticks. At most one read per invocation;
// late calls skip overdue slots instead of claiming to have sampled the past.
class PeriodicSampler {
public:
    explicit PeriodicSampler(std::uint32_t period_ticks) : period_(period_ticks) {}
    bool tick(std::uint32_t now, SampleSource &source,
              std::array<std::uint8_t, HUB_PACKET_SIZE> &wire);
private:
    std::uint32_t period_;
    std::uint32_t next_tick_ = 0;
    std::uint16_t sequence_ = 0;
    hub_filter_t filter_{};
    bool initialized_ = false;
};

#endif
