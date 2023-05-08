CXX = g++
CXXFLAGS = -std=c++20 -g -Wall -Wextra -pedantic 
LIB_FLAGS = -lpthread -ldl

SCANNERBOT_EXEC = bin/scannerbot
SCANNERBOT_CPP_SRCS = src/bus.cpp 
SCANNERBOT_CPP_OBJS := $(SCANNERBOT_CPP_SRCS:.cpp=.o)
SCANNERBOT_C_SRCS = src/sqlite3.c 
SCANNERBOT_C_OBJS := $(SCANNERBOT_C_SRCS:.c=.o)


RECORDER_EXEC = bin/recorder
RECORDER_SRCS = src/recorder.cpp
RECORDER_OBJS := $(RECORDER_SRCS:.cpp=.o)

all: $(SCANNERBOT_EXEC) $(RECORDER_EXEC)
	@$(MAKE) clean

dirs:
	mkdir -p bin audio transcripts secret db

$(RECORDER_EXEC): $(RECORDER_OBJS) | dirs
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB_FLAGS)

$(SCANNERBOT_EXEC): $(SCANNERBOT_CPP_OBJS) $(SCANNERBOT_C_OBJS) | dirs
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LIB_FLAGS)

obj/%.o: src/%.c
	$(CXX) $(CXXFLAGS) -c $< -o $@

obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

.PHONY: clean
clean:
	rm -f src/*.o