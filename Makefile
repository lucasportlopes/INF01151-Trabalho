# Nome dos compiladores e flags
CXX = g++
CXXFLAGS = -Wall -O2

# Estrutura de diretórios
SRC_DIR_SERVER = server
SRC_DIR_CLIENT = client
BUILD_DIR = build

# Arquivos fonte
SERVER_SRC = $(SRC_DIR_SERVER)/main.cpp $(SRC_DIR_SERVER)/server.cpp
CLIENT_SRC = $(SRC_DIR_CLIENT)/main.cpp $(SRC_DIR_CLIENT)/client.cpp

# Executáveis
SERVER_BIN = $(BUILD_DIR)/server
CLIENT_BIN = $(BUILD_DIR)/client

# Alvo padrão: compila tudo
all: $(SERVER_BIN) $(CLIENT_BIN)

# Compilação do servidor
$(SERVER_BIN): $(SERVER_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(SERVER_SRC)

# Compilação do cliente
$(CLIENT_BIN): $(CLIENT_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -o $@ $(CLIENT_SRC)

# Criar diretório de build se não existir
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Limpar os arquivos compilados
clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean
