# Sensor Hub

A small, host-runnable C/C++ sensor pipeline that turns sampled readings into checked binary packets and recovers them from a byte stream.

![Build: make](https://img.shields.io/badge/build-make-blue) ![Tests: local](https://img.shields.io/badge/tests-make%20test-2ea44f) ![Language: C11 and C++17](https://img.shields.io/badge/language-C11%20%2B%20C%2B%2B17-informational)

> These badges describe the local commands and language standards. They are not CI status or a claim that tests run automatically on GitHub.

## At a glance

- Fixed-size ring queue and a separate single-producer/single-consumer (SPSC) queue, with no heap allocation in the data path.
- Q8.8 fixed-point smoothing with a 32-bit intermediate to avoid overflow at 16-bit input extremes.
- Explicit little-endian packet encoding and CRC-16/CCITT-FALSE validation.
- Byte-by-byte stream receiver with sync recovery, sequence-gap tracking, and error/drop counters.
- Scripted ADC-like input and a tick-driven periodic sampler; host demos require no board or external libraries.
- C and C++ tests plus 100,000 deterministic arbitrary-packet cases; optional ASan/UBSan run.

## Build and run

On macOS, install the Command Line Tools if `cc`, `c++`, or `make` is missing: `xcode-select --install`. A C11/C++17 toolchain and Make are required.

```sh
make                    # build both demos and test binaries
make test               # run all tests; nonzero exit on failure
./build/sensor_hub       # basic encode/decode and queue demo
./build/stream_demo      # scripted sampling, injected corruption, receiver counters
make sanitize           # optional: rebuild and test with ASan + UBSan
make clean && make      # return to an ordinary build after sanitize
```

Example from `./build/stream_demo` (the sample with sequence 2 has one corrupted byte):

```text
seq=0 value=20.00
seq=1 value=20.50
seq=3 value=21.72
accepted=3 corrupt=1 missing=1 out_of_order=0 queue_drops=0
```

## Architecture

```text
ScriptedAdc -> PeriodicSampler -> Q8.8 filter -> packet encoder
                                                  |
                                         8 bytes, CRC-16
                                                  v
consumer <- SpscQueue <- StreamReceiver <- byte stream
```

`src/sensor_hub.c` contains the portable C codec, CRC, filter, and simple queue. `src/stream_receiver.cpp` adds the C++ sampling boundary, stream parsing, and SPSC queue. `src/stream_demo.cpp` wires the host example together. `src/sensor_pipeline.cpp` is a smaller source-to-queue exercise using the same core.

### Wire format

Each packet is exactly eight bytes; multibyte fields are little-endian. The CRC covers bytes 0 through 5.

| Offset | Size | Field | Meaning |
| ---: | ---: | --- | --- |
| 0 | 1 | Sync | `0xA5` |
| 1 | 1 | Version | `0x01` |
| 2 | 2 | Sequence | Unsigned 16-bit counter; wraps to zero |
| 4 | 2 | Reading | Signed two's-complement Q8.8; value = raw / 256 |
| 6 | 2 | CRC | CRC-16/CCITT-FALSE, polynomial `0x1021`, init `0xFFFF`, no reflection or final XOR |

The CRC check value for the ASCII bytes `123456789` is `0x29B1`. It detects accidental corruption, not malicious modification.

## Design notes

The low-pass filter seeds its output from the first input and then applies `y += (x - y) / 4`. Integer division truncates toward zero; this is intentional and testable, though tiny changes can stall. `PeriodicSampler::tick` emits at most one sample when due and skips overdue intervals; it uses a caller-provided monotonic tick and does not handle tick wrap.

The stream receiver collects bytes after sync, checks the complete packet, and slides one byte on a bad frame to find another sync. It counts invalid frames, apparent missing sequence numbers, duplicates/backward numbers, accepted frames, and full-queue drops. It cannot distinguish sender omissions from transport loss, and false sync within corrupt data can cause extra rejections before recovery. Sequence gaps use modulo-16-bit arithmetic with a half-range rule.

The SPSC queue publishes slots with release/acquire atomic indices under **exactly one producer and one consumer**. The original C ring queue is sequential, not interrupt-safe. Actual ISR suitability depends on the target's atomic implementation and timing; this host project does not measure either.

## Tests and limits

`tests/test_sensor_hub.c` covers the CRC vector, codec errors/round trips, fixed-point boundaries, and ring wrap/full behavior. `tests/test_sensor_pipeline.cpp` checks source-to-queue behavior. `tests/test_stream_receiver.cpp` checks framing, loss, 16-bit sequence wrap, duplicates, pressure, and periodic sampling. `tests/fuzz_packets.cpp` feeds 100,000 reproducible arbitrary 8-byte cases through decode and the stream receiver; it is **not** coverage-guided fuzzing.

This is a host-side exercise, not firmware for a deployed device. It has no real ADC/UART driver, RTOS, target interrupt validation, worst-case execution-time measurement, industrial protocol, three-phase power algorithm, or safety certification.
