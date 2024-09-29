# Variables
CC = gcc
CFLAGS = -Wall -Wextra -Werror
TARGET = code_analyzer
SCRIPT = analyze.sh
INSTALL_DIR = /usr/local/bin

# Default target
all: $(TARGET)

# Compile the C program
$(TARGET): code_analyzer.c
	$(CC) $(CFLAGS) -o $(TARGET) code_analyzer.c

# Install the script and binary
install: $(TARGET) $(SCRIPT)
	sudo cp $(TARGET) $(INSTALL_DIR)/$(TARGET)
	sudo cp $(SCRIPT) $(INSTALL_DIR)/$(SCRIPT)
	sudo chmod +x $(INSTALL_DIR)/$(SCRIPT)

# Clean up the binary
clean:
	rm -f $(TARGET)

# Phony targets
.PHONY: all install clean
