# Roteiro Prático - Apresentação FAT16

## Tempo Total: 15-20 minutos

---

## PARTE 1: Introdução (2 minutos)

### Slide de abertura
**Título:** Manipulador de Sistema de Arquivos FAT16

**O que você fala:**
"Desenvolvemos um programa em C++ que permite manipular uma imagem de disco FAT16. O programa abre a imagem, interpreta sua estrutura interna e realiza operações como criar, ler, renomear e deletar arquivos - tudo sem sair da aplicação através de um menu interativo."

### Contextualize o problema
**Diga:** 
- Trabalhamos com um arquivo binário que contém uma imagem FAT16
- Não podemos usar funções do SO diretamente
- Precisamos implementar tudo manualmente: leitura de estruturas, navegação pela FAT, manipulação de clusters

---

## PARTE 2: Arquitetura do Sistema FAT16 (3 minutos)

### Desenhe o layout (ou mostre diagrama)

```
Offset 0     [Boot Sector]     ← Informações do sistema (512 bytes)
             [FAT 1]            ← Tabela de alocação, entradas de 2 bytes
             [FAT 2]            ← Cópia redundante
             [Root Directory]   ← Entradas de arquivos
             [Data Area]        ← Dados dos arquivos (clusters)
```

### Explique cada parte:

**1. Boot Sector (512 bytes)**
- Primeiros 3 bytes: instrução de jump
- Informações críticas:
  - Bytes por setor: geralmente 512
  - Setores por cluster: define tamanho dos clusters
  - Número de FATs: geralmente 2
  - Tamanho da FAT: quantos setores ocupa

**Mostre no código:**
```cpp
struct BootSector {
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t fatSize16;
    // ...
};
```

**2. FAT (File Allocation Table)**
- Mapa que rastreia clusters
- Cada entrada tem 2 bytes (16 bits)
- Valores especiais:
  - `0x0000` = cluster livre
  - `0xFFF8-0xFFFF` = fim de arquivo

**Exemplo visual:**
```
FAT Index → Valor → Significado
    2     → 3     → Próximo cluster é o 3
    3     → 4     → Próximo cluster é o 4
    4     → 0xFFFF → Fim do arquivo
```

**3. Root Directory**
- Máximo de 224 entradas (em FAT16 padrão)
- Cada entrada = 32 bytes
- Campos importantes:
  - Nome (8 bytes) + Extensão (3 bytes)
  - Atributos (1 byte)
  - Primeiro cluster (2 bytes)
  - Tamanho do arquivo (4 bytes)

**4. Data Area**
- Clusters contêm os dados reais dos arquivos
- Clusters não estão necessariamente contíguos
- Usamos a FAT para seguir a cadeia

---

## PARTE 3: Estruturas de Dados (2 minutos)

### Mostre as estruturas que representam FAT16

```cpp
struct BootSector {
    uint8_t jump[3];
    char oem[8];
    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t numFATs;
    uint16_t rootEntryCount;
    // ... 14 bytes mais
};

struct DirectoryEntry {
    char filename[8];
    char extension[3];
    uint8_t attributes;
    uint8_t reserved[10];
    uint16_t time;
    uint16_t date;
    uint16_t firstCluster;
    uint32_t fileSize;
};
```

**Explicação:**
- `#pragma pack(push, 1)` → sem alinhamento de memória
- Isso permite `image.read((char*)&bs, sizeof(bs))` funcionar corretamente

**Mostre a relação:**
```cpp
BootSector bs;
DirectoryEntry entry;

image.read((char*)&bs, 512);        // Lê boot sector
// Depois usa bs.reservedSectors, bs.fatSize16 para navegar
image.read((char*)&entry, 32);      // Lê uma entrada do diretório
// Depois usa entry.firstCluster para acessar dados
```

---

## PARTE 4: Operações - Demonstração Prática (8-10 minutos)

### Operação 1: LISTAR ARQUIVOS (1 minuto)

**O que faz:** Mostra todos os arquivos no diretório raiz

**Fluxo:**
```
1. Calcular onde começa o diretório raiz
   rootDirOffset = (reservedSectors + numFATs * fatSize16) * bytesPerSector

2. Posicionar no arquivo (seek)
   image.seekg(rootDirOffset)

3. Ler entrada por entrada (32 bytes cada)
   for (int i = 0; i < rootEntryCount; i++) {
       image.read(&entry, 32)
       // Verificar se é válido
       if (entry.filename[0] == 0x00) break;      // Vazio
       if (entry.filename[0] == 0xE5) continue;   // Apagado
       // Exibir
   }
```

**Por que importante:** Primeiro passo para entender a estrutura do disco.

---

### Operação 2: EXIBIR ATRIBUTOS (1 minuto)

**O que faz:** Mostra informações detalhadas de um arquivo

**Informações:**
- Nome e extensão
- Tamanho
- Primeiro cluster (onde começa na area de dados)
- Atributos (READ_ONLY, HIDDEN, SYSTEM, etc.)
- Data/hora de modificação

**Decodificação de atributos:**
```cpp
uint8_t attr = entry.attributes;

if (attr & 0x01) cout << "READ_ONLY\n";
if (attr & 0x02) cout << "HIDDEN\n";
if (attr & 0x04) cout << "SYSTEM\n";
if (attr & 0x20) cout << "ARCHIVE\n";
```

**Decodificação de data:**
```
Entrada na FAT: 0x3B24 (exemplo)
Binário: 0011 1011 0010 0100

Dia:    00111 = 7
Mês:    1011 = 11
Ano:    00100 = 4 → 1980 + 4 = 1984

Resultado: 07/11/1984
```

---

### Operação 3: LER ARQUIVO (2 minutos) ⭐ MAIS IMPORTANTE

**O que faz:** Mostra o conteúdo de um arquivo lendo clusters

**Conceito chave: Cadeia de clusters**
```
Arquivo tem 3000 bytes, 1024 bytes por cluster
Clusters necessários: 3

DirectoryEntry.firstCluster = 10

FAT[10] = 11       ← cluster 10 aponta para 11
FAT[11] = 12       ← cluster 11 aponta para 12
FAT[12] = 0xFFFF   ← cluster 12 é o fim
```

**Fluxo no código:**
```cpp
uint16_t cluster = entry.firstCluster;  // Começa em 10

while (cluster >= 2 && cluster < 0xFFF8) {
    // Converter cluster em offset
    uint32_t offset = clusterToOffset(cluster);
    
    // Ler dados
    image.seekg(offset);
    vector<char> buffer(1024);
    image.read(buffer.data(), 1024);
    
    // Exibir dados
    cout << string(buffer.begin(), buffer.end());
    
    // Próximo cluster
    cluster = getNextCluster(cluster);  // Lê FAT
}
```

**Funções auxiliares cruciais:**

**getNextCluster():**
```cpp
uint16_t getNextCluster(uint16_t cluster) {
    uint32_t fatOffset = reservedSectors * bytesPerSector;
    uint32_t entryPos = fatOffset + cluster * 2;
    
    uint16_t next;
    image.seekg(entryPos);
    image.read(&next, 2);
    return next;
}
// Essencialmente: FAT[cluster]
```

**clusterToOffset():**
```cpp
uint32_t clusterToOffset(uint16_t cluster) {
    // Calcula onde no arquivo esse cluster começa
    uint32_t firstDataSector = reservedSectors + numFATs*fatSize16 + rootDirSectors;
    uint32_t sector = firstDataSector + (cluster - 2) * sectorsPerCluster;
    return sector * bytesPerSector;
}
```

**Exemplo concreto:**
```
Ler arquivo de 100 bytes no cluster 5:

1. entry.firstCluster = 5
2. offset = clusterToOffset(5) = 10240 (exemplo)
3. image.seekg(10240)
4. Ler 100 bytes
5. nextCluster = getNextCluster(5)
6. Se nextCluster == 0xFFFF, pronto!
7. Senão, repetir com novo cluster
```

---

### Operação 4: RENOMEAR ARQUIVO (1 minuto)

**O que faz:** Muda o nome de um arquivo

**Passos:**
1. Localizar a entrada no diretório (manter posição)
2. Converter novo nome para formato FAT 8.3
3. Reescrever a entrada

**Formato 8.3:**
```
Nome:      8 caracteres (preenchidos com espaço)
Extensão:  3 caracteres (preenchidos com espaço)
Maiúsculas: sempre

Exemplo:
"test.txt" → filename="TEST    ", extension="TXT"
"file"     → filename="FILE    ", extension="   "
```

**Código chave:**
```cpp
memset(entry.filename, ' ', 8);   // Preencher com espaços
memset(entry.extension, ' ', 3);

for (size_t i = 0; i < min((size_t)8, newName.size()); i++)
    entry.filename[i] = toupper(newName[i]);

image.seekp(entryPos);  // IMPORTANTE: seekp, não seekg!
image.write(&entry, 32);
image.flush();
```

---

### Operação 5: CRIAR ARQUIVO (2 minutos) ⭐ MAIS COMPLEXA

**O que faz:** Cria um novo arquivo com conteúdo fornecido

**Passos:**

**Passo 1: Encontrar cluster livre**
```cpp
for (int i = 2; i < 0xFFF0; i++) {
    uint16_t value;
    image.seekg(fatOffset + i*2);
    image.read(&value, 2);
    
    if (value == 0x0000) {  // Livre!
        freeCluster = i;
        break;
    }
}
```

**Passo 2: Marcar cluster como EOF**
```cpp
uint16_t eof = 0xFFFF;
image.seekp(fatOffset + freeCluster*2);
image.write(&eof, 2);
// FAT[freeCluster] = 0xFFFF
```

**Passo 3: Encontrar slot vazio no diretório**
```cpp
for (int i = 0; i < rootEntryCount; i++) {
    image.read(&entry, 32);
    
    if (entry.filename[0] == 0x00 ||          // Nunca usado
        (unsigned char)entry.filename[0] == 0xE5) {  // Apagado
        foundSlot = true;
        break;
    }
}
```

**Passo 4: Criar nova entrada**
```cpp
memset(&entry, 0, 32);
strcpy(entry.filename, "FILE    ");
strcpy(entry.extension, "TXT");
entry.attributes = 0x20;           // Arquivo normal
entry.firstCluster = freeCluster;  // Aponta para o cluster alocado
entry.fileSize = conteudo.size();
```

**Passo 5: Gravar entrada no diretório**
```cpp
image.seekp(entryPos);
image.write(&entry, 32);
```

**Passo 6: Gravar dados no cluster**
```cpp
uint32_t dataOffset = clusterToOffset(freeCluster);
image.seekp(dataOffset);
image.write(conteudo.c_str(), conteudo.size());
image.flush();
```

**Resumo visual:**
```
FAT antes:  [..., 0, 0, ...]  (cluster vazio)
FAT depois: [..., 0xFFFF, 0, ...] (cluster alocado)

Diretório:  nova entrada com firstCluster apontando pro cluster

Data Area:  cluster contém o conteúdo do arquivo
```

---

### Operação 6: REMOVER ARQUIVO (1 minuto)

**O que faz:** Apaga um arquivo

**Passos:**

**Passo 1: Localizar arquivo**
```cpp
// Encontrar entry e manter entryPos
```

**Passo 2: Liberar clusters**
```cpp
uint16_t cluster = entry.firstCluster;

while (cluster >= 2 && cluster < 0xFFF8) {
    uint16_t next;
    image.seekg(fatOffset + cluster*2);
    image.read(&next, 2);
    
    // Marcar cluster como livre
    uint16_t free = 0x0000;
    image.seekp(fatOffset + cluster*2);
    image.write(&free, 2);
    
    cluster = next;
}
```

**Passo 3: Marcar entrada como apagada**
```cpp
char deleted = 0xE5;
image.seekp(entryPos);
image.write(&deleted, 1);
image.flush();
```

**Resultado:**
```
FAT: clusters marcados como 0x0000 (livres)
Diretório: entrada.filename[0] = 0xE5 (marcada como deletada)
Dados: Não são fisicamente apagados, apenas marcados como reutilizáveis
```

---

## PARTE 5: Demonstração ao Vivo (5 minutos)

### Terminal - Execute passo a passo

```bash
$ g++ -std=c++17 -O2 -Wall -Wextra -pedantic main.cpp -o fat16
$ ./fat16

========== FAT16 ==========
1 - Abrir imagem FAT16
2 - Mostrar informacoes
3 - Listar diretorio raiz
4 - Ler arquivo
5 - Renomear arquivo
6 - Criar arquivo
7 - Remover arquivo
0 - Sair
Opcao: 1
Nome da imagem: disk.img
Imagem aberta com sucesso!

Opcao: 3
=== Diretório Raiz ===

README.TXT   Tamanho: 512
TEST.C       Tamanho: 1024

Opcao: 4
Nome do arquivo: README.TXT
===== CONTEUDO =====
[Conteúdo do arquivo exibido]
====================

Opcao: 2
Nome do arquivo: README.TXT
===== INFORMACOES DO ARQUIVO =====
Nome: README.TXT
Tamanho: 512 bytes
Primeiro Cluster: 10
Atributos: ARCHIVE
Data da ultima modificacao: 15/06/2026
Hora da ultima modificacao: 14:30:00
=================================

Opcao: 6
Nome do arquivo: NOVO.TXT
Conteudo: Hello World!
Arquivo criado com sucesso!

Opcao: 3
=== Diretório Raiz ===
README.TXT   Tamanho: 512
TEST.C       Tamanho: 1024
NOVO.TXT     Tamanho: 12

Opcao: 5
Nome atual: NOVO.TXT
Novo nome: BACKUP.TXT
Arquivo renomeado com sucesso!

Opcao: 7
Nome do arquivo: BACKUP.TXT
Arquivo removido com sucesso!

Opcao: 0
Encerrando...
```

---

## PARTE 6: Explicação das Limitações (1 minuto)

**Diga:**
"O programa tem algumas limitações propositais:

1. **Apenas diretório raiz** - Não há suporte a subdiretórios. Todos os arquivos estão no mesmo nível.

2. **Formato 8.3** - Nomes são limitados a 8 caracteres + 3 de extensão. Nomes longos não funcionam.

3. **Criação simples** - Quando criamos um arquivo, alocamos apenas 1 cluster. Se o arquivo for maior que 1 cluster, precisamos de uma cadeia de clusters (múltiplas alocações). Isso é suportado na leitura, mas na criação é mais simples.

4. **Sem tratamento de erro robusto** - Em um programa real, teríamos mais validações e tratamento de exceções.

Apesar disso, o programa demonstra os conceitos fundamentais do FAT16."

---

## PARTE 7: Conclusão (1 minuto)

**Resumir:**
- Implementamos um manipulador FAT16 completo
- Cobrimos as 6 operações obrigatórias
- Demonstramos compreensão da estrutura interna

**Fechamento:**
"O projeto mostrou na prática como sistemas operacionais reais gerenciam armazenamento em disco. Cada operação que implementamos envolve cálculos precisos de offsets e manipulação cuidadosa de estruturas binárias."

---

## Respostas Memoráveis para Perguntas

### P: "Por que `0xE5` para arquivo apagado?"
**R:** "É uma convenção da especificação FAT. Quando marcamos um arquivo como apagado, apenas sobrescrevemos o primeiro byte com `0xE5`. Os dados e clusters permanecem no disco até serem reutilizados. Por isso `0x00` é diferente - `0x00` significa que a entrada nunca foi usada."

### P: "Como você sabe onde termina um arquivo se os clusters não são contíguos?"
**R:** "Usa-se a FAT como um mapa. Cada cluster aponta para o próximo. Quando encontramos o valor `0xFFF8` até `0xFFFF`, sabemos que é o fim. Por exemplo: cluster 5 → FAT[5] = 10 → FAT[10] = 0xFFFF. Pronto!"

### P: "E se dois arquivos forem alocados com clusters intercalados?"
**R:** "Exatamente! Essa é a fragmentação. FAT16 segue a cadeia de cada arquivo independentemente. Cluster 5 do arquivo A pode estar fisicamente ao lado de cluster 3 do arquivo B. A FAT cuida de rastreá-los."

### P: "Por que usar `#pragma pack` em vez de struct normal?"
**R:** "Sem `#pragma pack`, o compilador poderia adicionar padding (bytes extras) entre campos para alinhamento. Isso arruinaria nossa leitura binária. Com `pack(1)`, a estrutura ocupa exatamente 512 bytes (boot) ou 32 bytes (entrada), como esperado."

### P: "Como você diferencia um cluster livre de um apagado?"
**R:** "Na FAT: livre é `0x0000`. Na entrada de diretório: apagado é primeiro byte = `0xE5`. São contextos diferentes - FAT vs. diretório."

### P: "Qual é a maior limitação deste programa?"
**R:** "Provavelmente ser limitado ao diretório raiz. FAT16 real suporta árvore de diretórios infinita. Implementar isso exigiria recursão e mais lógica. Mas para demonstrar os conceitos, o diretório raiz é suficiente."

---

## Dicas de Apresentação

✅ **DO's:**
- Fale claramente e pausadamente
- Mostre código uma função de cada vez
- Use o terminal para demonstração ao vivo
- Tenha exemplos concretos prontos (clusters, offsets)
- Mantenha contato visual com o professor

❌ **DON'Ts:**
- Não leia código diretamente sem explicar
- Não pule as operações simples (listar, ler)
- Não fale muito rápido sobre FAT/clusters
- Não esqueça de compilar e testar antes
- Não tente impressionar com complexidade desnecessária

---

**Boa sorte na apresentação! 🚀**
