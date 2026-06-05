CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Iinclude
LDFLAGS  := -lpthread

TARGET   := rasens
SRC_DIR  := src
BUILD_DIR := build/make

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(SRCS:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

INSTALL_BIN    := /opt/rasens/bin
INSTALL_CONFIG := /etc/rasens
INSTALL_SYSTEMD := /etc/systemd/system

.PHONY: all debug clean install uninstall

all: $(TARGET)

debug: CXXFLAGS += -g -DDEBUG
debug: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

install: $(TARGET)
	install -Dm755 $(TARGET) $(INSTALL_BIN)/$(TARGET)
	install -Dm644 config/rasens.conf $(INSTALL_CONFIG)/config.conf
	install -Dm644 systemd/rasens.service $(INSTALL_SYSTEMD)/rasens.service
	mkdir -p /var/log/rasens

uninstall:
	rm -f $(INSTALL_BIN)/$(TARGET)
	rm -f $(INSTALL_CONFIG)/config.conf
	rm -f $(INSTALL_SYSTEMD)/rasens.service

clean:
	rm -rf $(BUILD_DIR) $(TARGET)
