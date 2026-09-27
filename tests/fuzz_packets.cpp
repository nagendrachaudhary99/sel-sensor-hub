#include "stream_receiver.hpp"
#include <cstdint>
#include <cstdio>
int main() {
    SpscQueue queue;
    StreamReceiver rx(queue);
    std::uint32_t rng = 0x6D2B79F5u;
    hub_sample_t sample{};
    std::uint8_t bytes[HUB_PACKET_SIZE];
    for (unsigned case_id = 0; case_id < 100000; ++case_id) {
        for (auto &byte : bytes) { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; byte = static_cast<std::uint8_t>(rng); }
        hub_decode(bytes, HUB_PACKET_SIZE, &sample);
        for (auto byte : bytes) rx.receive(byte);
        while (queue.pop(sample)) {}
    }
    std::printf("PASS 100000 deterministic arbitrary 8-byte cases (decode + stream)\n");
}
