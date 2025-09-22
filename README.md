# INF01151 - Sistemas Operacionais - Trabalho

Este projeto implementa uma aplicação cliente-servidor para o trabalho da disciplina INF01151.

## Estrutura do Projeto

```
├── client/         # Código fonte do cliente
│   └── main.cpp
├── server/         # Código fonte do servidor
│   └── main.cpp
├── build/          # Executáveis compilados
├── Makefile        # Script de compilação
└── README.md       # Este arquivo
```

## Como Compilar e Executar

### 1. Compilação
Compile o projeto usando o Makefile:
```bash
make
```

### 2. Execução
Execute o servidor primeiro, depois o cliente:

**Terminal 1 - Servidor:**
```bash
./build/server
```

**Terminal 2 - Cliente:**
```bash
./build/client
```

## Limpeza
Para limpar os arquivos compilados:
```bash
make clean
```