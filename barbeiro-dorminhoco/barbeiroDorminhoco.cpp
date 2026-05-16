#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <vector>
#include <string>
#include <iomanip>
#include <random>

using namespace std;

// Estruturas de Sincronização
mutex mtx;
condition_variable cv_barbeiro; // Barbeiro espera cliente
condition_variable cv_cliente;  // Cliente espera corte
bool simulacao_ativa = true;

// Variáveis de Estado
int cadeiras_disponiveis;
int cadeiras_totais;
int clientes_atendidos = 0;
int clientes_desistentes = 0;
string estado_barbeiro = "DORME";

// Tempos da simulação
auto inicio_simulacao = chrono::steady_clock::now();

// Função auxiliar para o tempo formatado [HH:MM:SS.mmm]
string get_timestamp() {
    auto agora = chrono::steady_clock::now();
    auto diff = chrono::duration_cast<chrono::milliseconds>(agora - inicio_simulacao);
    
    long long ms = diff.count();
    int horas = ms / 3600000;
    ms %= 3600000;
    int minutos = ms / 60000;
    ms %= 60000;
    int segundos = ms / 1000;
    ms %= 1000;

    char buf[20];
    sprintf(buf, "[%02d:%02d:%02d.%03lld]", horas, minutos, segundos, ms);
    return string(buf);
}

void log(string evento) {
    lock_guard<mutex> trava(mtx);
    cout << get_timestamp() << " | " << setw(20) << left << evento 
         << " | Barbeiro: " << setw(7) << estado_barbeiro 
         << " | Fila: " << (cadeiras_totais - cadeiras_disponiveis) << "/" << cadeiras_totais
         << " | Atendidos: " << clientes_atendidos 
         << " | Desistências: " << clientes_desistentes << endl;
}

// Thread do Barbeiro
void barbeiro_func(int min_atend, int max_atend) {
    default_random_engine gerador(chrono::system_clock::now().time_since_epoch().count());
    uniform_int_distribution<int> distr(min_atend, max_atend);

    while (simulacao_ativa) {
        unique_lock<mutex> trava(mtx);
        
        while (cadeiras_disponiveis == cadeiras_totais && simulacao_ativa) {
            estado_barbeiro = "DORME";
            cv_barbeiro.wait_for(trava, chrono::milliseconds(100)); 
        }

        if (!simulacao_ativa) break;

        // Atender Cliente
        estado_barbeiro = "ATENDE";
        cadeiras_disponiveis++; // Libera cadeira
        trava.unlock();

        log("ATENDENDO CLIENTE");
        this_thread::sleep_for(chrono::milliseconds(distr(gerador)));
        
        trava.lock();
        clientes_atendidos++;
        log("CORTE FINALIZADO");
        trava.unlock();
        
        cv_cliente.notify_one(); // Avisa o cliente que acabou
    }
}

// Thread do Cliente
void cliente_func() {
    unique_lock<mutex> trava(mtx);
    log("ENTRA");

    if (cadeiras_disponiveis > 0) {
        cadeiras_disponiveis--;
        log("AGUARDA");
        
        cv_barbeiro.notify_one(); // Acorda o barbeiro se necessário
        cv_cliente.wait(trava);   // Espera o corte terminar
        log("ATENDIDO");
    } else {
        clientes_desistentes++;
        log("DESISTE (Fila Cheia)");
    }
}

int main() {
    int num_cadeiras, t_chegada_min, t_chegada_max, t_atend_min, t_atend_max, duracao;

    cout << "--- Configuracao da Barbearia ---\n";
    cout << "Numero de cadeiras de espera: "; cin >> num_cadeiras;
    cout << "Intervalo de chegada de clientes (ms) [min max]: "; cin >> t_chegada_min >> t_chegada_max;
    cout << "Intervalo de tempo de corte (ms) [min max]: "; cin >> t_atend_min >> t_atend_max;
    cout << "Duracao total da simulacao (segundos): "; cin >> duracao;

    cadeiras_totais = num_cadeiras;
    cadeiras_disponiveis = num_cadeiras;

    cout << "\nIniciando simulacao...\n";
    cout << "Tempo           | Evento               | Barbeiro | Fila | Stats\n";
    cout << "--------------------------------------------------------------------------------\n";

    thread t_barbeiro(barbeiro_func, t_atend_min, t_atend_max);
    
    auto inicio = chrono::steady_clock::now();
    default_random_engine gerador(chrono::system_clock::now().time_since_epoch().count());
    uniform_int_distribution<int> distr_chegada(t_chegada_min, t_chegada_max);

    vector<thread> clientes;

    while (chrono::duration_cast<chrono::seconds>(chrono::steady_clock::now() - inicio).count() < duracao) {
        this_thread::sleep_for(chrono::milliseconds(distr_chegada(gerador)));
        clientes.push_back(thread(cliente_func));
    }

    simulacao_ativa = false;
    cv_barbeiro.notify_all();
    t_barbeiro.join();

    for (auto& t : clientes) {
        if (t.joinable()) t.join();
    }

    cout << "\nSimulacao Encerrada.\n";
    return 0;
}