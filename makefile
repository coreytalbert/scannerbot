CXX=g++
CXXFLAGS=-std=c++20 -g -Wall -Wextra -pedantic -O3 -pthread

# File names
EXEC 	 = bin/scannerbot
SOURCES := $(wildcard src/*.cpp)
OBJECTS := $(SOURCES:.c=.o)

# Target executable
$(EXEC): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $(EXEC)

# Object files
obj/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean
clean:
	rm -f $(OBJECTS) $(EXEC)