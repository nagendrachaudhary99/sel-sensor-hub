#ifndef SENSOR_HUB_H
#define SENSOR_HUB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Q8.8: signed raw value / 256 is the value in sensor units. */
#define HUB_QUEUE_CAPACITY 8u
#define HUB_PACKET_SIZE 8u
#define HUB_SYNC 0xA5u
#define HUB_VERSION 1u

typedef struct {
    uint16_t sequence;
    int16_t value_q8_8;
} hub_sample_t;

typedef struct {
    hub_sample_t slots[HUB_QUEUE_CAPACITY];
    size_t head; /* next item to read */
    size_t count;
} hub_queue_t;

typedef struct {
    int32_t value_q8_8;
    bool initialized;
} hub_filter_t;

void hub_queue_init(hub_queue_t *queue);
bool hub_queue_push(hub_queue_t *queue, hub_sample_t sample);
bool hub_queue_pop(hub_queue_t *queue, hub_sample_t *out);

/* CRC-16/CCITT-FALSE: polynomial 0x1021, initial 0xFFFF, no reflection/xorout. */
uint16_t hub_crc16(const uint8_t *data, size_t length);

/* Wire format: sync, version, sequence LE16, value LE16, CRC LE16. */
void hub_encode(hub_sample_t sample, uint8_t out[HUB_PACKET_SIZE]);
bool hub_decode(const uint8_t *bytes, size_t length, hub_sample_t *out);

void hub_filter_init(hub_filter_t *filter);
/* First sample seeds the filter; later samples apply y += (x-y)/4. */
int16_t hub_filter_update(hub_filter_t *filter, int16_t input_q8_8);

#ifdef __cplusplus
}
#endif

#endif
