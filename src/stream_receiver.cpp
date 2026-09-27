#include "stream_receiver.hpp"

bool SpscQueue::push(hub_sample_t sample) {
    const auto tail = tail_.load(std::memory_order_relaxed);
    const auto head = head_.load(std::memory_order_acquire);
    if (tail - head == HUB_QUEUE_CAPACITY) return false;
    slots_[tail % HUB_QUEUE_CAPACITY] = sample;
    tail_.store(tail + 1, std::memory_order_release);
    return true;
}

bool SpscQueue::pop(hub_sample_t &sample) {
    const auto head = head_.load(std::memory_order_relaxed);
    const auto tail = tail_.load(std::memory_order_acquire);
    if (tail == head) return false;
    sample = slots_[head % HUB_QUEUE_CAPACITY];
    head_.store(head + 1, std::memory_order_release);
    return true;
}

void StreamReceiver::discard_one() {
    for (std::size_t i = 1; i < length_; ++i) frame_[i - 1] = frame_[i];
    --length_;
}

void StreamReceiver::receive(std::uint8_t byte) {
    if (length_ == 0 && byte != HUB_SYNC) return;
    frame_[length_++] = byte;
    while (length_ > 0) {
        if (frame_[0] != HUB_SYNC) { discard_one(); continue; }
        if (length_ < HUB_PACKET_SIZE) return;
        hub_sample_t sample{};
        if (!hub_decode(frame_.data(), frame_.size(), &sample)) {
            ++stats_.bad_frames;
            discard_one(); // slide one byte; sync might be inside damaged frame
            continue;
        }
        length_ = 0;
        if (have_sequence_) {
            const auto delta = static_cast<std::uint16_t>(sample.sequence - previous_sequence_);
            if (delta == 0 || delta >= 0x8000u) ++stats_.out_of_order;
            else stats_.missing_packets += static_cast<unsigned>(delta - 1);
        }
        previous_sequence_ = sample.sequence;
        have_sequence_ = true;
        if (!queue_.push(sample)) ++stats_.queue_drops;
        else ++stats_.accepted;
        return;
    }
}

bool ScriptedAdc::read(std::int16_t &out) {
    if (next_ == count_) return false;
    out = values_[next_++];
    return true;
}

bool PeriodicSampler::tick(std::uint32_t now, SampleSource &source,
                           std::array<std::uint8_t, HUB_PACKET_SIZE> &wire) {
    if (period_ == 0 || now < next_tick_) return false;
    next_tick_ = now + period_; // caller must not run near 32-bit tick wrap
    std::int16_t reading;
    if (!source.read(reading)) return false;
    if (!initialized_) { hub_filter_init(&filter_); initialized_ = true; }
    hub_sample_t sample{sequence_++, hub_filter_update(&filter_, reading)};
    hub_encode(sample, wire.data());
    return true;
}
