#include "sensor_pipeline.hpp"

SensorPipeline::SensorPipeline() {
    hub_queue_init(&queue_);
    hub_filter_init(&filter_);
}

bool SensorPipeline::step(SampleSource &source,
                          std::array<std::uint8_t, HUB_PACKET_SIZE> &wire) {
    std::int16_t reading;
    if (!source.read(reading)) return false;
    // Check capacity before filtering: rejected samples do not change filter state.
    if (queue_.count == HUB_QUEUE_CAPACITY) {
        ++dropped_;
        return false;
    }
    hub_sample_t sample = { next_sequence_++, hub_filter_update(&filter_, reading) };
    hub_encode(sample, wire.data());
    hub_sample_t decoded;
    if (!hub_decode(wire.data(), wire.size(), &decoded)) return false;
    return hub_queue_push(&queue_, decoded);
}

bool SensorPipeline::consume(hub_sample_t &sample) {
    return hub_queue_pop(&queue_, &sample);
}
