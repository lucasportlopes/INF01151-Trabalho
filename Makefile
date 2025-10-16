CC = gcc
CFLAGS = -Wall -Wextra -g -pthread

# Directories
SERVER_DIR = server
CLIENT_DIR = client
BUILD_DIR  = build

# Sources
SERVER_SRCS = $(SERVER_DIR)/server.c $(SERVER_DIR)/interface.c $(SERVER_DIR)/discovery.c $(SERVER_DIR)/processing.c
CLIENT_SRCS = $(CLIENT_DIR)/client.c $(CLIENT_DIR)/interface.c $(CLIENT_DIR)/discovery.c $(CLIENT_DIR)/processing.c

# Targets (binaries go inside build/)
SERVER_BIN = $(BUILD_DIR)/servidor
CLIENT_BIN = $(BUILD_DIR)/cliente

all: $(SERVER_BIN) $(CLIENT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SERVER_BIN): $(SERVER_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

$(CLIENT_BIN): $(CLIENT_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $^ -o $@

run-server: $(SERVER_BIN)
	./$(SERVER_BIN) 4000

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN) 4000

run: all
	./$(SERVER_BIN) 4000 & \
	./$(CLIENT_BIN) 4000

clean:
	rm -rf $(BUILD_DIR)
