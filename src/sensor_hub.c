#include "sensor_hub.h"

void hub_queue_init(hub_queue_t *queue) {
    queue->head = 0;
    queue->count = 0;
}

bool hub_queue_push(hub_queue_t *queue, hub_sample_t sample) {
    if (queue->count == HUB_QUEUE_CAPACITY) return false;
    size_t tail = (queue->head + queue->count) % HUB_QUEUE_CAPACITY;
    queue->slots[tail] = sample;
    queue->count++;
    return true;
}

bool hub_queue_pop(hub_queue_t *queue, hub_sample_t *out) {
    if (queue->count == 0 || out == NULL) return false;
    *out = queue->slots[queue->head];
    queue->head = (queue->head + 1) % HUB_QUEUE_CAPACITY;
    queue->count--;
    return true;
}

uint16_t hub_crc16(const uint8_t *data, size_t length) {
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (unsigned bit = 0; bit < 8; bit++) {
            crc = (crc & 0x8000u) ? (uint16_t)((crc << 1) ^ 0x1021u)
                                   : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static uint16_t read_le16(const uint8_t *bytes) {
    return (uint16_t)((uint16_t)bytes[0] | ((uint16_t)bytes[1] << 8));
}

static void write_le16(uint8_t *bytes, uint16_t value) {
    bytes[0] = (uint8_t)(value & 0xFFu);
    bytes[1] = (uint8_t)(value >> 8);
}

void hub_encode(hub_sample_t sample, uint8_t out[HUB_PACKET_SIZE]) {
    out[0] = HUB_SYNC;
    out[1] = HUB_VERSION;
    write_le16(&out[2], sample.sequence);
    write_le16(&out[4], (uint16_t)sample.value_q8_8);
    write_le16(&out[6], hub_crc16(out, 6));
}

bool hub_decode(const uint8_t *bytes, size_t length, hub_sample_t *out) {
    if (bytes == NULL || out == NULL || length != HUB_PACKET_SIZE) return false;
    if (bytes[0] != HUB_SYNC || bytes[1] != HUB_VERSION) return false;
    if (read_le16(&bytes[6]) != hub_crc16(bytes, 6)) return false;
    /* Map the on-wire two's-complement value without an out-of-range unsigned-to-signed cast. */
    uint16_t raw = read_le16(&bytes[4]);
    int32_t signed_value = raw <= INT16_MAX ? (int32_t)raw : (int32_t)raw - 65536;
    out->sequence = read_le16(&bytes[2]);
    out->value_q8_8 = (int16_t)signed_value;
    return true;
}

void hub_filter_init(hub_filter_t *filter) {
    filter->value_q8_8 = 0;
    filter->initialized = false;
}

int16_t hub_filter_update(hub_filter_t *filter, int16_t input_q8_8) {
    if (!filter->initialized) {
        filter->value_q8_8 = input_q8_8;
        filter->initialized = true;
    } else {
        /* int32_t avoids overflow even for the full int16_t input range. */
        filter->value_q8_8 += ((int32_t)input_q8_8 - filter->value_q8_8) / 4;
    }
    return (int16_t)filter->value_q8_8;
}
