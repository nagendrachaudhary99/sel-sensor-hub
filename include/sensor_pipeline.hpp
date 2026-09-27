#ifndef SENSOR_PIPELINE_HPP
#define SENSOR_PIPELINE_HPP

#include "sensor_hub.h"
#include <array>
#include <cstdint>

/* Hardware boundary: a device-specific ADC adapter could implement this interface. */
class SampleSource {
public:
    virtual ~SampleSource() = default;
    virtual bool read(std::int16_t &value_q8_8) = 0;
};

class SensorPipeline {
public:
    SensorPipeline();
    // One bounded step: at most one input, filter, packet and queue operation.
    // Returns false if no input, the queue is full, or decoding fails.
    bool step(SampleSource &source, std::array<std::uint8_t, HUB_PACKET_SIZE> &wire);
    bool consume(hub_sample_t &sample);
    unsigned dropped() const { return dropped_; }
private:
    hub_queue_t queue_;
    hub_filter_t filter_;
    std::uint16_t next_sequence_ = 0;
    unsigned dropped_ = 0;
};

#endif
