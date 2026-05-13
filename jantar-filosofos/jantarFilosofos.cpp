#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

//definicao dos 3 estados
#define FAMINTO  0
#define PENSANDO 1
#define COMENDO  2

//calculo dos vizinhos
#define ESQUERDA (id + qtdFilosofos - 1) % qtdFilosofos
#define DIREITA  (id + 1) % qtdFilosofos

//variaveis compartilhadas
int qtdFilosofos;
int *estado;
int *refeicoes;

//protege a regiao critica com exclusao mutua
pthread_mutex_t mutex;  
//bloqueia se nao houver garfos   
sem_t *semFilosofos; 
//tempo no formato [HH:MM:SS.mmm]     
struct timeval tempo; 

void mostraStatus(int id, const char *evento) {
    struct timeval tempoAtual;
    gettimeofday(&tempoAtual, NULL);
    
    long segundos = tempoAtual.tv_sec - tempo.tv_sec;
    long milisegundos = (tempoAtual.tv_usec - tempo.tv_usec) / 1000;
    if (milisegundos < 0) { segundos--; milisegundos += 1000; }

    int hh = segundos / 3600;
    int mm = (segundos % 3600) / 60;
    int ss = segundos % 60;

    printf("[%02d:%02d:%02d.%03ld] Filósofo %d %s\n", hh, mm, ss, milisegundos, id, evento);
}

void testar(int id) {
    //verifica se pode comer (sincronizacao)
    if (estado[id] == FAMINTO && estado[ESQUERDA] != COMENDO && estado[DIREITA] != COMENDO) {
        //comeca a refeicao se estiver FAMINTO sem vizinhos COMENDO
        estado[id] = COMENDO;
        refeicoes[id]++;
        mostraStatus(id, "está COMENDO");
        //libera o semaforo (desperta a thread)
        sem_post(&semFilosofos[id]);
    }
}

void pegaGarfos(int id) {
    //entra na regiao critica
    pthread_mutex_lock(&mutex); 
    //fica FAMINTO para tentar pegar garfos
    estado[id] = FAMINTO;
    mostraStatus(id, "está FAMINTO");
    testar(id);
    //sai da regiao critica
    pthread_mutex_unlock(&mutex);
    //bloqueia thread enquanto nao comer (sem sinal do sem_post)
    sem_wait(&semFilosofos[id]); 
}

void liberaGarfos(int id) {
    //entra na regiao critica
    pthread_mutex_lock(&mutex);
    //vai ao estado PENSANDO apos comer    
    estado[id] = PENSANDO;
    mostraStatus(id, "está PENSANDO");
    
    //tenta despertar threads vizinhas
    testar(ESQUERDA);
    testar(DIREITA);
    //sai da regiao critica
    pthread_mutex_unlock(&mutex);
}

void* filosofo(void* arg) {
    int id = *(int*)arg;
    while (1) {
        //simula tempo de pensamento
        usleep((rand() % 1000 + 500) * 1000); 
        //acaba de pensar e tenta pegar garfos (fica FAMINTO)
        pegaGarfos(id);
        //simula tempo de refeicao
        usleep((rand() % 1000 + 500) * 1000); 
        //libera garfos apos comer
        liberaGarfos(id);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printf("Inserir quantidade de filosofos: %s <numero_de_filosofos>\n", argv[0]);
        return 1;
    }

//converte quantidade inserida no terminal
    qtdFilosofos = atoi(argv[1]);
//vetor para estado
    estado = (int*)malloc(qtdFilosofos * sizeof(int));
//vetor para contador de refeicao
    refeicoes = (int*)malloc(qtdFilosofos * sizeof(int));
//vetor de semaforo
    semFilosofos = (sem_t*)malloc(qtdFilosofos * sizeof(sem_t));
//armazena id de cada thread
    pthread_t threads[qtdFilosofos];
//vetor de ids em valores númericos
    int ids[qtdFilosofos];
//tempo inicial do main para calculo do tmepo no log
    gettimeofday(&tempo, NULL);
//inicializa mutex
    pthread_mutex_init(&mutex, NULL);

    for (int i = 0; i < qtdFilosofos; i++) {
        estado[i] = PENSANDO;
        refeicoes[i] = 0;
        ids[i] = i;
        sem_init(&semFilosofos[i], 0, 0); // Inicializa semáforos em 0
    }

    // Criação das threads (Primitiva de SO)
    for (int i = 0; i < qtdFilosofos; i++) {
        pthread_create(&threads[i], NULL, filosofo, &ids[i]);
    }

    // Deixa rodar por um tempo determinado (ex: 10 segundos)
    sleep(10);

    printf("\n--- Fim da Simulação ---\n");
    for (int i = 0; i < qtdFilosofos; i++) {
        printf("Filósofo %d comeu %d vezes.\n", i, refeicoes[i]);
    }

    return 0;
}