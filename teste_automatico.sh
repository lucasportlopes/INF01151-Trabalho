#!/bin/bash

# ==============================================================================
#  SCRIPT DE TESTE AUTOMATIZADO - SISTEMA DISTRIBUÍDO (ALGORITMO DO VALENTÃO)
# ==============================================================================

echo "=================================================="
echo "  PASSO 1: LIMPEZA E PREPARAÇÃO"
echo "=================================================="

# 1. Mata processos antigos para liberar as portas
pkill -f "servidor"
pkill -f "cliente"

# 2. Compila o projeto
echo "[SCRIPT] Compilando o projeto..."
make clean > /dev/null 2>&1
make

# Verifica se a compilação deu certo
if [ $? -ne 0 ]; then
    echo " [ERRO] O código não compilou! Verifique os erros acima."
    exit 1
fi
echo " [SUCESSO] Compilação concluída."
echo ""

echo "=================================================="
echo " PASSO 2: INICIANDO SERVIDORES"
echo "=================================================="

# Verifica onde está o executável (na pasta build ou na raiz)
if [ -f "./build/servidor" ]; then
    BIN="./build/servidor"
elif [ -f "./servidor" ]; then
    BIN="./servidor"
else
    echo " [ERRO] Executável 'servidor' não encontrado!"
    exit 1
fi

# Inicia LÍDER (Porta 4000, ID 1)
$BIN 4000 3 &
LIDER_PID=$!
sleep 3

# Inicia BACKUP 1 (Porta 4001, ID 2)
$BIN 4001 2 &
sleep 3

# Inicia BACKUP 2 (Porta 4002, ID 3)
$BIN 4002 1 &
sleep 3

sleep 5
echo "=================================================="
echo "  PASSO 3: O TESTE DE FOGO (MATAR O LÍDER)"
echo "=================================================="

kill -9 $LIDER_PID
echo " [SCRIPT] Líder morto."
echo ""
echo "--------------------------------------------------"
echo "   OLHE ABAIXO! Você deve ver:"
echo "   1. [FALHA] Timeout! O Líder sumiu."
echo "   2. [ELEICAO] Iniciando eleição..."
echo "   3. [ELEICAO] VITORIA! Eu (ID 3) sou o novo Lider."
echo "--------------------------------------------------"

# Espera tempo suficiente para o timeout (5s) + eleição
sleep 10

echo ""
echo "=================================================="
echo "  TESTE FINALIZADO"
echo "=================================================="
echo "Pressione [ENTER] para encerrar todos os processos e sair."
read

# Limpeza final
pkill -f "servidor"
echo "[SCRIPT] Tudo limpo. Teste concluído."