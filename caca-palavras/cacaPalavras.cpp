#include <iostream>
#include <vector>
#include <string>
#include <thread>    //gerenciamento de fluxos de execução paralelos
#include <mutex>     //exclusao mutua (condicao de corrida)
#include <cctype>
using namespace std;

//armazena o estado de cada busca e evita conflitos de escrita
struct Resultado {
    string palavra;
    bool encontrou = false;
    int linha, coluna;
    string direcao;
};

//variaveis enxergadas por todas threads
vector<string> matriz;
vector<Resultado> resultados;
mutex mtx; //garante a alteracao por apenas uma thread
int numeroLinha, numeroColuna;

//vetores de deslocamento (direcoes na matriz)
const int dx[] = {-1, 1, 0, 0, -1, -1, 1, 1};
const int dy[] = {0, 0, -1, 1, -1, 1, -1, 1};
const string nomeDirecao[] = {"cima", "baixo", "esquerda", "direita", "esquerda/cima", "direita/cima", "esquerda/baixo", "direita/baixo"};

//funcao de busca (executada por cada thread) com indice (uma palavra por thread = paralelismo)
void buscar(int indice) {
    //variaveis locais na pilha da thread (sem compartilhamento)
    string p = resultados[indice].palavra;
    int tamanho = p.length();
    //varre a matriz
    for (int i = 0; i < numeroLinha; ++i) {
        for (int j = 0; j < numeroColuna; ++j) {
            for (int d = 0; d < 8; ++d) {
                int k, x = i, y = j;
                //verifica se a palavra esta na direcao
                for (k = 0; k < tamanho; ++k) {
                    if (x < 0 || x >= numeroLinha || y < 0 || y >= numeroColuna) break; //fora dos limites da matriz
                    if ((char)tolower(matriz[x][y]) != (char)tolower(p[k])) break; //não esta na direcao
                    x += dx[d]; y += dy[d]; //esta na direcao = vai para a letra seguinte
                }
                //se k = tamanho da palavra atual ela foi encontrada
                if (k == tamanho) {
                    //lock_guard pega o mutex, se estiver livre a thread tranca o acesso e passa
                    //caso ja tenha uma, a thread fica com estado bloqueado até a atual terminar (concorrencia)
                    lock_guard<mutex> lock(mtx);
                    //guarda resultados da busca da thread
                    resultados[indice].encontrou = true;
                    resultados[indice].linha = i + 1;
                    resultados[indice].coluna = j + 1;
                    resultados[indice].direcao = nomeDirecao[d];
                    //destaque da palavra na matriz
                    int auxLinha = i, auxColuna = j;
                    for (int cont = 0; cont < tamanho; ++cont) {
                        matriz[auxLinha][auxColuna] = (char)toupper(matriz[auxLinha][auxColuna]);
                        auxLinha += dx[d]; auxColuna += dy[d];
                    }
                    return; //encerra o ciclo da thread
                }
            }
        }
    }
}

int main() {
    //leitura das dimensões
    cin >> numeroLinha >> numeroColuna;
    //redimensiona o vetor (menos desperdicio de memoria)
    matriz.resize(numeroLinha);
    for (int i = 0; i < numeroLinha; ++i) cin >> matriz[i];

    string p;
    while (cin >> p) {
        //armazena palavra no vetor
        Resultado r; r.palavra = p;
        resultados.push_back(r);
    }

    //cria thread
    vector<thread> t;
    for (int i = 0; i < (int)resultados.size(); ++i) {
        t.push_back(thread(buscar, i));
    }

    //bloqueia main() até que as threads finalizem
    for (auto& th : t) {
        th.join();
    }

    //matriz com palavras encontradas
    for (const auto& linha : matriz) {
        cout << linha << "\n";
    }
    cout << "\n";

    //relatório com resultados 
    for (const auto& r : resultados) {
        if (r.encontrou) 
            cout << r.palavra << " (" << r.linha << "," << r.coluna << "): " << r.direcao << "\n";
        else 
            cout << r.palavra << ": Não encontrado\n";
    }
    return 0;
}
