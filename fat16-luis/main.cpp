/*
 * Manipulador de Imagem FAT16 - Sistema de Arquivos em Disco
 * 
 * OBJETIVO:
 * Este programa implementa um gerenciador de arquivos que trabalha diretamente
 * com uma imagem de disco FAT16, sem usar bibliotecas prontas de FAT.
 * 
 * POR QUE FAT16?
 * FAT16 é um sistema de arquivos antigo mas ainda pedagógico. Sua estrutura
 * é simples o bastante para ser implementada manualmente, mas complexa o
 * suficiente para ensinar conceitos reais de sistemas de arquivos como:
 * - Boot Sector (metadados do volume)
 * - File Allocation Table (mapa de clusters)
 * - Root Directory (listagem de arquivos)
 * - Data Region (local onde os dados realmente estão)
 * 
 * COMO FUNCIONA:
 * 1. Lê a imagem do disco inteira na memória como vetor de bytes
 * 2. Parse o Boot Sector para extrair configuração do volume
 * 3. Calcula posições de FAT, diretório raiz e dados usando fórmulas
 * 4. Para cada operação, localiza dados pela cadeia de clusters
 * 5. Atualiza FAT e diretório quando há modificações
 * 6. Salva a imagem modificada de volta ao disco
 */

#include <algorithm>  // Para std::min, std::copy, std::fill
#include <cctype>     // Para std::toupper, std::isalnum
#include <cstdint>    // Para uint8_t, uint16_t, uint32_t (tipos de tamanho fixo)
#include <fstream>    // Para ler/escrever a imagem em modo binário
#include <iomanip>    // Para formatação de saída (setfill, setw)
#include <iostream>   // Para entrada/saída de texto
#include <sstream>    // Para manipulação de strings
#include <string>     // Para std::string
#include <vector>     // Para std::vector (armazenar imagem na memória)
#include <ctime>      // Para data/hora de criação de arquivos

/*
 * ESTRUTURA DOS METADADOS DO VOLUME FAT16
 * 
 * Esta struct armazena TODAS as informações derivadas do Boot Sector.
 * Por quê separar em struct? Para manter tudo organizado e evitar variáveis globais
 * soltas que tornam o código confuso.
 */
struct Fat16Volume {
    // Campos lidos diretamente do Boot Sector (BIOS Parameter Block)
    uint16_t bytesPerSector;      // Tamanho de um setor (ex: 512 bytes). Nem sempre é 512!
    uint8_t sectorsPerCluster;    // Quantos setores cabem em um cluster (ex: 8 setores = 1 cluster)
    uint16_t reservedSectorCount; // Setores antes da primeira FAT (inclui boot sector)
    uint8_t numFATs;              // Quantas cópias da FAT existem (geralmente 2)
    uint16_t rootEntryCount;      // Quantas entradas o diretório raiz pode ter (ex: 512)
    uint16_t sectorsPerFAT;       // Setores usados por UMA cópia da FAT
    uint32_t totalSectors;        // Total de setores na imagem
    
    // Campos calculados (derivados dos anteriores)
    // Por quê calcular e armazenar? Para evitar repetir o cálculo n vezes durante execução
    uint32_t rootDirSectors;      // Quantos setores o root directory ocupa
    uint32_t firstRootDirSector;  // Número do setor onde root directory começa
    uint32_t firstDataSector;     // Número do setor onde a data region começa (cluster 2)
    uint32_t rootDirOffset;       // Byte offset na imagem onde root directory começa
    uint32_t fatOffset;           // Byte offset na imagem onde FAT começa
    uint32_t dataRegionOffset;    // Byte offset na imagem onde dados começam
    uint32_t clusterSize;         // Tamanho de um cluster em bytes
    uint32_t totalClusters;       // Total de clusters disponíveis para dados
};

// VARIÁVEIS GLOBAIS
// Sim, usamos globais aqui. Por quê? Para simplifidade. Na prática, em sistemas reais,
// isso seria encapsulado em uma classe. Mas o enunciado pede simplicidade, então mantemos assim.

std::vector<uint8_t> imagemDisco;  // A imagem FAT16 inteira carregada na memória como vetor de bytes.
                                   // Usamos vetor porque permite acesso rápido por índice e modificações.
                                   // uint8_t (unsigned char) porque cada byte da imagem é independente.

Fat16Volume volume;                // Metadados do volume (lidos do Boot Sector)

std::string caminhoImagem;         // Caminho para a imagem, para poder salvar alterações depois

/*
 * LEITURA E ESCRITA DE DADOS BINÁRIOS
 * 
 * FAT16 trabalha com dados binários em little-endian. Isso significa que números
 * multi-byte são armazenados com o byte MENOS significativo primeiro.
 * Por exemplo: 0x1234 é armazenado como 0x34 0x12
 * 
 * Por quê little-endian? Porque foi assim que a Intel x86 fez historicamente.
 * 
 * Essas funções lêm valores little-endian corretamente.
 */

// Lê um inteiro de 16 bits (2 bytes) no offset especificado
uint16_t ler16(uint32_t deslocamento) {
    return static_cast<uint16_t>(imagemDisco[deslocamento]) |
           (static_cast<uint16_t>(imagemDisco[deslocamento + 1]) << 8);
    // Explica: byte[0] | (byte[1] << 8)
    // Exemplo: se imagemDisco[deslocamento] = 0x34 e imagemDisco[deslocamento+1] = 0x12
    // Resultado: 0x34 | (0x12 << 8) = 0x34 | 0x1200 = 0x1234 ✓
}

// Lê um inteiro de 32 bits (4 bytes) no offset especificado
uint32_t ler32(uint32_t deslocamento) {
    return static_cast<uint32_t>(imagemDisco[deslocamento]) |
           (static_cast<uint32_t>(imagemDisco[deslocamento + 1]) << 8) |
           (static_cast<uint32_t>(imagemDisco[deslocamento + 2]) << 16) |
           (static_cast<uint32_t>(imagemDisco[deslocamento + 3]) << 24);
    // Mesma lógica: combina 4 bytes em little-endian
}

// Escreve um inteiro de 16 bits no offset especificado
void escrever16(uint32_t deslocamento, uint16_t valor) {
    imagemDisco[deslocamento] = static_cast<uint8_t>(valor & 0xff);           // byte menos significativo
    imagemDisco[deslocamento + 1] = static_cast<uint8_t>((valor >> 8) & 0xff); // byte mais significativo
    // Inverte o processo: quebra 0x1234 em 0x34 0x12
}

// Escreve um inteiro de 32 bits no offset especificado
void escrever32(uint32_t deslocamento, uint32_t valor) {
    imagemDisco[deslocamento] = static_cast<uint8_t>(valor & 0xff);
    imagemDisco[deslocamento + 1] = static_cast<uint8_t>((valor >> 8) & 0xff);
    imagemDisco[deslocamento + 2] = static_cast<uint8_t>((valor >> 16) & 0xff);
    imagemDisco[deslocamento + 3] = static_cast<uint8_t>((valor >> 24) & 0xff);
}

/*
 * IDENTIFICAÇÃO DE FIM DE CADEIA NA FAT
 * 
 * Na FAT16, cada cluster aponta para o próximo cluster do arquivo.
 * Quando chegamos ao fim, a FAT contém um valor >= 0xFFF8.
 * 
 * Por quê 0xFFF8? É um marcador especial que significa "fim de cadeia".
 * Valores 0xFFF8 até 0xFFFF são todos marcadores de fim (reservados).
 */
bool ehFimDaCadeia(uint16_t cluster) {
    return cluster >= 0xFFF8u;
}

/*
 * VALIDAÇÃO E PARSE DO BOOT SECTOR
 * 
 * O Boot Sector é o primeiro setor (512 bytes) da imagem.
 * Contém TODA a informação que precisamos para entender a estrutura do volume.
 * 
 * Por quê? Porque programas precisam descobrir dinamicamente:
 * - Quantos bytes tem cada setor (pode não ser 512)
 * - Quantos setores tem cada cluster
 * - Onde fica a FAT, o root directory e os dados
 * 
 * Nunca assumir valores fixos! Sempre ler do Boot Sector.
 */
bool validarBootSector() {
    // Verificação básica: imagem tem que ter pelo menos um setor
    if (imagemDisco.size() < 512) {
        std::cerr << "Imagem muito pequena para ser FAT16.\n";
        return false;
    }
    
    // Verificação da assinatura: Boot Sector termina com 0x55AA (little-endian = AA55)
    // Por quê? É um padrão universal de x86 para indicar um boot sector válido.
    if (ler16(510) != 0xAA55) {
        std::cerr << "Assinatura de boot faltando: nao parece ser FAT16.\n";
        return false;
    }
    
    // Leitura dos campos do BIOS Parameter Block (BPB)
    // Offsets definidos pelo padrão FAT16 (Microsoft)
    volume.bytesPerSector = ler16(11);        // Offset 11-12
    volume.sectorsPerCluster = imagemDisco[13];  // Offset 13
    volume.reservedSectorCount = ler16(14);   // Offset 14-15 (inclui boot sector)
    volume.numFATs = imagemDisco[16];            // Offset 16
    volume.rootEntryCount = ler16(17);        // Offset 17-18
    volume.sectorsPerFAT = ler16(22);         // Offset 22-23
    
    // FAT16 pode ter total de setores em um de dois campos
    // Campo de 16 bits se < 65536, ou campo de 32 bits se maior
    uint16_t totalSectors16 = ler16(19);
    uint32_t totalSectors32 = ler32(32);
    volume.totalSectors = totalSectors16 != 0 ? totalSectors16 : totalSectors32;

    // Validação: nenhum desses valores pode ser zero
    // Se algum for zero, a imagem está corrompida ou não é FAT16
    if (volume.bytesPerSector == 0 || volume.sectorsPerCluster == 0 ||
        volume.reservedSectorCount == 0 || volume.numFATs == 0 ||
        volume.rootEntryCount == 0 || volume.sectorsPerFAT == 0 ||
        volume.totalSectors == 0) {
        std::cerr << "Boot sector com valores invalidos para FAT16.\n";
        return false;
    }

    // CÁLCULO DE POSIÇÕES - Fórmulas do padrão Microsoft FATGEN103
    
    // Tamanho do diretório raiz em setores
    // Cada entrada tem 32 bytes, então: (entradas * 32) / bytesPerSetor = setores
    volume.rootDirSectors = ((volume.rootEntryCount * 32u) + (volume.bytesPerSector - 1u)) / volume.bytesPerSector;
    
    // Primeiro setor do diretório raiz
    // Vem logo após: setores reservados + (nFATs * setoresPerFAT)
    volume.firstRootDirSector = volume.reservedSectorCount + (volume.numFATs * volume.sectorsPerFAT);
    
    // Primeiro setor da região de dados
    // Vem logo após: root directory
    volume.firstDataSector = volume.firstRootDirSector + volume.rootDirSectors;
    
    // Offsets em BYTES (para acessar o vetor diskImage)
    volume.fatOffset = static_cast<uint32_t>(volume.reservedSectorCount) * volume.bytesPerSector;
    volume.rootDirOffset = static_cast<uint32_t>(volume.firstRootDirSector) * volume.bytesPerSector;
    volume.dataRegionOffset = static_cast<uint32_t>(volume.firstDataSector) * volume.bytesPerSector;
    
    // Tamanho de um cluster em bytes
    volume.clusterSize = static_cast<uint32_t>(volume.bytesPerSector) * volume.sectorsPerCluster;

    // Total de clusters (data region / tamanho do cluster)
    // Importante: em FAT16, o primeiro cluster é cluster 2 (0 e 1 são especiais)
    uint32_t dataSectors = (volume.totalSectors - volume.firstDataSector);
    volume.totalClusters = dataSectors / volume.sectorsPerCluster;
    
    // Validação: FAT16 precisa de pelo menos cluster 2 em diante
    if (volume.totalClusters < 2) {
        std::cerr << "Imagem FAT16 com area de dados invalida.\n";
        return false;
    }
    
    return true;
}

/*
 * MANIPULAÇÃO DE NOMES NO PADRÃO FAT16 (8.3)
 * 
 * FAT16 usa formato 8.3: máximo 8 caracteres + ponto + máximo 3 de extensão.
 * Os 11 bytes da entrada de diretório são sempre: 8 do nome + 3 da extensão.
 * Espaços em branco preenchem campos não usados.
 * 
 * Por quê esse formato antigo? Compatibilidade com DOS e sistemas legados.
 * Nomes longos (LFN) vieram depois e precisam de mais entradas.
 */

// Normaliza um nome para maiúsculas e remove caracteres inválidos
std::string normalizarNome(const std::string& texto) {
    std::string resultado;
    for (char c : texto) {
        if (c == '.') {
            resultado.push_back('.');
        } else if (std::isalnum(static_cast<unsigned char>(c)) || std::strchr("$%'-_@~`!(){}^#&", c)) {
            // FAT16 permite esses caracteres especiais (além de alfanuméricos)
            resultado.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
        }
    }
    return resultado;
}

// Converte uma string como "arquivo.txt" em array FAT16 8.3: "ARQUIVO TXT"
// Retorna true se conversão foi válida, false se nome não segue o padrão 8.3
bool analisarNomeFat(const std::string& entrada, std::array<char, 11>& saida) {
    std::string texto = normalizarNome(entrada);
    
    // Procura por ponto para separar nome e extensão
    size_t posicaoPonto = texto.find('.');
    std::string nome;
    std::string extensao;
    
    if (posicaoPonto == std::string::npos) {
        // Sem extensão
        nome = texto;
    } else {
        // Com extensão
        nome = texto.substr(0, posicaoPonto);
        extensao = texto.substr(posicaoPonto + 1);
        
        // Validação: não pode ter mais de um ponto
        if (texto.find('.', posicaoPonto + 1) != std::string::npos) {
            return false;
        }
    }
        return false;
    }
    
    // Preenche o array: nome em 0-7, extensão em 8-10, resto com espaços
    saida.fill(' ');
    for (size_t i = 0; i < nome.size(); ++i) {
        saida[i] = nome[i];
    }
    for (size_t i = 0; i < extensao.size(); ++i) {
        saida[8 + i] = extensao[i];
    }
    return true;
}

// Lê um nome do array FAT16 8.3 e retorna como string legível
// Ex: "ARQUIVO TXT" vira "ARQUIVO.TXT"
std::string obterNomeFatDaEntrada(uint32_t deslocamentoEntrada) {
    std::string nome;
    
    // Lê os 8 bytes do nome, ignorando espaços no final
    for (int i = 0; i < 8; ++i) {
        char c = static_cast<char>(imagemDisco[deslocamentoEntrada + i]);
        if (c == ' ') continue; // Espaço significa fim do nome
        nome.push_back(c);
    }
    
    // Lê os 3 bytes da extensão, ignorando espaços
    std::string extensao;
    for (int i = 0; i < 3; ++i) {
        char c = static_cast<char>(imagemDisco[deslocamentoEntrada + 8 + i]);
        if (c == ' ') continue;
        extensao.push_back(c);
    }
    
    // Monta string final com ponto se houver extensão
    if (!extensao.empty()) {
        return nome + "." + extensao;
    }
    return nome;
}

// Verificação de atributos de entrada de diretório
// Volume label é um metadata da imagem, não um arquivo
bool ehRotuloDeVolume(uint32_t deslocamentoEntrada) {
    uint8_t atributos = imagemDisco[deslocamentoEntrada + 11]; // Byte 11 da entrada
    return (atributos & 0x08u) != 0u; // Bit 3 = volume label
}

// Entrada de Nome Longo (Long File Name - LFN)
// FAT16 original não suporta nomes longos, mas o padrão permite
// Por quê? Para compatibilidade com Windows 95+
bool ehEntradaNomeLongo(uint32_t deslocamentoEntrada) {
    uint8_t atributos = imagemDisco[deslocamentoEntrada + 11];
    return atributos == 0x0Fu; // Valor especial 0x0F indica LFN
}

/*
 * LEITURA DE CAMPOS DA ENTRADA DE DIRETÓRIO
 * 
 * Uma entrada de 32 bytes contém:
 * 0-7:    Nome (8 bytes)
 * 8-10:   Extensão (3 bytes)
 * 11:     Atributos
 * 12:     Reservado
 * 13:     Centésimo de segundo da criação
 * 14-15:  Hora e minuto de criação
 * 16-17:  Data de criação
 * 18-19:  Data do último acesso
 * 20-21:  Bits altos do primeiro cluster (FAT32 apenas, 0 em FAT16)
 * 22-23:  Hora e minuto da última modificação
 * 24-25:  Data da última modificação
 * 26-27:  Primeiro cluster (número do cluster onde arquivo começa)
 * 28-31:  Tamanho do arquivo em bytes
 */

// Lê o cluster inicial do arquivo (onde começam os dados)
uint16_t obterClusterInicioDaEntrada(uint32_t deslocamentoEntrada) {
    return ler16(deslocamentoEntrada + 26);
}

// Lê o tamanho do arquivo em bytes
uint32_t obterTamanhoDaEntrada(uint32_t deslocamentoEntrada) {
    return ler32(deslocamentoEntrada + 28);
}

/*
 * OPERAÇÕES NA FILE ALLOCATION TABLE (FAT)
 * 
 * A FAT é uma tabela que mapeia clusters.
 * FAT[n] = m significa "cluster n aponta para o próximo cluster m"
 * 
 * Por quê usar FAT? Permite fragmentação flexível. O arquivo não precisa
 * estar em clusters contíguos - pode estar espalhado pelo disco.
 * 
 * Exemplo:
 * Arquivo em clusters 5 -> 12 -> 19 -> EOF
 * FAT[5] = 12
 * FAT[12] = 19
 * FAT[19] = 0xFFFF (fim)
 */

// Lê uma entrada de 16 bits da FAT
// Cada cluster aponta para o próximo (ou 0xFFFF se for o último)
uint16_t obterEntradaFat(uint16_t cluster) {
    uint32_t deslocamento = volume.fatOffset + (cluster * 2u); // Cada entrada é 2 bytes
    return ler16(deslocamento);
}

// Escreve uma entrada de 16 bits na FAT
// Importante: em FAT16 com múltiplas cópias, precisamos atualizar TODAS
void definirEntradaFat(uint16_t cluster, uint16_t valor) {
    uint32_t deslocamentoPrimario = volume.fatOffset + (cluster * 2u);
    
    // Por quê múltiplas cópias? Para redundância. Se uma FAT corromper, usa a outra.
    for (uint8_t fat = 0; fat < volume.numFATs; ++fat) {
        uint32_t deslocamentoFat = deslocamentoPrimario + static_cast<uint32_t>(fat) * volume.sectorsPerFAT * volume.bytesPerSector;
        escrever16(deslocamentoFat, valor);
    }
}

/*
 * CONVERSÃO DE NÚMERO DE CLUSTER PARA OFFSET NA IMAGEM
 * 
 * Clusters são numerados começando em 2 (0 e 1 são especiais).
 * Para encontrar onde o cluster está na imagem, precisamos:
 * 1. Somar o número de clusters (menos 2, pois começamos em 2)
 * 2. Multiplicar pelo tamanho do cluster em setores
 * 3. Converter para bytes
 * 
 * Fórmula: offset = firstDataSector + ((cluster - 2) * sectorsPerCluster) * bytesPerSector
 */
uint32_t converterClusterParaDeslocamento(uint16_t cluster) {
    uint32_t primeiroSetor = volume.firstDataSector + (static_cast<uint32_t>(cluster - 2) * volume.sectorsPerCluster);
    return primeiroSetor * volume.bytesPerSector;
}

/*
 * BUSCA POR ARQUIVO NO DIRETÓRIO RAIZ
 * 
 * O diretório raiz é um array de 32 bytes por entrada.
 * Iteramos por todas as entradas procurando pelo nome especificado.
 * 
 * Regras:
 * - 0x00 como primeiro byte = fim de entradas válidas
 * - 0xE5 como primeiro byte = entrada apagada (ignorar)
 * - Comparar bytes 0-10 (8 bytes nome + 3 bytes extensão)
 */
bool findRootEntryByName(const std::string& name, uint32_t& entryOffset) {
    // Converte para formato FAT16 8.3
    std::array<char, 11> searchName;
    if (!parseFatName(name, searchName)) {
        return false;
    }
    
    // Itera por todas as entradas do root directory
    for (uint32_t entry = 0; entry < volume.rootEntryCount; ++entry) {
        uint32_t offset = volume.rootDirOffset + entry * 32;
        uint8_t firstByte = diskImage[offset];
        
        // 0x00 = fim de entradas válidas
        if (firstByte == 0x00u) {
            break;
        }
        
        // 0xE5 = entrada apagada
        if (firstByte == 0xE5u) {
            continue;
        }
        
        // Ignora entradas especiais (volume label, LFN)
        if (isLongFileNameEntry(offset) || isVolumeLabel(offset)) {
            continue;
        }
        
        // Compara nome (11 bytes)
        bool equal = true;
        for (int i = 0; i < 11; ++i) {
            if (diskImage[offset + i] != static_cast<uint8_t>(searchName[i])) {
                equal = false;
                break;
            }
        }
        
        if (equal) {
            entryOffset = offset;
            return true;
        }
    }
    
    return false;
}

/*
 * DECODIFICAÇÃO DE DATA E HORA DO FAT16
 * 
 * FAT16 armazena data/hora em formato comprimido de 16 bits para cada:
 * 
 * DATA (16 bits):
 *   bits 0-4:   Dia (1-31)
 *   bits 5-8:   Mês (1-12)
 *   bits 9-15:  Ano (0-127, onde 0 = 1980)
 * 
 * HORA (16 bits):
 *   bits 0-4:   Segundos/2 (0-29, pois só temos 5 bits = 32 valores para 60 segundos)
 *   bits 5-10:  Minutos (0-59)
 *   bits 11-15: Horas (0-23)
 * 
 * Nota: segundos são armazenados em incrementos de 2, então não há precisão de 1 segundo.
 */
void imprimirDataHora(uint16_t data, uint16_t hora) {
    // Se ambos forem 0, significa que não foi definido
    if (data == 0 && hora == 0) {
        std::cout << "N/A";
        return;
    }
    
    // Decodificação da DATA
    uint16_t dia = data & 0x1F;                    // bits 0-4
    uint16_t mes = (data >> 5) & 0x0F;           // bits 5-8
    uint16_t ano = ((data >> 9) & 0x7F) + 1980;   // bits 9-15 + offset de 1980
    
    // Decodificação da HORA
    uint16_t segundo = (hora & 0x1F) * 2;           // bits 0-4, multiplicado por 2
    uint16_t minuto = (hora >> 5) & 0x3F;          // bits 5-10
    uint16_t hora_formato = (hora >> 11) & 0x1F;           // bits 11-15
    
    // Formata e imprime
    std::cout << std::setfill('0') << std::setw(2) << dia << "/"
              << std::setw(2) << mes << "/"
              << std::setw(4) << ano << " "
              << std::setw(2) << hora_formato << ":"
              << std::setw(2) << minuto << ":"
              << std::setw(2) << segundo << std::setfill(' ');
}

/*
 * OPERAÇÃO 1: LISTAR ARQUIVOS DO DIRETÓRIO RAIZ
 * 
 * Itera por todas as entradas do root directory.
 * Ignora entradas inválidas (apagadas, especiais).
 * Exibe nome e tamanho de cada arquivo.
 */
void listRootDirectory() {
    std::cout << "\nArquivos no diretorio raiz:\n";
    std::cout << std::left << std::setw(20) << "NOME" << std::right << std::setw(10) << "TAMANHO" << "\n";
    std::cout << std::string(30, '-') << "\n";
    
    for (uint32_t entry = 0; entry < volume.rootEntryCount; ++entry) {
        uint32_t offset = volume.rootDirOffset + entry * 32;
        uint8_t firstByte = diskImage[offset];
        
        // Fim de entradas válidas
        if (firstByte == 0x00u) {
            break;
        }
        
        // Entrada apagada
        if (firstByte == 0xE5u) {
            continue;
        }
        
        // Entradas especiais (volume label, LFN)
        if (isLongFileNameEntry(offset) || isVolumeLabel(offset)) {
            continue;
        }
        
        std::string filename = fatNameFromEntry(offset);
        uint32_t fileSize = getEntryFileSize(offset);
        std::cout << std::left << std::setw(20) << filename << std::right << std::setw(10) << fileSize << "\n";
    }
}

/*
 * OPERAÇÃO 2: EXIBIR CONTEÚDO DE UM ARQUIVO
 * 
 * Passos:
 * 1. Localiza arquivo no diretório raiz
 * 2. Lê o cluster inicial da entrada
 * 3. Segue a cadeia de clusters na FAT
 * 4. Para cada cluster, lê os dados e exibe
 * 5. Para quando atinge o fim de cadeia ou tamanho do arquivo
 * 
 * Por quê seguir a FAT? Arquivo pode estar fragmentado.
 * FAT nos diz a ordem correta dos clusters, não a posição física.
 */
void showFileContent() {
    std::cout << "Digite o nome do arquivo (8.3): ";
    std::string name;
    std::getline(std::cin, name);
    if (name.empty()) {
        std::cout << "Nome invalido.\n";
        return;
    }
    
    // Localiza arquivo
    uint32_t entryOffset;
    if (!findRootEntryByName(name, entryOffset)) {
        std::cout << "Arquivo nao encontrado.\n";
        return;
    }
    
    uint32_t fileSize = getEntryFileSize(entryOffset);
    uint16_t cluster = getEntryStartCluster(entryOffset);
    
    if (fileSize == 0) {
        std::cout << "Arquivo vazio.\n";
        return;
    }
    
    std::cout << "\nConteudo do arquivo:\n";
    
    // Segue a cadeia de clusters
    uint32_t remaining = fileSize;
    while (remaining > 0 && cluster >= 2 && !isEndOfChain(cluster)) {
        uint32_t offset = clusterToOffset(cluster);
        
        // Lê até o tamanho do cluster ou o que sobrou do arquivo
        uint32_t toRead = std::min<uint32_t>(remaining, volume.clusterSize);
        
        // Imprime cada byte do cluster
        for (uint32_t i = 0; i < toRead; ++i) {
            std::cout << static_cast<char>(diskImage[offset + i]);
        }
        
        remaining -= toRead;
        // Próximo cluster
        cluster = getFatEntry(cluster);
    }
    std::cout << "\n";
}

/*
 * OPERAÇÃO 3: EXIBIR ATRIBUTOS DE UM ARQUIVO
 * 
 * Mostra:
 * - Data/hora de criação
 * - Data/hora da última modificação
 * - Flags: somente leitura, oculto, sistema
 */
void mostrarAtributos() {
    std::cout << "Digite o nome do arquivo (8.3): ";
    std::string nome;
    std::getline(std::cin, nome);
    if (nome.empty()) {
        std::cout << "Nome invalido.\n";
        return;
    }
    
    uint32_t deslocamentoEntrada;
    if (!encontrarEntradaRaizPorNome(nome, deslocamentoEntrada)) {
        std::cout << "Arquivo nao encontrado.\n";
        return;
    }
    
    // Lê o byte de atributos (offset 11)
    uint8_t atributos = imagemDisco[deslocamentoEntrada + 11];
    
    // Lê timestamps de criação (offsets 14-17)
    uint16_t horaGriacao = ler16(deslocamentoEntrada + 14);
    uint16_t dataGriacao = ler16(deslocamentoEntrada + 16);
    
    // Lê timestamps de modificação (offsets 22-25)
    uint16_t horaUltimaEscrita = ler16(deslocamentoEntrada + 22);
    uint16_t dataUltimaEscrita = ler16(deslocamentoEntrada + 24);

    std::cout << "\nAtributos do arquivo: " << obterNomeFatDaEntrada(deslocamentoEntrada) << "\n";
    std::cout << "Criacao: ";
    imprimirDataHora(dataGriacao, horaGriacao);
    std::cout << "\nUltima modificacao: ";
    imprimirDataHora(dataUltimaEscrita, horaUltimaEscrita);
    
    // Decodifica bits de atributos
    std::cout << "\nSomente leitura: " << ((atributos & 0x01u) ? "SIM" : "NAO");  // Bit 0
    std::cout << "\nOculto: " << ((atributos & 0x02u) ? "SIM" : "NAO");            // Bit 1
    std::cout << "\nSistema: " << ((atributos & 0x04u) ? "SIM" : "NAO");          // Bit 2
    std::cout << "\n";
}

/*
 * FUNÇÕES AUXILIARES PARA CRIAÇÃO DE ARQUIVO
 */

// Encontra uma entrada livre no root directory para novo arquivo
// 0x00 = entrada vazia (nunca foi usada)
// 0xE5 = entrada apagada (foi usada, depois removida)
// Ambas são reutilizáveis para novo arquivo
bool findFreeRootEntry(uint32_t& entryOffset) {
    for (uint32_t entry = 0; entry < volume.rootEntryCount; ++entry) {
        uint32_t offset = volume.rootDirOffset + entry * 32;
        uint8_t firstByte = diskImage[offset];
        if (firstByte == 0x00u || firstByte == 0xE5u) {
            entryOffset = offset;
            return true;
        }
    }
    return false;
}

// Procura por clusters livres na FAT (valor = 0x0000)
// Calcula quantos clusters são necessários para o arquivo
// Retorna vetor com números dos clusters livres encontrados
bool findFreeClusters(uint32_t needed, std::vector<uint16_t>& clusters) {
    clusters.clear();
    
    // Itera por todos os clusters disponíveis
    for (uint16_t cluster = 2; cluster <= volume.totalClusters + 1; ++cluster) {
        if (getFatEntry(cluster) == 0x0000u) {  // Cluster livre
            clusters.push_back(cluster);
            if (clusters.size() == needed) {
                return true;  // Achou espaço suficiente
            }
        }
    }
    
    return false;  // Não achou espaço suficiente
}

// Aloca uma cadeia de clusters na FAT
// Conecta cada cluster ao próximo: C1 -> C2 -> C3 -> 0xFFFF
// Retorna o número do primeiro cluster
uint16_t allocateChain(const std::vector<uint16_t>& clusters) {
    if (clusters.empty()) {
        return 0;
    }
    
    for (size_t i = 0; i < clusters.size(); ++i) {
        uint16_t current = clusters[i];
        
        // Se é o último cluster, marca com 0xFFFF (fim de cadeia)
        // Senão, aponta para o próximo cluster
        uint16_t value = (i + 1 < clusters.size()) ? clusters[i + 1] : 0xFFFFu;
        setFatEntry(current, value);
    }
    
    return clusters[0];  // Retorna primeiro cluster da cadeia
}

/*
 * CODIFICAÇÃO DE DATA E HORA PARA FAT16
 * 
 * Convertem da estrutura tm do C++ (std::localtime) para o formato comprimido FAT16.
 * Ver comentário em printTimestamp() para entender o formato.
 */

// Codifica time_t em formato FAT16 de 16 bits para hora
uint16_t empacorarTempoFat(const std::tm& tempo) {
    uint16_t horas = static_cast<uint16_t>(tempo.tm_hour);
    uint16_t minutos = static_cast<uint16_t>(tempo.tm_min);
    uint16_t segundos = static_cast<uint16_t>(tempo.tm_sec / 2);  // Dividido por 2
    return static_cast<uint16_t>((horas << 11) | (minutos << 5) | segundos);
}

// Codifica tm em formato FAT16 de 16 bits para data
uint16_t empacorarDataFat(const std::tm& tempo) {
    uint16_t ano = static_cast<uint16_t>(tempo.tm_year + 1900);
    
    // Se ano < 1980, usa 1980 (limite mínimo do FAT)
    if (ano < 1980) {
        ano = 1980;
    }
    
    uint16_t mes = static_cast<uint16_t>(tempo.tm_mon + 1);  // tm_mon é 0-11
    uint16_t dia = static_cast<uint16_t>(tempo.tm_mday);
    
    return static_cast<uint16_t>(((ano - 1980) << 9) | (mes << 5) | dia);
}

/*
 * SALVAR IMAGEM DE VOLTA AO DISCO
 * 
 * Toda vez que modificamos FAT, root directory ou dados de arquivo,
 * precisamos escrever a imagem modificada de volta ao arquivo.
 * 
 * Por quê modo binário? Para garantir que cada byte seja salvo exatamente como está.
 * Modo texto poderia interpretar sequências especiais como \n, \r, etc.
 */
bool salvarImagem() {
    // std::ios::binary = não interpretar nada
    // std::ios::trunc = truncar arquivo se já existir
    std::ofstream saida(caminhoImagem, std::ios::binary | std::ios::trunc);
    if (!saida) {
        std::cerr << "Nao foi possivel salvar a imagem.\n";
        return false;
    }
    
    // Escreve todo o vetor como bytes brutos
    saida.write(reinterpret_cast<const char*>(imagemDisco.data()), 
                 static_cast<std::streamsize>(imagemDisco.size()));
    
    return saida.good();
}

void criarNovoArquivo() {
    /*
     * OPERAÇÃO 5: INSERIR NOVO ARQUIVO EXTERNO NA IMAGEM
     * 
     * Passos:
     * 1. Lê arquivo externo do computador
     * 2. Valida que não existe arquivo com mesmo nome
     * 3. Aloca clusters livres na FAT
     * 4. Escreve dados nos clusters
     * 5. Cria entrada no root directory
     * 6. Atualiza FAT com a cadeia de clusters
     * 7. Salva imagem
     * 
     * Por quê tudo isso?
     * - Lê arquivo externo porque vem do sistema local
     * - Aloca clusters porque FAT precisa saber onde estão os dados
     * - Cria entrada de diretório para arquivo aparecer em listagem
     * - Salva porque modificações precisam ser persistidas
     */
    
    // Lê arquivo externo
    std::cout << "Caminho do arquivo externo: ";
    std::string caminhoExterno;
    std::getline(std::cin, caminhoExterno);
    if (caminhoExterno.empty()) {
        std::cout << "Caminho invalido.\n";
        return;
    }
    std::ifstream externo(caminhoExterno, std::ios::binary);
    if (!externo) {
        std::cout << "Nao foi possivel abrir o arquivo externo.\n";
        return;
    }
    std::vector<uint8_t> dadosExternos((std::istreambuf_iterator<char>(externo)), std::istreambuf_iterator<char>());
    externo.close();

    // Obtém nome para arquivo na imagem
    std::cout << "Nome para o arquivo na imagem (8.3): ";
    std::string nomeAlvo;
    std::getline(std::cin, nomeAlvo);
    if (nomeAlvo.empty()) {
        std::cout << "Nome invalido.\n";
        return;
    }
    
    // Valida que não existe com mesmo nome
    uint32_t deslocamentoExistente;
    if (encontrarEntradaRaizPorNome(nomeAlvo, deslocamentoExistente)) {
        std::cout << "Ja existe um arquivo com esse nome.\n";
        return;
    }
    
    // Converte para formato 8.3
    std::array<char, 11> nomeFat;
    if (!analisarNomeFat(nomeAlvo, nomeFat)) {
        std::cout << "Nome nao esta no formato valido 8.3.\n";
        return;
    }

    // Encontra entrada livre no root directory
    uint32_t deslocamentoRaizLivre;
    if (!encontrarEntradaRaizLivre(deslocamentoRaizLivre)) {
        std::cout << "Diretorio raiz sem entradas livres.\n";
        return;
    }
    
    // Calcula quantos clusters são necessários
    uint32_t tamanhoArquivo = static_cast<uint32_t>(dadosExternos.size());
    uint32_t clustersNecessarios = 0;
    if (tamanhoArquivo > 0) {
        clustersNecessarios = (tamanhoArquivo + volume.clusterSize - 1u) / volume.clusterSize;
    }
    
    // Encontra clusters livres
    std::vector<uint16_t> clustersLivres;
    if (clustersNecessarios > 0 && !encontrarClusterLivres(clustersNecessarios, clustersLivres)) {
        std::cout << "Espaco insuficiente na imagem.\n";
        return;
    }
    
    // Aloca cadeia de clusters na FAT
    uint16_t primeiroCluster = alocarCadeia(clustersLivres);

    // Escreve dados nos clusters
    if (tamanhoArquivo > 0) {
        uint32_t restante = tamanhoArquivo;
        for (size_t i = 0; i < clustersLivres.size(); ++i) {
            uint32_t deslocamento = converterClusterParaDeslocamento(clustersLivres[i]);
            uint32_t paraEscrever = std::min<uint32_t>(restante, volume.clusterSize);
            
            // Copia dados do arquivo externo para cluster
            std::copy(dadosExternos.begin() + (i * volume.clusterSize), dadosExternos.begin() + (i * volume.clusterSize + paraEscrever), imagemDisco.begin() + deslocamento);
            
            // Preenche resto do cluster com zeros
            if (paraEscrever < volume.clusterSize) {
                std::fill(imagemDisco.begin() + deslocamento + paraEscrever, imagemDisco.begin() + deslocamento + volume.clusterSize, 0);
            }
            restante -= paraEscrever;
        }
    }

    // Obtém timestamp atual
    std::time_t agora = std::time(nullptr);
    std::tm horaLocal = *std::localtime(&agora);
    uint16_t horaGriacao = empacorarTempoFat(horaLocal);
    uint16_t dataGriacao = empacorarDataFat(horaLocal);
    uint16_t horaUltimaEscrita = horaGriacao;
    uint16_t dataUltimaEscrita = dataGriacao;

    // Preenche entrada de diretório
    std::fill(imagemDisco.begin() + deslocamentoRaizLivre, imagemDisco.begin() + deslocamentoRaizLivre + 32, 0);
    for (int i = 0; i < 11; ++i) {
        imagemDisco[deslocamentoRaizLivre + i] = static_cast<uint8_t>(nomeFat[i]);
    }
    imagemDisco[deslocamentoRaizLivre + 11] = 0x20; // Atributos: arquivo normal
    escrever16(deslocamentoRaizLivre + 14, horaGriacao);
    escrever16(deslocamentoRaizLivre + 16, dataGriacao);
    escrever16(deslocamentoRaizLivre + 18, dataGriacao); // last access date
    escrever16(deslocamentoRaizLivre + 22, horaUltimaEscrita);
    escrever16(deslocamentoRaizLivre + 24, dataUltimaEscrita);
    escrever16(deslocamentoRaizLivre + 26, primeiroCluster);
    escrever32(deslocamentoRaizLivre + 28, tamanhoArquivo);

    // Salva imagem modificada
    if (!salvarImagem()) {
        std::cout << "Falha ao salvar imagem apos criacao.\n";
        return;
    }
    std::cout << "Arquivo criado com sucesso.\n";
}

/*
 * OPERAÇÃO 6: REMOVER/APAGAR UM ARQUIVO
 * 
 * Passos:
 * 1. Localiza arquivo no diretório raiz
 * 2. Segue a cadeia de clusters e libera cada um na FAT
 * 3. Marca a entrada de diretório como apagada (0xE5)
 * 4. Salva imagem
 * 
 * Por quê marcar como 0xE5? Para que possa ser reutilizada depois.
 * E liberar os clusters? Para que novos arquivos usem esse espaço.
 */
void removeFile() {
    std::cout << "Digite o nome do arquivo para apagar (8.3): ";
    std::string name;
    std::getline(std::cin, name);
    if (name.empty()) {
        std::cout << "Nome invalido.\n";
        return;
    }
    
    // Localiza arquivo
    uint32_t entryOffset;
    if (!findRootEntryByName(name, entryOffset)) {
        std::cout << "Arquivo nao encontrado.\n";
        return;
    }
    
    // Segue a cadeia de clusters e libera cada um
    uint16_t cluster = getEntryStartCluster(entryOffset);
    while (cluster >= 2 && !isEndOfChain(cluster)) {
        uint16_t nextCluster = getFatEntry(cluster);
        setFatEntry(cluster, 0x0000u);  // 0x0000 = cluster livre
        if (isEndOfChain(nextCluster)) {
            break;
        }
        cluster = nextCluster;
    }
    
    // Marca entrada de diretório como apagada
    diskImage[entryOffset] = 0xE5u;
    
    // Salva imagem
    if (!saveImage()) {
        std::cout << "Falha ao salvar imagem apos remocao.\n";
        return;
    }
    std::cout << "Arquivo removido com sucesso.\n";
}

/*
 * OPERAÇÃO 4: RENOMEAR UM ARQUIVO
 * 
 * Passos:
 * 1. Localiza arquivo atual
 * 2. Valida novo nome (formato 8.3 e colisão)
 * 3. Modifica apenas os 11 bytes do nome na entrada
 * 4. Não toca dados, clusters ou FAT
 * 5. Salva imagem
 * 
 * Por quê não tocar em nada mais?
 * Renomeação é uma operação que afeta apenas metadados na entrada de diretório.
 * Os dados do arquivo permanecem no mesmo lugar, com os mesmos clusters.
 */
void renomearArquivo() {
    std::cout << "Digite o nome atual do arquivo (8.3): ";
    std::string nomeAtual;
    std::getline(std::cin, nomeAtual);
    if (nomeAtual.empty()) {
        std::cout << "Nome invalido.\n";
        return;
    }
    
    // Localiza arquivo atual
    uint32_t deslocamentoEntrada;
    if (!encontrarEntradaRaizPorNome(nomeAtual, deslocamentoEntrada)) {
        std::cout << "Arquivo nao encontrado.\n";
        return;
    }
    
    // Obtém novo nome
    std::cout << "Digite o novo nome (8.3): ";
    std::string novoNome;
    std::getline(std::cin, novoNome);
    if (novoNome.empty()) {
        std::cout << "Nome invalido.\n";
        return;
    }
    
    // Converte novo nome para formato FAT16
    std::array<char, 11> novoNomeFat;
    if (!analisarNomeFat(novoNome, novoNomeFat)) {
        std::cout << "Novo nome nao esta no formato valido 8.3.\n";
        return;
    }
    
    // Valida colisão: não pode existir outro arquivo com novo nome
    uint32_t deslocamentoColisao;
    if (encontrarEntradaRaizPorNome(novoNome, deslocamentoColisao)) {
        std::cout << "Ja existe um arquivo com esse nome.\n";
        return;
    }
    
    // Modifica apenas os 11 bytes do nome na entrada (offsets 0-10)
    for (int i = 0; i < 11; ++i) {
        imagemDisco[deslocamentoEntrada + i] = static_cast<uint8_t>(novoNomeFat[i]);
    }
    
    // Salva imagem
    if (!salvarImagem()) {
        std::cout << "Falha ao salvar imagem apos renomear.\n";
        return;
    }
    std::cout << "Arquivo renomeado com sucesso.\n";
}

// Exibe menu de opções
void exibirMenu() {
    std::cout << "\n=== Gerenciador FAT16 ===\n";
    std::cout << "1. Listar arquivos do diretorio raiz\n";
    std::cout << "2. Exibir conteudo de um arquivo\n";
    std::cout << "3. Mostrar atributos de um arquivo\n";
    std::cout << "4. Renomear arquivo\n";
    std::cout << "5. Inserir novo arquivo\n";
    std::cout << "6. Remover arquivo\n";
    std::cout << "7. Sair\n";
    std::cout << "Escolha uma opcao: ";
}

/*
 * CARREGAMENTO E VALIDAÇÃO DA IMAGEM
 * 
 * Passos:
 * 1. Abre arquivo de imagem em modo binário
 * 2. Lê arquivo inteiro para memória (vetor diskImage)
 * 3. Valida Boot Sector (estrutura e metadados)
 * 4. Calcula posições de FAT, root directory e dados
 * 
 * Por quê carregar inteira na memória?
 * - Mais rápido que fazer seeks repetidos no arquivo
 * - Simplifica o código (tudo é um vetor)
 * - Imagens FAT16 geralmente cabem na RAM
 * 
 * Nota: em sistemas reais, seria mais eficiente usar mmap ou buffer caching.
 */
bool carregarImagem(const std::string& caminho) {
    // Abre arquivo e descobre tamanho
    std::ifstream arquivo(caminho, std::ios::binary | std::ios::ate);
    if (!arquivo) {
        std::cerr << "Nao foi possivel abrir a imagem FAT16.\n";
        return false;
    }
    
    // tellg() retorna posição do ponteiro (que é o final do arquivo)
    std::streamsize tamanho = arquivo.tellg();
    if (tamanho <= 0) {
        std::cerr << "Imagem vazia ou invalida.\n";
        return false;
    }
    
    // Redimensiona vetor para caber a imagem inteira
    imagemDisco.resize(static_cast<size_t>(tamanho));
    
    // Volta ao início e lê tudo
    arquivo.seekg(0, std::ios::beg);
    if (!arquivo.read(reinterpret_cast<char*>(imagemDisco.data()), tamanho)) {
        std::cerr << "Falha ao ler a imagem.\n";
        return false;
    }
    
    caminhoImagem = caminho;
    return validarBootSector();
}

/*
 * FUNÇÃO PRINCIPAL
 * 
 * Fluxo:
 * 1. Solicita caminho da imagem FAT16 ao usuário
 * 2. Carrega e valida a imagem
 * 3. Loop infinito exibindo menu e processando comandos
 * 4. Cada comando executa uma operação (listar, ler, renomear, etc.)
 * 5. Sai quando usuário escolhe opção 7
 * 
 * Por quê loop contínuo?
 * O enunciado pede menu contínuo sem reiniciar. Isso permite usar o programa
 * sem recarregar a imagem múltiplas vezes.
 */
int main() {
    // Solicita caminho da imagem
    std::cout << "Informe o caminho da imagem FAT16: ";
    std::string caminho;
    std::getline(std::cin, caminho);
    
    if (caminho.empty()) {
        std::cerr << "Caminho de imagem nao informado.\n";
        return 1;
    }
    
    // Carrega e valida imagem
    if (!carregarImagem(caminho)) {
        return 1;
    }
    
    // Loop principal: menu contínuo
    while (true) {
        exibirMenu();
        std::string opcao;
        std::getline(std::cin, opcao);
        
        if (opcao == "1") {
            listarDiretorioRaiz();
        } else if (opcao == "2") {
            mostrarConteudoArquivo();
        } else if (opcao == "3") {
            mostrarAtributos();
        } else if (opcao == "4") {
            renomearArquivo();
        } else if (opcao == "5") {
            criarNovoArquivo();
        } else if (opcao == "6") {
            removerArquivo();
        } else if (opcao == "7") {
            std::cout << "Saindo...\n";
            break;
        } else {
            std::cout << "Opcao invalida. Tente novamente.\n";
        }
    }
    
    return 0;
}
