#include "stream_receiver.hpp"
#include <cstdio>
#include <cstdint>

static unsigned checks;
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); return 1; } ++checks; } while (0)
static void feed(StreamReceiver &rx, std::uint16_t seq, bool corrupt = false) {
    std::uint8_t packet[HUB_PACKET_SIZE];
    hub_encode({seq, static_cast<std::int16_t>(seq)}, packet);
    if (corrupt) packet[4] ^= 0x40;
    for (auto byte : packet) rx.receive(byte);
}
int main() {
    SpscQueue queue;
    StreamReceiver rx(queue);
    hub_sample_t sample{};
    CHECK(!queue.pop(sample));
    rx.receive(0x12); rx.receive(0x00); // noise ignored
    feed(rx, 65534);
    feed(rx, 65535);
    feed(rx, 0); // wrap without gap
    CHECK(rx.stats().accepted == 3 && rx.stats().missing_packets == 0);
    CHECK(queue.pop(sample) && sample.sequence == 65534);
    CHECK(queue.pop(sample) && sample.sequence == 65535);
    CHECK(queue.pop(sample) && sample.sequence == 0);
    feed(rx, 1, true); // corrupt packet: next valid packet shows one missing
    feed(rx, 2);
    CHECK(rx.stats().bad_frames >= 1);
    CHECK(rx.stats().missing_packets == 1);
    CHECK(queue.pop(sample) && sample.sequence == 2);
    feed(rx, 2); // duplicate
    CHECK(rx.stats().out_of_order == 1);
    CHECK(queue.pop(sample));
    // Noise plus a truncated frame followed by another sync resynchronizes.
    rx.receive(HUB_SYNC); rx.receive(1); rx.receive(0x33);
    feed(rx, 3);
    CHECK(rx.stats().bad_frames >= 1);
    // Frame 3 may be lost because a partial frame consumes its first bytes;
    // a subsequent complete frame must recover.
    feed(rx, 4);
    CHECK(rx.stats().accepted >= 5);
    while (queue.pop(sample)) {}
    for (unsigned i = 0; i < HUB_QUEUE_CAPACITY; ++i) feed(rx, static_cast<std::uint16_t>(10+i));
    feed(rx, 18);
    CHECK(rx.stats().queue_drops == 1);
    for (unsigned i = 0; i < HUB_QUEUE_CAPACITY; ++i) {
        CHECK(queue.pop(sample));
        CHECK(sample.sequence == i + 10);
    }
    CHECK(!queue.pop(sample));
    // Backward sequence does not claim a huge gap.
    feed(rx, 17);
    CHECK(rx.stats().out_of_order == 2); // duplicate 2 and backward 18 -> 17
    CHECK(queue.pop(sample) && sample.sequence == 17);
    // Byte-by-byte accumulation: nothing appears before the final CRC byte.
    std::uint8_t split[HUB_PACKET_SIZE];
    hub_encode({19, 1234}, split);
    for (std::size_t i = 0; i < HUB_PACKET_SIZE - 1; ++i) rx.receive(split[i]);
    CHECK(!queue.pop(sample));
    rx.receive(split[HUB_PACKET_SIZE - 1]);
    CHECK(queue.pop(sample) && sample.sequence == 19 && sample.value_q8_8 == 1234);
    std::int16_t readings[] = { 20*256, 24*256, 28*256 };
    ScriptedAdc adc(readings, 3);
    PeriodicSampler sampler(5);
    std::array<std::uint8_t, HUB_PACKET_SIZE> wire{};
    CHECK(sampler.tick(0, adc, wire));
    CHECK(hub_decode(wire.data(), wire.size(), &sample));
    CHECK(sample.sequence == 0 && sample.value_q8_8 == 20*256);
    CHECK(!sampler.tick(4, adc, wire));
    CHECK(sampler.tick(5, adc, wire));
    CHECK(hub_decode(wire.data(), wire.size(), &sample));
    CHECK(sample.sequence == 1 && sample.value_q8_8 == 21*256);
    CHECK(sampler.tick(17, adc, wire)); // late call: one sample, next due at 22
    CHECK(hub_decode(wire.data(), wire.size(), &sample));
    CHECK(sample.sequence == 2 && sample.value_q8_8 == 22*256 + 192);
    CHECK(!sampler.tick(18, adc, wire));
    CHECK(!sampler.tick(22, adc, wire)); // script exhausted
    std::printf("PASS %u stream/sampling checks\n", checks);
}
