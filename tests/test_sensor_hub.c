#include "sensor_hub.h"
#include <stdio.h>
#include <string.h>

static unsigned tests_run = 0;
#define CHECK(expr) do { tests_run++; if (!(expr)) { \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; \
} } while (0)

static int test_crc(void) {
    const uint8_t vector[] = "123456789";
    CHECK(hub_crc16(vector, 9) == 0x29B1u); /* Published CCITT-FALSE check value. */
    CHECK(hub_crc16(vector, 0) == 0xFFFFu);
    return 0;
}

static int test_packets(void) {
    const hub_sample_t input = { 0x1234u, -384 }; /* -1.5 in Q8.8 */
    uint8_t packet[HUB_PACKET_SIZE];
    hub_encode(input, packet);
    CHECK(packet[0] == 0xA5u && packet[1] == 1u);
    CHECK(packet[2] == 0x34u && packet[3] == 0x12u);
    CHECK(packet[4] == 0x80u && packet[5] == 0xFEu);
    hub_sample_t output = { 99u, 99 };
    CHECK(hub_decode(packet, sizeof packet, &output));
    CHECK(output.sequence == input.sequence && output.value_q8_8 == input.value_q8_8);
    CHECK(!hub_decode(packet, sizeof packet - 1, &output));
    CHECK(!hub_decode(NULL, sizeof packet, &output));
    CHECK(!hub_decode(packet, sizeof packet, NULL));
    packet[4] ^= 0x01u;
    CHECK(!hub_decode(packet, sizeof packet, &output));
    packet[4] ^= 0x01u;
    packet[0] = 0;
    CHECK(!hub_decode(packet, sizeof packet, &output));
    packet[0] = HUB_SYNC;
    packet[1] = 2;
    CHECK(!hub_decode(packet, sizeof packet, &output));
    hub_encode((hub_sample_t){ UINT16_MAX, INT16_MIN }, packet);
    CHECK(hub_decode(packet, sizeof packet, &output));
    CHECK(output.sequence == UINT16_MAX && output.value_q8_8 == INT16_MIN);
    hub_encode((hub_sample_t){ 0, INT16_MAX }, packet);
    CHECK(hub_decode(packet, sizeof packet, &output));
    CHECK(output.value_q8_8 == INT16_MAX);
    return 0;
}

static int test_queue(void) {
    hub_queue_t queue;
    hub_sample_t sample;
    hub_queue_init(&queue);
    CHECK(!hub_queue_pop(&queue, &sample));
    for (unsigned i = 0; i < HUB_QUEUE_CAPACITY; i++)
        CHECK(hub_queue_push(&queue, (hub_sample_t){ (uint16_t)i, (int16_t)i }));
    CHECK(!hub_queue_push(&queue, (hub_sample_t){ 99, 99 }));
    for (unsigned i = 0; i < 3; i++) {
        CHECK(hub_queue_pop(&queue, &sample));
        CHECK(sample.sequence == i);
    }
    for (unsigned i = HUB_QUEUE_CAPACITY; i < HUB_QUEUE_CAPACITY + 3; i++)
        CHECK(hub_queue_push(&queue, (hub_sample_t){ (uint16_t)i, (int16_t)i }));
    for (unsigned i = 3; i < HUB_QUEUE_CAPACITY + 3; i++) {
        CHECK(hub_queue_pop(&queue, &sample));
        CHECK(sample.sequence == i);
    }
    CHECK(!hub_queue_pop(&queue, &sample));
    CHECK(!hub_queue_pop(&queue, NULL));
    return 0;
}

static int test_filter(void) {
    hub_filter_t filter;
    hub_filter_init(&filter);
    CHECK(hub_filter_update(&filter, 0) == 0);
    CHECK(hub_filter_update(&filter, 1024) == 256);
    CHECK(hub_filter_update(&filter, 1024) == 448);
    hub_filter_init(&filter);
    CHECK(hub_filter_update(&filter, -1024) == -1024);
    CHECK(hub_filter_update(&filter, 0) == -768);
    hub_filter_init(&filter);
    CHECK(hub_filter_update(&filter, INT16_MIN) == INT16_MIN);
    CHECK(hub_filter_update(&filter, INT16_MAX) == -16385);
    return 0;
}

int main(void) {
    if (test_crc() || test_packets() || test_queue() || test_filter()) return 1;
    printf("PASS %u checks\n", tests_run);
    return 0;
}
