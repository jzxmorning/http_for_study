CXX := g++
TARGET := http

CPPFLAGS += -MMD -MP
CXXFLAGS ?= -std=c++17 -O2 -g
CXXFLAGS += -pthread
LDLIBS += -lmysqlclient -ljsoncpp -pthread

SOURCES := http.cc
OBJECTS := $(SOURCES:.cc=.o)
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(TARGET) $(OBJECTS) $(DEPS)

test:
	CXX="$(CXX)" python3 tests/run_tests.py

-include $(DEPS)
