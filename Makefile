CXX = g++
CXXFLAGS = -std=c++20 -O3 -Wall -Wextra $(shell pkg-config --cflags sdl3 xcursor) -Isrc -MMD -MP
LDFLAGS = $(shell pkg-config --libs sdl3 xcursor) -lm

TARGET = physics_cursor
SRCS = src/main.cpp src/Font8x8.cpp
OBJS = $(SRCS:.cpp=.o)

-include $(OBJS:.o=.d)

.DEFAULT_GOAL := all
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET)

run: $(TARGET)
	/lib64/ld-linux-x86-64.so.2 ./$(TARGET)

run-daemon: $(TARGET)
	/lib64/ld-linux-x86-64.so.2 ./$(TARGET) --daemon

run-overlay: $(TARGET)
	/lib64/ld-linux-x86-64.so.2 ./$(TARGET) --overlay

.PHONY: all clean run run-daemon run-overlay

TEST_BINARY = build/physics_tests
$(TEST_BINARY): tests/physics_tests.cpp src/PhysicsEngine.hpp src/PhysicsDefaults.hpp src/BuildAction.hpp src/CursorEffects.hpp src/CursorTransition.hpp
	@mkdir -p build
	$(CXX) -std=c++20 -O2 -Wall -Wextra -Isrc $< -o $@

test: $(TEST_BINARY)
	/lib64/ld-linux-x86-64.so.2 ./$(TEST_BINARY)

.PHONY: test
