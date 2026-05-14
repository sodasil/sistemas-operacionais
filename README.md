# Sistemas Operacionais — Ambiente C/C++ para execução de trabalhos
Abra este repositório no **GitHub Codespaces**:

Para executar (exemplo):
- Compilar: `gcc ex0x.c -o ex0x` ou `make`
- Executar: `./ex0x`

Para consultar documentação de comandos linux:
- Manpages: `man 2 fork`, `man 2 execve`, `man 3 pthread_create`

Ferramentas disponíveis: gcc/g++, gdb, valgrind, strace, ltrace, cmake, make.


# caca-palavras

Procurar uma lista de palavras em um diagrama de letras 
e indicar se cada palavra foi encontrada ou não. 

Se a palavra foi encontrada, deve ser indicada a posição inicial e o sentido
(todos os sentidos são válidos).

# jantar-filosofos

Simular N filósofos dispostos em volta de uma mesa circular. Cada 
filósofo alterna entre três estados:

`PENSANDO` → `COM_FOME` → `COMENDO` → `PENSANDO` 

Para comer, um filósofo precisa obter os dois garfos adjacentes à sua posição. 

O programa deve rodar por um período de tempo pré-definido e exibir no terminal o estado do 
sistema e estatísticas de cada filósofo.

- Compilar: `g++ jantarFilosofos.cpp -o jantar -lpthread`
- Executar: `./jantar <numero_de_filosofos>`
  
# barbeiro-dorminhoco

Simular uma barbearia com 1 barbeiro e um número limitado de cadeiras de 
espera. Clientes chegam ao longo do tempo de forma contínua e tentam ser 
atendidos. Se não houver clientes, o barbeiro dorme. Se a sala de espera 
estiver cheia, clientes que chegam vão embora.
