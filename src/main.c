#include "sensor_hub.h"
#include <stdio.h>

int main(void) {
    const int16_t readings[] = { 20 * 256, 22 * 256, 21 * 256, 25 * 256 };
    hub_queue_t queue;
    hub_filter_t filter;
    hub_queue_init(&queue);
    hub_filter_init(&filter);

    for (size_t i = 0; i < sizeof readings / sizeof readings[0]; i++) {
        hub_sample_t sample = { (uint16_t)i, hub_filter_update(&filter, readings[i]) };
        uint8_t packet[HUB_PACKET_SIZE];
        hub_encode(sample, packet);
        hub_sample_t received;
        if (!hub_decode(packet, sizeof packet, &received) || !hub_queue_push(&queue, received)) {
            fputs("packet rejected or queue full\n", stderr);
            return 1;
        }
        printf("seq=%u input=%.2f filtered=%.2f packet=", (unsigned)sample.sequence,
               readings[i] / 256.0, sample.value_q8_8 / 256.0);
        for (size_t byte = 0; byte < sizeof packet; byte++) printf("%02X", packet[byte]);
        putchar('\n');
    }

    hub_sample_t sample;
    while (hub_queue_pop(&queue, &sample))
        printf("consumed seq=%u value=%.2f\n", (unsigned)sample.sequence, sample.value_q8_8 / 256.0);
    return 0;
}
