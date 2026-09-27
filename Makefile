CC ?= cc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -pedantic
CPPFLAGS += -Iinclude
BUILD := build
.PHONY: all test clean sanitize
all: $(BUILD)/sensor_hub $(BUILD)/test_sensor_hub $(BUILD)/test_sensor_pipeline $(BUILD)/test_stream_receiver $(BUILD)/fuzz_packets $(BUILD)/stream_demo
$(BUILD):
	mkdir -p $(BUILD)
$(BUILD)/sensor_hub: src/main.c src/sensor_hub.c include/sensor_hub.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) src/main.c src/sensor_hub.c -o $@
$(BUILD)/test_sensor_hub: tests/test_sensor_hub.c src/sensor_hub.c include/sensor_hub.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) tests/test_sensor_hub.c src/sensor_hub.c -o $@
test: $(BUILD)/test_sensor_hub $(BUILD)/test_sensor_pipeline $(BUILD)/test_stream_receiver $(BUILD)/fuzz_packets
	./$(BUILD)/test_sensor_hub
	./$(BUILD)/test_sensor_pipeline
	./$(BUILD)/test_stream_receiver
	./$(BUILD)/fuzz_packets
sanitize:
	$(MAKE) clean
	$(MAKE) CFLAGS='-std=c11 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer' CXXFLAGS='-std=c++17 -O1 -g -Wall -Wextra -Werror -pedantic -fsanitize=address,undefined -fno-omit-frame-pointer' test
clean:
	rm -rf $(BUILD)

CXX ?= c++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror -pedantic
$(BUILD)/sensor_hub.o: src/sensor_hub.c include/sensor_hub.h | $(BUILD)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c src/sensor_hub.c -o $@
$(BUILD)/test_sensor_pipeline: tests/test_sensor_pipeline.cpp src/sensor_pipeline.cpp include/sensor_pipeline.hpp $(BUILD)/sensor_hub.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_sensor_pipeline.cpp src/sensor_pipeline.cpp $(BUILD)/sensor_hub.o -o $@

$(BUILD)/test_stream_receiver: tests/test_stream_receiver.cpp src/stream_receiver.cpp include/stream_receiver.hpp $(BUILD)/sensor_hub.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/test_stream_receiver.cpp src/stream_receiver.cpp $(BUILD)/sensor_hub.o -o $@
$(BUILD)/fuzz_packets: tests/fuzz_packets.cpp src/stream_receiver.cpp include/stream_receiver.hpp $(BUILD)/sensor_hub.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) tests/fuzz_packets.cpp src/stream_receiver.cpp $(BUILD)/sensor_hub.o -o $@
$(BUILD)/stream_demo: src/stream_demo.cpp src/stream_receiver.cpp include/stream_receiver.hpp $(BUILD)/sensor_hub.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) src/stream_demo.cpp src/stream_receiver.cpp $(BUILD)/sensor_hub.o -o $@
