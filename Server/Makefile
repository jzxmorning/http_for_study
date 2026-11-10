CXX := g++
SERVER_DIR := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))
PROJECT_DIR := $(abspath $(SERVER_DIR)/..)
TARGET := $(SERVER_DIR)/http

CPPFLAGS += -MMD -MP
CXXFLAGS ?= -std=c++17 -O2 -g
CXXFLAGS += -pthread
LDLIBS += -lmysqlclient -ljsoncpp -pthread

SOURCES := $(SERVER_DIR)/http.cc
OBJECTS := $(SOURCES:.cc=.o)
DEPS := $(OBJECTS:.o=.d)

.PHONY: all clean test run

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(SERVER_DIR)/%.o: $(SERVER_DIR)/%.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@

clean:
	$(RM) $(TARGET) $(OBJECTS) $(DEPS)

test:
	CXX="$(CXX)" python3 "$(PROJECT_DIR)/tests/run_tests.py"

# 从项目根目录启动，确保 ./wwwroot 和 ./log 的相对路径正确。
run: $(TARGET)
	cd "$(PROJECT_DIR)" && "$(TARGET)"

-include $(DEPS)
