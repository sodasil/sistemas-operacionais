#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <queue>
#include <vector>
#include <random>
#include <iomanip>

using namespace std;

// ===================== ESTRUTURAS GLOBAIS =====================

mutex mtx;
condition_variable cv_barbeiro;

bool simulacao_ativa = true;

// Fila de espera
queue<int> fila_clientes;

int cadeiras_totais;
int clientes_atendidos = 0;
int clientes_desistentes = 0;

string estado_barbeiro = "DORME";

// Tempo inicial da simulação
auto inicio_simulacao = chrono::steady_clock::now();

// ===================== FUNÇÕES AUXILIARES =====================

// Timestamp [HH:MM:SS.mmm]
string timestamp() {
    auto agora = chrono::steady_clock::now();

    auto tempo =
        chrono::duration_cast<chrono::milliseconds>(
            agora - inicio_simulacao).count();

    int horas = tempo / 3600000;
    tempo %= 3600000;

    int minutos = tempo / 60000;
    tempo %= 60000;

    int segundos = tempo / 1000;
    tempo %= 1000;

    char buffer[30];

    sprintf(
        buffer,
        "[%02d:%02d:%02d.%03lld]",
        horas,
        minutos,
        segundos,
        tempo
    );

    return string(buffer);
}

// Mostrar fila graficamente
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

// Mostrar IDs na fila
string listar_fila() {
    queue<int> copia = fila_clientes;

    string s;

    while (!copia.empty()) {
        s += "C" + to_string(copia.front()) + " ";
        copia.pop();
    }

    return s;
}

// Log completo do sistema
void log_evento(string evento) {

    cout << timestamp() << " " << evento << endl;

    cout << "Barbeiro: " << estado_barbeiro << endl;

    cout << "Fila: "
         << desenhar_fila()
         << " (" << fila_clientes.size()
         << "/" << cadeiras_totais << ") -> "
         << listar_fila()
         << endl;

    cout << "Contadores: "
         << "atendidos=" << clientes_atendidos
         << " | desistentes=" << clientes_desistentes
         << " | em_espera=" << fila_clientes.size()
         << endl;

    cout << "------------------------------------------------------------"
         << endl;
}

// ===================== THREAD DO BARBEIRO =====================

void barbeiro_func(int tmin, int tmax) {

    default_random_engine gerador(
        chrono::system_clock::now()
        .time_since_epoch()
        .count()
    );

    uniform_int_distribution<int> tempo_corte(tmin, tmax);

    while (simulacao_ativa || !fila_clientes.empty()) {

        unique_lock<mutex> lock(mtx);

        // Dorme se não há clientes
        while (fila_clientes.empty() && simulacao_ativa) {

            estado_barbeiro = "DORME";

            cv_barbeiro.wait(lock);
        }

        // Encerrar corretamente
        if (!simulacao_ativa && fila_clientes.empty())
            break;

        // Próximo cliente
        int cliente = fila_clientes.front();
        fila_clientes.pop();

        estado_barbeiro =
            "ATENDE C" + to_string(cliente);

        log_evento(
            "Barbeiro iniciou atendimento do cliente C"
            + to_string(cliente)
        );

        lock.unlock();

        // Simula corte
        this_thread::sleep_for(
            chrono::milliseconds(
                tempo_corte(gerador)
            )
        );

        lock.lock();

        clientes_atendidos++;

        log_evento(
            "Barbeiro concluiu atendimento do cliente C"
            + to_string(cliente)
        );
    }

    estado_barbeiro = "DORME";
}

// ===================== THREAD DO CLIENTE =====================

void cliente_func(int id_cliente) {

    unique_lock<mutex> lock(mtx);

    // Existe cadeira?
    if ((int)fila_clientes.size() < cadeiras_totais) {

        fila_clientes.push(id_cliente);

        log_evento(
            "Cliente C" + to_string(id_cliente)
            + " chegou e entrou na fila"
        );

        // Acorda barbeiro
        cv_barbeiro.notify_one();
    }
    else {

        clientes_desistentes++;

        log_evento(
            "Cliente C" + to_string(id_cliente)
            + " chegou, mas desistiu por falta de cadeira"
        );
    }
}

// ===================== MAIN =====================

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

    // Thread barbeiro
    thread barbeiro(
        barbeiro_func,
        corte_min,
        corte_max
    );

    vector<thread> clientes;

    default_random_engine gerador(
        chrono::system_clock::now()
        .time_since_epoch()
        .count()
    );

    uniform_int_distribution<int>
        tempo_chegada(chegada_min, chegada_max);

    auto inicio = chrono::steady_clock::now();

    int id_cliente = 1;

    // Gerador contínuo de clientes
    while (
        chrono::duration_cast<chrono::seconds>(
            chrono::steady_clock::now() - inicio
        ).count() < duracao
    ) {

        this_thread::sleep_for(
            chrono::milliseconds(
                tempo_chegada(gerador)
            )
        );

        clientes.push_back(
            thread(cliente_func, id_cliente++)
        );
    }

    // Finalização
    simulacao_ativa = false;

    cv_barbeiro.notify_all();

    // Espera clientes
    for (auto &t : clientes) {
        if (t.joinable())
            t.join();
    }

    // Espera barbeiro
    barbeiro.join();

    // ===================== RESUMO FINAL =====================

    cout << endl;
    cout << "=========== RESUMO FINAL ===========" << endl;

    cout << "Clientes atendidos: "
         << clientes_atendidos << endl;

    cout << "Clientes desistentes: "
         << clientes_desistentes << endl;

    cout << "Clientes restantes na fila: "
         << fila_clientes.size() << endl;

    return 0;
}