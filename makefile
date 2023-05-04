CXX = g++
CXXFLAGS = -std=c++20 -g -Wall -Wextra -pedantic -pthread

SCANNERBOT_EXEC = bin/scannerbot
SCANNERBOT_SRCS = src/bus.cpp
SCANNERBOT_OBJS := $(SCANNERBOT_SRCS:.cpp=.o)

RECORDER_EXEC = bin/recorder
RECORDER_SRCS = src/recorder.cpp
RECORDER_OBJS := $(RECORDER_SRCS:.cpp=.o)

all: $(SCANNERBOT_EXEC) $(RECORDER_EXEC)
	@$(MAKE) clean

dirs:
	mkdir -p bin audio transcripts secret db

$(RECORDER_EXEC): $(RECORDER_OBJS) | dirs
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SCANNERBOT_EXEC): $(SCANNERBOT_OBJS) | dirs
	$(CXX) $(CXXFLAGS) -o $@ $^

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f src/*.o