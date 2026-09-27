#include "sensor_pipeline.hpp"
#include <cstdio>

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } checks++; } while (0)

class SequenceSource : public SampleSource {
public:
    explicit SequenceSource(std::int16_t value) : value_(value) {}
    bool read(std::int16_t &out) override { out = value_; return true; }
    void set(std::int16_t value) { value_ = value; }
private:
    std::int16_t value_;
};

class EmptySource : public SampleSource {
public:
    bool read(std::int16_t &) override { return false; }
};

int main() {
    unsigned checks = 0;
    SensorPipeline pipeline;
    SequenceSource source(20 * 256);
    EmptySource empty;
    std::array<std::uint8_t, HUB_PACKET_SIZE> wire{};
    hub_sample_t sample{};
    CHECK(!pipeline.step(empty, wire));
    CHECK(!pipeline.consume(sample));
    CHECK(pipeline.step(source, wire));
    CHECK(hub_decode(wire.data(), wire.size(), &sample));
    CHECK(sample.sequence == 0 && sample.value_q8_8 == 20 * 256);
    source.set(24 * 256);
    CHECK(pipeline.step(source, wire));
    CHECK(pipeline.consume(sample));
    CHECK(sample.sequence == 0);
    CHECK(pipeline.consume(sample));
    CHECK(sample.sequence == 1 && sample.value_q8_8 == 21 * 256);
    CHECK(!pipeline.consume(sample));
    for (unsigned i = 0; i < HUB_QUEUE_CAPACITY; i++) CHECK(pipeline.step(source, wire));
    source.set(28 * 256);
    CHECK(!pipeline.step(source, wire));
    CHECK(pipeline.dropped() == 1);
    CHECK(pipeline.consume(sample));
    CHECK(pipeline.step(source, wire));
    CHECK(pipeline.consume(sample));
    CHECK(sample.sequence == 3 && sample.value_q8_8 == 5712);
    // Drop did not advance sequence or filter; the next accepted sample is sequence 10.
    for (unsigned i = 0; i < HUB_QUEUE_CAPACITY - 2; i++) CHECK(pipeline.consume(sample));
    CHECK(pipeline.consume(sample));
    CHECK(sample.sequence == 10 && sample.value_q8_8 == 6341);
    CHECK(!pipeline.consume(sample));
    std::printf("PASS %u C++ checks\n", checks);
}
