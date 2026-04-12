#!/bin/bash

echo "Compilando código..."

g++ -pthread cacaPalavras.cpp -o cacaPalavras

if [ $? -eq 0 ]; then
    echo "Compilação concluída."
else
    echo "Erro na compilação. Verifique o código."
    exit 1
fi

echo ""
echo "1) Rodar 'cacapalavras.txt'"
echo "2) Rodar com outro arquivo (digitar nome)"
echo "3) Sair"
printf "Escolha uma opção: "
read opcao

case $opcao in
    1)
        ./cacaPalavras < cacapalavras.txt > resultado.txt
        echo "Executado com cacapalavras.txt"
        ;;
    2)
        printf "Nome do arquivo: "
        read arquivo
        if [ -f "$arquivo" ]; then
            ./cacaPalavras < "$arquivo" > resultado.txt
            echo "Executado com '$arquivo'"
        else
            echo "Erro: '$arquivo' não encontrado!"
        fi
        ;;
    3)
        exit 0
        ;;
    *)
        echo "Opção inválida."
        ;;
esac