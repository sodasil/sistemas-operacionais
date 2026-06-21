# Manipulador FAT16

## Objetivo
Implementar um programa em C++ para manipular uma imagem de disco FAT16.

## Regras do trabalho
- Um único arquivo fonte para a aplicação localizado em src/main.cpp.
- Somente diretório raiz.
- Sem subdiretórios.
- Operações obrigatórias:
  - listar arquivos
  - ler conteúdo
  - mostrar atributos
  - renomear
  - criar arquivo
  - remover arquivo

## Ambiente
A execução final deve funcionar no GitHub Codespaces.

## Como compilar
g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp -o fat16

## Como executar
./fat16