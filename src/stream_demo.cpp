#include "stream_receiver.hpp"
#include <cstdio>
int main() {
    std::int16_t readings[] = {20*256, 22*256, 21*256, 25*256};
    ScriptedAdc adc(readings, 4);
    PeriodicSampler sampler(5);
    SpscQueue queue;
    StreamReceiver receiver(queue);
    std::array<std::uint8_t, HUB_PACKET_SIZE> wire{};
    for (std::uint32_t tick = 0; tick < 20; ++tick) {
        if (!sampler.tick(tick, adc, wire)) continue;
        if (tick == 10) wire[4] ^= 1; // Inject accidental corruption.
        for (auto byte : wire) receiver.receive(byte);
    }
    hub_sample_t sample{};
    while (queue.pop(sample))
        std::printf("seq=%u value=%.2f\n", sample.sequence, sample.value_q8_8 / 256.0);
    const auto stats = receiver.stats();
    std::printf("accepted=%u corrupt=%u missing=%u out_of_order=%u queue_drops=%u\n",
                stats.accepted, stats.bad_frames, stats.missing_packets,
                stats.out_of_order, stats.queue_drops);
}
