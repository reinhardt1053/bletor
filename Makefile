# Compiler
CC = gcc

# Base compiler flags
CFLAGS = -Wall -Wextra -std=c23 -pedantic

# Debug compiler flags
DEBUG_CFLAGS = -g -O0 -DDEBUG

# Directories
SRC_DIR = .
BUILD_DIR = build
DEBUG_BUILD_DIR = build/debug

# Target executables
TARGET = bletor
DEBUG_TARGET = bletor_debug

# Source files
SRCS = bletor.c benc.c sha1.c 

# Header files
HDRS = benc.h sha1.h

# Object files
OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))
DEBUG_OBJS = $(patsubst %.c,$(DEBUG_BUILD_DIR)/%.o,$(SRCS))

# Create build directories
$(shell mkdir -p $(BUILD_DIR) $(DEBUG_BUILD_DIR))

# Default target (release build)
all: CFLAGS += -O2
all: $(BUILD_DIR)/$(TARGET)

# Debug target
debug: CFLAGS += $(DEBUG_CFLAGS)
debug: $(DEBUG_BUILD_DIR)/$(DEBUG_TARGET)

# Link release object files
$(BUILD_DIR)/$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Link debug object files
$(DEBUG_BUILD_DIR)/$(DEBUG_TARGET): $(DEBUG_OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile release source files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Compile debug source files
$(DEBUG_BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

# Install target
install: $(BUILD_DIR)/$(TARGET)
	cp $(BUILD_DIR)/$(TARGET) /usr/local/bin

# Clean up build files
clean:
	rm -rf $(BUILD_DIR)

# Phony targets
.PHONY: all debug clean install
