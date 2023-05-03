CXX = g++
CXXFLAGS = -std=c++20 -g -Wall -Wextra -pedantic -O3 -pthread

SCANNERBOT_EXEC = bin/scannerbot
SCANNERBOT_SRCS = src/bus.cpp
SCANNERBOT_OBJS := $(SCANNERBOT_SRCS:.cpp=.o)

RECORDER_EXEC = bin/recorder
RECORDER_SRCS = src/recorder.cpp
RECORDER_OBJS := $(RECORDER_SRCS:.cpp=.o)

all: $(SCANNERBOT_EXEC) $(RECORDER_EXEC)

$(RECORDER_EXEC): $(RECORDER_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(SCANNERBOT_EXEC): $(SCANNERBOT_OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

# Object files
obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY:
clean:
	rm -f $(SCANNERBOT_OBJS) $(RECORDER_OBJS) $(SCANNERBOT_EXEC) $(RECORDER_EXEC)