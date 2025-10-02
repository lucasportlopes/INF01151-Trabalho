# INF01151 - Sistemas Operacionais - Trabalho

Este projeto implementa uma aplicação cliente-servidor para o trabalho da disciplina INF01151.

## Estrutura do Projeto

```
├── client/         # Código fonte do cliente
│   └── client.cpp
├── server/         # Código fonte do servidor
│   └── server.cpp
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
./build/servidor 4000
```
**Terminal 2 - Cliente:**
```bash
./build/cliente 4000
```

Ou com make
**Terminal 1 - Servidor:**
```bash
make run-server
```
**Terminal 2 - Cliente:**
```bash
make run-client
```

Para rodar o servidor em background e o cliente em foreground:
```bash
make run
```

## Limpeza
Para limpar os arquivos compilados:
```bash
make clean
```
