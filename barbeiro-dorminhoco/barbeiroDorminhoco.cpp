#include <iostream>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <chrono>
#include <queue>
#include <vector>
#include <random>

using namespace std;


struct Cliente {
    int id;
    string estado;
};

// Mutex para proteger região crítica
pthread_mutex_t mutex_fila;
// Semaforo para controlar o atendimento de clientes
sem_t sem_clientes;

queue<Cliente> fila_clientes;

bool simulacao_ativa = true;

int cadeiras_totais;

int clientes_atendidos = 0;
int clientes_desistentes = 0;

string estado_barbeiro = "DORME";

auto inicio_simulacao = chrono::steady_clock::now();


string timestamp() {

    auto agora = chrono::steady_clock::now();

    auto tempo =
        chrono::duration_cast<chrono::milliseconds>(
            agora - inicio_simulacao
        ).count();

    int horas = tempo / 3600000;
    tempo %= 3600000;

    int minutos = tempo / 60000;
    tempo %= 60000;

    int segundos = tempo / 1000;
    tempo %= 1000;

    char buffer[30];

    sprintf(
        buffer,
        "[%02d:%02d:%02d.%03ld]",
        horas,
        minutos,
        segundos,
        tempo
    );

    return string(buffer);
}

string desenhar_fila() {

    string s = "[";

    int ocupadas = fila_clientes.size();

    for (int i = 0; i < cadeiras_totais; i++) {

        if (i < ocupadas)
            s += "#";
        else
            s += ".";
    }

    s += "]";

    return s;
}

string listar_fila() {

    queue<Cliente> copia = fila_clientes;

    string s;

    while (!copia.empty()) {

        Cliente c = copia.front();

        s += to_string(c.id)
           + ":" + c.estado + " ";

        copia.pop();
    }

    return s;
}

void log_evento(string evento) {

    cout << timestamp()
         << " "
         << evento
         << endl;

    cout << "Barbeiro: "
         << estado_barbeiro
         << endl;

    cout << "Fila: "
         << desenhar_fila()
         << " ("
         << fila_clientes.size()
         << "/"
         << cadeiras_totais
         << ") -> "
         << listar_fila()
         << endl;

    cout << "Contadores: "
         << "atendidos="
         << clientes_atendidos
         << " | desistentes="
         << clientes_desistentes
         << " | em_espera="
         << fila_clientes.size()
         << endl;

    cout << "------------------------------------------------------------"
         << endl;
}

//Thread do barbeiro
struct ParametrosBarbeiro {

    int tempo_min;
    int tempo_max;
};

void* barbeiro_func(void* arg) {

    ParametrosBarbeiro* p =
        (ParametrosBarbeiro*) arg;

    default_random_engine gerador(
        chrono::system_clock::now()
        .time_since_epoch()
        .count()
    );

    uniform_int_distribution<int>
        tempo_corte(
            p->tempo_min,
            p->tempo_max
        );

    while (simulacao_ativa || !fila_clientes.empty()) {

        estado_barbeiro = "DORME";

        // Fica a espera de clientes
        sem_wait(&sem_clientes);

        pthread_mutex_lock(&mutex_fila);

        if (!simulacao_ativa &&
            fila_clientes.empty()) {

            pthread_mutex_unlock(&mutex_fila);

            break;
        }

        if (fila_clientes.empty()) {

            pthread_mutex_unlock(&mutex_fila);

            continue;
        }

        Cliente cliente =
            fila_clientes.front();

        fila_clientes.pop();

        estado_barbeiro =
            "ATENDE";
        cliente.estado = "ATENDIDO";

        log_evento(
            "Cliente "
            + to_string(cliente.id)
            + ": AGUARDA -> ATENDIDO"
        );

        pthread_mutex_unlock(&mutex_fila);

        // Simula atendimento
        usleep(
            tempo_corte(gerador) * 1000
        );

        pthread_mutex_lock(&mutex_fila);

        clientes_atendidos++;

        log_evento(
            "Cliente "
            + to_string(cliente.id)
            + " foi atendido"
        );

        pthread_mutex_unlock(&mutex_fila);
    }

    pthread_exit(NULL);
}

// Thread do cliente
void* cliente_func(void* arg) {

    int id_cliente = *((int*) arg);

    delete (int*) arg;

    pthread_mutex_lock(&mutex_fila);

    Cliente cliente;

    cliente.id = id_cliente;
    cliente.estado = "ENTRA";

    log_evento(
        "Cliente "
        + to_string(cliente.id)
        + ": ENTRA"
    );

    // Verifica se existe espaço na fila
    if ((int)fila_clientes.size()
        < cadeiras_totais) {

        //Caso tenha espaço na fila
        cliente.estado = "AGUARDA";

        fila_clientes.push(cliente);

        log_evento(
            "Cliente "
            + to_string(cliente.id)
            + ": ENTRA -> AGUARDA"
        );

        // Alerta o barbeiro
        sem_post(&sem_clientes);
    }
    else {

        //Caso não tenha
        cliente.estado = "DESISTE";
        clientes_desistentes++;
        log_evento(
            "Cliente "
            + to_string(cliente.id)
            + ": ENTRA -> DESISTE"
        );
    }

    pthread_mutex_unlock(&mutex_fila);

    pthread_exit(NULL);
}

//Main

int main() {

    int chegada_min;
    int chegada_max;

    int corte_min;
    int corte_max;

    int duracao;

    cout << "=========== BARBEIRO DORMINHOCO ===========" << endl;

    cout << "Numero de cadeiras: ";
    cin >> cadeiras_totais;

    cout << "Intervalo chegada clientes (ms) [min max]: ";
    cin >> chegada_min >> chegada_max;

    cout << "Intervalo atendimento (ms) [min max]: ";
    cin >> corte_min >> corte_max;

    cout << "Duracao simulacao (segundos): ";
    cin >> duracao;

    cout << endl;
    cout << "Simulacao iniciada..." << endl;
    cout << endl;

    // Inicializa mutex
    pthread_mutex_init(
        &mutex_fila,
        NULL
    );

    // Inicializa semáforo
    sem_init(
        &sem_clientes,
        0,
        0
    );

    ParametrosBarbeiro params;

    params.tempo_min = corte_min;
    params.tempo_max = corte_max;

    // Cria a thread do Barbeiro
    pthread_t barbeiro;

    pthread_create(
        &barbeiro,
        NULL,
        barbeiro_func,
        &params
    );

    vector<pthread_t> clientes;

    default_random_engine gerador(
        chrono::system_clock::now()
        .time_since_epoch()
        .count()
    );

    uniform_int_distribution<int>
        tempo_chegada(
            chegada_min,
            chegada_max
        );

    auto inicio = chrono::steady_clock::now();

    int id_cliente = 1;

    // Gera clientes até o fim da simulação
    while (

        chrono::duration_cast<chrono::seconds>(
            chrono::steady_clock::now()
            - inicio
        ).count() < duracao

    ) {

        usleep(
            tempo_chegada(gerador) * 1000
        );
        pthread_t cliente;
        int* id = new int(id_cliente++);
        pthread_create(
            &cliente,
            NULL,
            cliente_func,
            id
        );
        clientes.push_back(cliente);
    }

    // Encerra simulação
    simulacao_ativa = false;

    // Acorda barbeiro caso esteja dormindo para evitar espera infinita
    sem_post(&sem_clientes);

    // Espera clientes finalizarem para dar continuidade ao codigo no main
    for (pthread_t& t : clientes) {

        pthread_join(t, NULL);
    }

    // Espera barbeiro finalizar para dar continuidade ao codigo do main
    pthread_join(barbeiro, NULL);

    // Libera recursos
    pthread_mutex_destroy(&mutex_fila);

    sem_destroy(&sem_clientes);

    //Relatório Final
    cout << endl;

    cout << "=========== RESUMO FINAL ===========" << endl;

    cout << "Clientes atendidos: "
         << clientes_atendidos
         << endl;

    cout << "Clientes desistentes: "
         << clientes_desistentes
         << endl;

    cout << "Clientes restantes na fila: "
         << fila_clientes.size()
         << endl;

    return 0;
}