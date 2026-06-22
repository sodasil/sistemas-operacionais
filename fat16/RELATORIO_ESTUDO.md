# Relatório de Estudo - Projeto FAT16

## 1. Objetivo do Trabalho

O projeto implementa um **manipulador de sistema de arquivos FAT16** em C++, capaz de abrir uma imagem de disco FAT16 e realizar operações básicas sobre o diretório raiz.

### Operações obrigatórias implementadas:
- ✅ Listar arquivos do disco
- ✅ Ler conteúdo de arquivo
- ✅ Exibir atributos de arquivo
- ✅ Renomear arquivo
- ✅ Criar arquivo
- ✅ Remover arquivo

### Limitações:
- Apenas diretório raiz (sem subdiretórios)
- Sistema de arquivos FAT16 em imagem de disco

---

## 2. Estrutura do Projeto

```
fat16/
├── main.cpp          # Código completo da aplicação
├── README.md         # Descrição e instruções
├── atividade.md      # Enunciado do trabalho
└── RELATORIO_ESTUDO.md  # Este arquivo
```

### Compilação:
```bash
g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp -o fat16
```

### Execução:
```bash
./fat16
```

---

## 3. Estruturas de Dados FAT16

### 3.1 BootSector
Representa o primeiro setor (512 bytes) da imagem FAT16 com informações críticas:

```cpp
struct BootSector {
    uint8_t jump[3];           // Instrução de jump
    char oem[8];               // Nome do OEM
    uint16_t bytesPerSector;   // Bytes por setor (geralmente 512)
    uint8_t sectorsPerCluster; // Setores por cluster
    uint16_t reservedSectors;  // Setores reservados (inclui boot sector)
    uint8_t numFATs;           // Número de FATs (geralmente 2)
    uint16_t rootEntryCount;   // Entradas no diretório raiz
    uint16_t totalSectors16;   // Total de setores (16 bits)
    uint8_t media;             // Tipo de mídia
    uint16_t fatSize16;        // Tamanho de uma FAT em setores
    uint16_t sectorsPerTrack;  // Setores por trilha
    uint16_t numHeads;         // Número de cabeçotes
    uint32_t hiddenSectors;    // Setores ocultos
    uint32_t totalSectors32;   // Total de setores (32 bits)
    // ... mais 14 bytes de informações
};
```

**Importante:** `#pragma pack(push, 1)` garante que a estrutura seja lida byte-a-byte sem alinhamento.

### 3.2 DirectoryEntry
Representa uma entrada do diretório raiz (32 bytes):

```cpp
struct DirectoryEntry {
    char filename[8];      // Nome do arquivo (preenchido com espaços)
    char extension[3];     // Extensão (preenchido com espaços)
    uint8_t attributes;    // Atributos do arquivo
    uint8_t reserved[10];  // Reservado
    uint16_t time;         // Hora de modificação
    uint16_t date;         // Data de modificação
    uint16_t firstCluster; // Primeiro cluster do arquivo
    uint32_t fileSize;     // Tamanho do arquivo em bytes
};
```

---

## 4. Layout da Imagem FAT16

```
[Boot Sector]      <- Setor 0 (512 bytes) - Informações do sistema
      ↓
[FAT 1]            <- reservedSectors até reservedSectors + fatSize16
      ↓            <- Tabela de Alocação de Arquivo (aponta clusters)
[FAT 2]            <- Cópia redundante da FAT
      ↓
[Root Directory]   <- Diretório raiz com entradas dos arquivos
      ↓
[Data Area]        <- Clusters com dados dos arquivos
```

### Cálculo de offsets:

**FAT Offset:**
```
fatOffset = reservedSectors * bytesPerSector
```

**Root Directory Offset:**
```
rootDirSector = reservedSectors + (numFATs * fatSize16)
rootDirOffset = rootDirSector * bytesPerSector
```

**Data Cluster Offset:**
```
rootDirSectors = ceil((rootEntryCount * 32) / bytesPerSector)
firstDataSector = reservedSectors + (numFATs * fatSize16) + rootDirSectors
offset = (firstDataSector + (cluster - 2) * sectorsPerCluster) * bytesPerSector
```

---

## 5. Operações Implementadas

### 5.1 ABRIR IMAGEM FAT16
**Método:** `openImage(filename)`

```cpp
bool openImage(const string& filename) {
    image.open(filename, ios::in | ios::out | ios::binary);
    if (!image.is_open()) {
        cout << "Erro ao abrir imagem.\n";
        return false;
    }
    image.read(reinterpret_cast<char*>(&bs), sizeof(bs));
    return true;
}
```

**O que faz:**
1. Abre arquivo em modo binário (leitura + escrita)
2. Lê os primeiros 512 bytes como `BootSector`
3. Valida se a leitura foi bem-sucedida

**Por que importante:** Esta é a operação primeira que permite acessar todas as outras informações do disco.

---

### 5.2 LISTAR DIRETÓRIO RAIZ
**Método:** `listRootDirectory()`

**Passos:**

1. **Calcular posição do diretório raiz:**
   ```cpp
   uint32_t rootDirSector = bs.reservedSectors + (bs.numFATs * bs.fatSize16);
   uint32_t rootDirOffset = rootDirSector * bs.bytesPerSector;
   ```

2. **Posicionar arquivo no offset:**
   ```cpp
   image.seekg(rootDirOffset);
   ```

3. **Ler cada entrada do diretório:**
   - Loop por cada entrada (até `rootEntryCount`)
   - Ler 32 bytes em uma `DirectoryEntry`
   - Ignorar entradas vazias (`filename[0] == 0x00`)
   - Ignorar entradas apagadas (`filename[0] == 0xE5`)
   - Ignorar nomes longos (`attributes == 0x0F`)

4. **Formatar e exibir:**
   ```cpp
   string name(entry.filename, 8);
   string ext(entry.extension, 3);
   // Remove espaços
   while (!name.empty() && name.back() == ' ') name.pop_back();
   while (!ext.empty() && ext.back() == ' ') ext.pop_back();
   // Exibe nome e tamanho
   cout << (ext.empty() ? name : name + "." + ext) 
        << " - " << entry.fileSize << " bytes\n";
   ```

**Saída esperada:**
```
=== Diretório Raiz ===

FILE1.TXT   Tamanho: 1024
DOCUMENT.DOC Tamanho: 2048
TEST            Tamanho: 512
```

---

### 5.3 LER ARQUIVO
**Método:** `readFile(filename)`

**Passos:**

1. **Localizar arquivo no diretório:**
   ```cpp
   DirectoryEntry fileEntry;
   if (!findFile(filename, fileEntry)) {
       return; // Arquivo não encontrado
   }
   ```

2. **Inicializar leitura:**
   ```cpp
   uint32_t remainingBytes = fileEntry.fileSize;
   uint16_t cluster = fileEntry.firstCluster;
   ```

3. **Ler cluster por cluster:**
   ```cpp
   while (cluster >= 2 && cluster < 0xFFF8 && remainingBytes > 0) {
       // Converter cluster para offset
       uint32_t offset = clusterToOffset(cluster);
       // Ler dados do cluster
       image.seekg(offset);
       vector<char> buffer(clusterSize);
       image.read(buffer.data(), clusterSize);
       // Imprimir conteúdo
       for (uint32_t i = 0; i < bytesToPrint; i++) {
           cout << buffer[i];
       }
       // Próximo cluster
       cluster = getNextCluster(cluster);
       remainingBytes -= bytesToPrint;
   }
   ```

**Pontos críticos:**
- `0xFFF8` é o valor que marca fim de arquivo na FAT
- `getNextCluster()` consulta a FAT para encontrar o próximo cluster
- Método segue a cadeia de clusters automaticamente

---

### 5.4 EXIBIR ATRIBUTOS DE ARQUIVO
**Método:** `showFileInfo(filename)`

**Informações exibidas:**
1. Nome completo (nome + extensão)
2. Tamanho em bytes
3. Primeiro cluster (onde o arquivo começa)
4. Atributos decodificados
5. Data de última modificação
6. Hora de última modificação

**Decodificação de atributos:**
```cpp
string decodeAttributes(uint8_t attr) {
    string result;
    if (attr & 0x01) result += "READ_ONLY ";   // Bit 0
    if (attr & 0x02) result += "HIDDEN ";      // Bit 1
    if (attr & 0x04) result += "SYSTEM ";      // Bit 2
    if (attr & 0x08) result += "VOLUME ";      // Bit 3
    if (attr & 0x10) result += "DIRECTORY ";   // Bit 4
    if (attr & 0x20) result += "ARCHIVE ";     // Bit 5
    return result;
}
```

**Decodificação de data FAT:**
```
Formato: 16 bits
[YYYYYYY MMMM DDDDD]
  Ano-80  Mês   Dia
```

**Decodificação de hora FAT:**
```
Formato: 16 bits
[HHHHH MMMMMM SSSSS]
Hora   Minuto  Segundo/2
```

**Saída esperada:**
```
===== INFORMACOES DO ARQUIVO =====

Nome: FILE1.TXT
Tamanho: 1024 bytes
Primeiro Cluster: 10
Atributos: ARCHIVE
Data da ultima modificacao: 15/06/2026
Hora da ultima modificacao: 14:30:00

=================================
```

---

### 5.5 RENOMEAR ARQUIVO
**Método:** `renameFile(oldName, newName)`

**Passos:**

1. **Localizar entrada no diretório:**
   - Iterar sobre entradas do diretório
   - Manter a posição (`streampos entryPos`)

2. **Converter novo nome para FAT 8.3:**
   ```cpp
   // Dividir em nome e extensão
   size_t dot = newName.find('.');
   string newBase = newName.substr(0, dot);
   string newExt = (dot != npos) ? newName.substr(dot + 1) : "";
   
   // Montar estrutura FAT 8.3
   memset(entry.filename, ' ', 8);
   memset(entry.extension, ' ', 3);
   
   for (size_t j = 0; j < min((size_t)8, newBase.size()); j++)
       entry.filename[j] = toupper(newBase[j]);
   
   for (size_t j = 0; j < min((size_t)3, newExt.size()); j++)
       entry.extension[j] = toupper(newExt[j]);
   ```

3. **Reescrever entrada no disco:**
   ```cpp
   image.seekp(entryPos);  // Volta para posição exata
   image.write(reinterpret_cast<char*>(&entry), sizeof(entry));
   image.flush();          // Garante gravação
   ```

**Importante:** 
- FAT16 usa formato **8.3**: 8 caracteres para nome, 3 para extensão
- Espaços são preenchidos automaticamente
- Tudo é convertido para MAIÚSCULAS

---

### 5.6 CRIAR ARQUIVO
**Método:** `createFile(filename, content)`

**Passos:**

1. **Converter nome para FAT 8.3:**
   ```cpp
   string base = filename;
   string ext = "";
   size_t dot = filename.find('.');
   if (dot != string::npos) {
       base = filename.substr(0, dot);
       ext = filename.substr(dot + 1);
   }
   // Preenchimento e uppercase...
   ```

2. **Encontrar cluster livre na FAT:**
   ```cpp
   uint16_t freeCluster = 0;
   uint32_t fatOffset = bs.reservedSectors * bs.bytesPerSector;
   
   for (int i = 2; i < 0xFFF0; i++) {
       uint16_t value;
       image.seekg(fatOffset + i * 2);
       image.read(reinterpret_cast<char*>(&value), 2);
       
       if (value == 0x0000) {
           freeCluster = i;  // Cluster livre!
           break;
       }
   }
   ```

3. **Marcar cluster como fim de arquivo na FAT:**
   ```cpp
   uint16_t eof = 0xFFFF;  // Marca como EOF
   image.seekp(fatOffset + freeCluster * 2);
   image.write(reinterpret_cast<char*>(&eof), 2);
   ```

4. **Encontrar slot livre no diretório raiz:**
   ```cpp
   // Procurar entrada com filename[0] == 0x00 ou 0xE5
   for (int i = 0; i < bs.rootEntryCount; i++) {
       entryPos = image.tellg();
       image.read(reinterpret_cast<char*>(&entry), sizeof(entry));
       
       if (entry.filename[0] == 0x00 ||
           (unsigned char)entry.filename[0] == 0xE5) {
           foundSlot = true;
           break;
       }
   }
   ```

5. **Criar nova entrada:**
   ```cpp
   memset(&entry, 0, sizeof(entry));
   memcpy(entry.filename, fatName, 8);
   memcpy(entry.extension, fatExt, 3);
   entry.attributes = 0x20;           // Arquivo normal
   entry.firstCluster = freeCluster;  // Aponta para novo cluster
   entry.fileSize = content.size();   // Tamanho do conteúdo
   ```

6. **Gravar entrada no diretório:**
   ```cpp
   image.seekp(entryPos);
   image.write(reinterpret_cast<char*>(&entry), sizeof(entry));
   ```

7. **Gravar dados do arquivo:**
   ```cpp
   uint32_t dataOffset = clusterToOffset(freeCluster);
   image.seekp(dataOffset);
   image.write(content.c_str(), content.size());
   image.flush();
   ```

**Fluxo resumido:**
```
1. Converter nome → 8.3
2. Encontrar cluster livre (valor 0x0000 na FAT)
3. Marcar cluster como EOF (0xFFFF)
4. Encontrar slot vazio no diretório
5. Criar DirectoryEntry com dados
6. Gravar entry no diretório
7. Gravar conteúdo no cluster
```

---

### 5.7 REMOVER ARQUIVO
**Método:** `deleteFile(filename)`

**Passos:**

1. **Localizar entrada no diretório:**
   ```cpp
   // Similar ao rename, mantém streampos entryPos
   ```

2. **Liberar todos os clusters do arquivo:**
   ```cpp
   uint16_t cluster = entry.firstCluster;
   uint32_t fatOffset = bs.reservedSectors * bs.bytesPerSector;
   
   while (cluster >= 2 && cluster < 0xFFF8) {
       uint16_t next;
       
       // Ler próximo cluster antes de sobrescrever
       image.seekg(fatOffset + cluster * 2);
       image.read(reinterpret_cast<char*>(&next), 2);
       
       // Marcar cluster como livre (0x0000)
       uint16_t free = 0x0000;
       image.seekp(fatOffset + cluster * 2);
       image.write(reinterpret_cast<char*>(&free), 2);
       
       cluster = next;
   }
   ```

3. **Marcar entrada como apagada:**
   ```cpp
   char deleted = (char)0xE5;  // Marcador de apagado
   image.seekp(entryPos);
   image.write(&deleted, 1);   // Sobrescreve primeiro byte
   image.flush();
   ```

**Pontos críticos:**
- `0xE5` no primeiro byte indica arquivo apagado
- Todos os clusters devem ser marcados como livres (0x0000)
- A entrada fica fisicamente no disco mas marcada como deletada

---

## 6. Detalhes Técnicos Avançados

### 6.1 Métodos auxiliares importantes

**`getNextCluster(uint16_t cluster)`**
```cpp
uint16_t getNextCluster(uint16_t cluster) {
    uint32_t fatOffset = bs.reservedSectors * bs.bytesPerSector;
    uint32_t entryOffset = fatOffset + cluster * 2;
    
    uint16_t nextCluster;
    image.seekg(entryOffset);
    image.read(reinterpret_cast<char*>(&nextCluster), sizeof(nextCluster));
    
    return nextCluster;
}
```

**Explicação:**
- FAT16 usa 2 bytes (16 bits) por entrada
- Cada entrada contém o número do próximo cluster
- Posição na FAT: `fatOffset + cluster * 2`

**Valores especiais:**
- `0x0000` = cluster livre
- `0x0002 até 0xFFEF` = próximo cluster
- `0xFFF8 até 0xFFFF` = fim de arquivo (EOF)

---

**`clusterToOffset(uint16_t cluster)`**
```cpp
uint32_t clusterToOffset(uint16_t cluster) {
    uint32_t rootDirSectors = 
        ((bs.rootEntryCount * 32) + (bs.bytesPerSector - 1)) / bs.bytesPerSector;
    
    uint32_t firstDataSector =
        bs.reservedSectors +
        (bs.numFATs * bs.fatSize16) +
        rootDirSectors;
    
    uint32_t sector =
        firstDataSector +
        (cluster - 2) * bs.sectorsPerCluster;
    
    return sector * bs.bytesPerSector;
}
```

**Explicação:**
- Converte número de cluster em offset de byte no arquivo
- Subtrai 2 porque clusters 0 e 1 são reservados
- Multiplica por `bytesPerSector` para obter offset em bytes

---

### 6.2 Formato de nome FAT 8.3

```
Memória:           "FILENAME EXT"
Interpretação:     FILENAME.EXT
Capacidade:        8 caracteres + 3 caracteres
Preenchimento:     espaços (0x20)
Maiúsculas:        sempre convertidas
```

**Exemplo:**
```
Entrada:   "FILE    TXT"  (8 bytes nome + 3 bytes ext)
Exibição:  FILE.TXT
```

**No código:**
```cpp
char filename[8];   // "FILE    "
char extension[3];  // "TXT"
// Após remover espaços:
// string name = "FILE"
// string ext = "TXT"
// fullName = "FILE.TXT"
```

---

### 6.3 Leitura/Escrita no arquivo

**`seekg()` - Seek Get (leitura):**
```cpp
image.seekg(offset);  // Posiciona para ler
```

**`seekp()` - Seek Put (escrita):**
```cpp
image.seekp(offset);  // Posiciona para escrever
```

**Importante:** Usar `seekp()` **antes de `write()`** garante que se escreve no local correto.

---

## 7. Fluxo de Execução - Menu Principal

```
============ FAT16 ============
1 - Abrir imagem FAT16
2 - Mostrar informacoes
3 - Listar diretorio raiz
4 - Ler arquivo
5 - Renomear arquivo
6 - Criar arquivo
7 - Remover arquivo
0 - Sair
```

**Loop:** Cada operação valida se a imagem está aberta com `isOpen()`.

---

## 8. Limitações e Considerações

### 8.1 Limitações implementadas
- ❌ Sem suporte a subdiretórios
- ❌ Apenas formato 8.3 (nomes longos não funcionam)
- ⚠️ `createFile` aloca apenas 1 cluster
  - Arquivos maiores que 1 cluster podem não funcionar corretamente
  - Solução real precisaria alocar múltiplos clusters em cadeia

### 8.2 Problemas potenciais
- Não valida espaço disponível antes de criar
- Não trata fragmentação de clusters
- Sem tratamento de erro para valores inválidos de cluster

### 8.3 Suposições
- Boot sector sempre no setor 0
- Formato FAT16 válido
- Imagem aberta permanece consistente entre operações

---

## 9. Roteiro de Apresentação

### Abertura (1-2 minutos)
1. Apresentar o objetivo: manipular FAT16
2. Mostrar as 6 operações implementadas
3. Explicar limitações (só raiz, sem subdiretórios)

### Conceitos (3-4 minutos)
1. Layout FAT16: Boot → FAT → Root → Data
2. Estruturas: `BootSector` e `DirectoryEntry`
3. Cálculo de offsets no disco

### Demonstração do código (5-7 minutos)
1. Mostrar `listRootDirectory()` - operação simples
2. Mostrar `readFile()` - cadeia de clusters
3. Mostrar `createFile()` - escrita e alocação
4. Mostrar `deleteFile()` - limpeza

### Funcionamento em tempo real (3-5 minutos)
1. Compilar: `g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp -o fat16`
2. Executar: `./fat16`
3. Demonstrar operações no menu

### Conclusão (1 minuto)
- Resumir o que foi feito
- Mencionar desafios/aprendizados

---

## 10. Perguntas Frequentes do Professor

### P: Por que usar `#pragma pack(push, 1)`?
**R:** Para garantir que a estrutura seja lida byte-a-byte sem alinhamento de memória. FAT16 tem layout fixo, e o compilador poderia adicionar padding.

### P: Como você diferencia arquivo apagado de entrada vazia?
**R:** 
- `filename[0] == 0x00` → entrada vazia (nunca usada)
- `filename[0] == 0xE5` → entrada apagada (já usada, marcada como deletada)

### P: O que significa o valor `0xFFF8` na FAT?
**R:** É o marcador de fim de arquivo (EOF). Qualquer valor de `0xFFF8` até `0xFFFF` indica o último cluster de um arquivo.

### P: Por que clusters começam em 2 e não em 0?
**R:** Clusters 0 e 1 são reservados pela especificação FAT16. O cluster lógico 2 corresponde ao primeiro cluster de dados.

### P: Como você garante que não sobrescreve dados ao criar arquivo?
**R:** Procura por um cluster marcado como `0x0000` (livre) na FAT antes de usar.

### P: E se o diretório raiz ficar cheio?
**R:** O código retorna erro "Diretório raiz cheio". Em FAT16 real, não é possível estender o diretório raiz.

---

## 11. Checklist para Apresentação

- [ ] Código compilado e funcionando
- [ ] Imagem FAT16 de teste disponível
- [ ] Conceitos de boot sector e FAT memorizados
- [ ] Exemplos práticos de offset prontos
- [ ] Entender fluxo de: criar → ler → renomear → deletar
- [ ] Saber explicar `getNextCluster()` e `clusterToOffset()`
- [ ] Respostas às 6 perguntas frequentes memoridas

---

## 12. Dicionário de Termos

| Termo | Explicação |
|-------|-----------|
| **Cluster** | Menor unidade de alocação no disco (múltiplos setores) |
| **FAT** | Tabela de Alocação de Arquivo - mapa de clusters |
| **Boot Sector** | Primeiro setor do disco com informações do sistema |
| **Root Directory** | Diretório raiz do disco |
| **Offset** | Posição em bytes dentro do arquivo |
| **Setor** | Unidade de leitura do disco (geralmente 512 bytes) |
| **EOF** | End of File - marca fim de arquivo |
| **Atributo** | Flag que indica propriedades do arquivo |
| **Entrada** | Estrutura de 32 bytes que representa arquivo no diretório |

---

**Boa apresentação! 🎓**
