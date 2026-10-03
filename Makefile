CXX = g++
CXXFLAGS = -std=c++20 -O3 -Wall -Wextra $(shell pkg-config --cflags sdl3 xcursor) -Isrc
LDFLAGS = $(shell pkg-config --libs sdl3 xcursor) -lm

TARGET = physics_cursor
SRCS = src/main.cpp src/Font8x8.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

run: $(TARGET)
	/lib64/ld-linux-x86-64.so.2 ./$(TARGET)

run-daemon: $(TARGET)
	/lib64/ld-linux-x86-64.so.2 ./$(TARGET) --daemon

run-overlay: $(TARGET)
	/lib64/ld-linux-x86-64.so.2 ./$(TARGET) --overlay

.PHONY: all clean run run-daemon run-overlay
