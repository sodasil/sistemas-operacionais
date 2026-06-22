# Perguntas Técnicas e Desafios - FAT16

## Nível 1: Fundamentais (Obrigatório dominar)

### 1. Estruturas de Dados

**P: Qual é o tamanho exato em bytes de uma `DirectoryEntry`?**
R: 32 bytes.
```
filename:    8 bytes
extension:   3 bytes
attributes:  1 byte
reserved:    10 bytes
time:        2 bytes
date:        2 bytes
firstCluster: 2 bytes
fileSize:    4 bytes
-----------
TOTAL:       32 bytes
```

**P: Por que a extensão tem apenas 3 bytes?**
R: Convenção FAT16 - formato 8.3 (8 caracteres de nome + 3 de extensão). É uma limitação da especificação, não do programa.

**P: Qual é o significado do `#pragma pack(push, 1)` e `#pragma pack(pop)`?**
R: `push, 1` desativa alinhamento de memória (packing = 1 byte). `pop` restaura o alinhamento anterior. Isso garante que a estrutura ocupe exatamente o número de bytes especificado.

**P: Qual seria o problema se não usássemos `#pragma pack`?**
R: O compilador poderia adicionar padding (bytes vazios) entre campos para alinhamento. A estrutura ocuparia mais de 32 bytes, e `image.read((char*)&entry, 32)` leria dados incorretos.

---

### 2. Layout do Disco

**P: Como calcular o offset absoluto da FAT?**
R: `fatOffset = reservedSectors * bytesPerSector`

**Exemplo:**
- `reservedSectors = 1`
- `bytesPerSector = 512`
- `fatOffset = 1 * 512 = 512`
- FAT começa no byte 512

**P: Como calcular o offset absoluto do diretório raiz?**
R: 
```
rootDirSector = reservedSectors + (numFATs * fatSize16)
rootDirOffset = rootDirSector * bytesPerSector
```

**Exemplo:**
- `reservedSectors = 1`
- `numFATs = 2`
- `fatSize16 = 9`
- `bytesPerSector = 512`
- `rootDirSector = 1 + (2 * 9) = 19`
- `rootDirOffset = 19 * 512 = 9728`

**P: Se o arquivo FAT16 tem 1024 entradas no diretório raiz, quantos bytes ele ocupa?**
R: `1024 * 32 = 32768 bytes = 64 setores`

**P: Quantos clusters são necessários para um arquivo de 5000 bytes se cada cluster tem 1024 bytes?**
R: `ceil(5000 / 1024) = 5 clusters`

---

### 3. Nomes em Formato 8.3

**P: Como o nome "documento.doc" é armazenado em FAT16?**
R:
```
filename:  "DOCUMENT"  (8 bytes, espaços se necessário)
extension: "DOC"       (3 bytes)
Armazenamento: "DOCUMENTDOC"
Exibição:      "DOCUMENT.DOC"
```

**P: Como armazenar "readme" (sem extensão)?**
R:
```
filename:  "README  " (preenchido com espaços até 8)
extension: "   "      (3 espaços)
Exibição:  "README"
```

**P: Qual é o comprimento máximo de um nome de arquivo?**
R: 12 caracteres (8 do nome + 1 ponto + 3 da extensão). Mas armazenados sem o ponto.

---

### 4. Leitura Básica

**P: Qual é o valor de cluster que marca "fim de arquivo"?**
R: Qualquer valor de `0xFFF8` até `0xFFFF` (inclusive). Mais comum é `0xFFFF`.

**P: Qual é o valor que marca um cluster como livre?**
R: `0x0000`

**P: Qual é o valor reservado que não deveria aparecer?**
R: `0xFFF7` - marca setores ruins (bad sectors).

**P: Se a FAT contém [0, 0, 5, 8, 0xFFFF, 0, 0, ...], qual é a cadeia de um arquivo que começa no cluster 3?**
R: 3 → 8 → 0xFFFF (fim)

O arquivo tem 2 clusters (3 e 8).

---

## Nível 2: Intermediário (Bom ter)

### 5. Cálculos de Offset

**P: Explique a fórmula de `clusterToOffset`:**
R:
```cpp
uint32_t rootDirSectors = 
    ((rootEntryCount * 32) + (bytesPerSector - 1)) / bytesPerSector;
// Calcula quantos setores o diretório raiz ocupa

uint32_t firstDataSector =
    reservedSectors + (numFATs * fatSize16) + rootDirSectors;
// Primeira posição onde dados de usuário podem estar

uint32_t sector =
    firstDataSector + (cluster - 2) * sectorsPerCluster;
// Localiza o setor do cluster (subtraindo 2 pois clusters 0 e 1 são reservados)

return sector * bytesPerSector;
// Converte em offset de byte
```

**P: Por que subtrair 2 do cluster?**
R: Clusters 0 e 1 são reservados na especificação FAT16. O primeiro cluster de dados é o cluster 2. Então para localizar fisicamente o cluster N, usamos `(N - 2) * sectorsPerCluster`.

**P: Se um arquivo tem 3000 bytes e cada cluster tem 512 bytes, quantos clusters são usados?**
R: `ceil(3000 / 512) = 6 clusters`

**Exemplo de cadeia:**
```
Cluster 5: 0-512 bytes
Cluster 6: 512-1024 bytes
Cluster 7: 1024-1536 bytes
Cluster 8: 1536-2048 bytes
Cluster 9: 2048-2560 bytes
Cluster 10: 2560-3000 bytes (apenas 440 bytes usados)

FAT:
FAT[5] = 6
FAT[6] = 7
FAT[7] = 8
FAT[8] = 9
FAT[9] = 10
FAT[10] = 0xFFFF
```

---

### 6. Operações de Leitura

**P: Qual é a diferença entre `seekg` e `seekp`?**
R:
- `seekg` (get) posiciona para **leitura**
- `seekp` (put) posiciona para **escrita**

**Importante:** Sempre usar `seekp` antes de `write()`, caso contrário pode escrever no lugar errado.

**P: Qual é a sequência correta para ler um arquivo do disco?**
R:
1. `image.seekg(offset)` - posiciona para ler
2. `image.read(buffer, size)` - lê dados
3. Processar dados no buffer

**Errado:** Tentar ler sem seekg se o arquivo pointer estava em outro lugar.

**P: Como verificar se uma entrada de diretório é um arquivo válido?**
R:
```cpp
if (entry.filename[0] == 0x00)        // Não usada
    continue;
if ((unsigned char)entry.filename[0] == 0xE5)  // Apagada
    continue;
if (entry.attributes == 0x0F)         // Nome longo
    continue;
// Agora é válida
```

---

### 7. Decodificação de Data/Hora

**P: Como decodificar a data `0x5335`?**
R:
```
Binário: 0101 0011 0011 0101
         YYYY YYYMMMDD DDD

Dia:    00101 = 5
Mês:    0011 = 3
Ano:    01001 = 9 → 1980 + 9 = 1989

Resultado: 05/03/1989
```

**P: Como decodificar a hora `0x7123`?**
R:
```
Binário: 0111 0001 0010 0011
         HHHH HMMM MMSSS SS

Horas:  01110 = 14
Minutos: 001001 = 9
Segundos: 00011 = 3 × 2 = 6

Resultado: 14:09:06
```

**P: Qual é a hora mais cedo que pode ser armazenada?**
R: `00:00:00` (000 0000 000 00 = 0x0000)

**P: Qual é a hora mais tarde?**
R: `23:59:58` (10111 111011 111110 = 0xBFBE)

---

### 8. Atributos de Arquivo

**P: Como decodificar o atributo `0x27`?**
R:
```
0x27 = 0010 0111

Bit 0: 1 = READ_ONLY
Bit 1: 1 = HIDDEN
Bit 2: 1 = SYSTEM
Bit 3: 0 = -
Bit 4: 0 = -
Bit 5: 1 = ARCHIVE

Resultado: READ_ONLY, HIDDEN, SYSTEM, ARCHIVE
```

**P: Como verificar se um arquivo é somente leitura?**
R:
```cpp
if (entry.attributes & 0x01) {
    cout << "Arquivo é somente leitura\n";
}
```

**P: Como definir um arquivo como oculto?**
R:
```cpp
entry.attributes |= 0x02;  // Set bit 1
```

---

## Nível 3: Avançado (Para impressionar)

### 9. Operações Complexas

**P: Qual é o algoritmo exato para criar um arquivo?**
R:
1. Converter nome para 8.3
2. Procurar na FAT por um cluster com valor `0x0000`
3. Marcar esse cluster com `0xFFFF` (EOF)
4. Procurar uma entrada livre no diretório (filename[0] == 0x00 ou 0xE5)
5. Preencher nova `DirectoryEntry`:
   - Nome e extensão em formato 8.3
   - `attributes = 0x20` (arquivo)
   - `firstCluster = cluster livre`
   - `fileSize = tamanho do conteúdo`
   - `time` e `date` poderiam ser preenchidos com data atual
6. Gravar a entrada no diretório
7. Gravar dados no cluster alocado
8. Flush para garantir escrita

**P: E se não houver cluster livre?**
R: Retornar erro "Disco cheio" ou similar.

**P: E se o diretório raiz estiver cheio?**
R: Retornar erro "Diretório raiz cheio". Em FAT16, o diretório raiz tem tamanho fixo.

**P: Qual é o algoritmo exato para deletar um arquivo?**
R:
1. Localizar entrada no diretório (manter streampos)
2. Ler `firstCluster` da entrada
3. Loop: seguir a cadeia de clusters
   - Ler valor atual da FAT
   - Gravar `0x0000` (liberar cluster)
   - Ir para próximo cluster
   - Parar quando encontrar `0xFFF8` ou além
4. Marcar primeira posição da entrada com `0xE5`
5. Flush

**P: Por que não podemos simplesmente escrever zeros na area de dados?**
R: Porque em FAT16, dados já existentes podem ser reutilizados. A entrada é marcada como deletada, mas dados físicos não são apagados (recuperação de dados). Isso é muito mais rápido que apagar tudo.

---

### 10. Problemas Potenciais

**P: O que acontece se lermos um cluster com número inválido (ex: 0xFFFF)?**
R: `clusterToOffset` calcularia um offset absurdo fora do arquivo, causando leitura incorreta ou crash. Deveria validar antes.

**P: E se criarmos um arquivo com 2048 bytes mas o cluster tiver apenas 1024 bytes?**
R: O arquivo ocuparia 2 clusters. Mas nosso `createFile` simples aloca apenas 1. Dados sobrescreveriam o próximo cluster!

**Solução real:** Implementar loop de alocação múltipla.

**P: Como a fragmentação afeta a performance?**
R: Se um arquivo de 1 MB está espalhado em clusters não contíguos, a cabeça do disco precisa se mover muitas vezes, tornando a leitura lenta.

**P: Como reparar fragmentação?**
R: Implementando um defragmentador que realoca clusters em ordem contígua.

---

### 11. Casos Extremos

**P: O que acontece se tentar renomear um arquivo para um nome que já existe?**
R: O programa atual sobrescreveria a entrada. Seria melhor validar primeiro.

**P: E se renomear para um nome vazio?**
R: Formato 8.3 não permite. Mas o programa aceitaria espaços em branco - resultado: arquivo ilegível.

**P: Qual é o arquivo mais pequeno possível?**
R: 0 bytes. `fileSize = 0`. Ocuparia 1 entry no diretório mas 0 clusters de dados.

**P: E o maior?**
R: Com FAT16: máximo ~2 GB (para 512 bytes/setor, 32 KB/cluster).

---

### 12. Comparações com Sistemas Reais

**P: Como FAT16 se compara com FAT32?**
R:
- FAT16: clusters de 2 bytes, tamanho máximo ~2 GB
- FAT32: clusters de 4 bytes, tamanho máximo ~2 TB
- FAT32 usa mais clusters, melhor para discos grandes

**P: Como se compara com NTFS?**
R:
- NTFS: muito mais complexo, suporta permissões, journaling, criptografia
- FAT16/32: simples, compatível com qualquer SO
- NTFS: mais moderno e seguro

**P: Por que ainda usamos FAT em pendrives?**
R: Compatibilidade universal. FAT16/32 funcionam em Windows, Mac, Linux, câmeras, TVs.

---

## Desafios Práticos

### Desafio 1: Fragmentação
**Imagine:** Você cria 3 arquivos A, B, C de 100 bytes cada em um disco com cluster de 512 bytes.
**Pergunta:** Quantos clusters cada um usa? Desenhe a FAT resultante.
**Resposta:**
```
Cada arquivo: 1 cluster (menos de 512 bytes)
FAT:
Index Value
2     0xFFFF (A)
3     0xFFFF (B)
4     0xFFFF (C)
5     0x0000 (livre)
...
```

### Desafio 2: Cadeia Longa
**Imagine:** Arquivo de 10 MB em clusters de 512 bytes.
**Pergunta:** Quantos clusters tem? Como a FAT se parece?
**Resposta:**
```
10 MB = 10485760 bytes
10485760 / 512 = 20480 clusters

FAT teria sequência:
FAT[2] = 3
FAT[3] = 4
FAT[4] = 5
...
FAT[20481] = 0xFFFF
```

### Desafio 3: Recuperação
**Imagine:** Você deleta um arquivo por acidente.
**Pergunta:** Quais dados mudaram? É possível recuperar?
**Resposta:**
- FAT não mudou (clusters ainda têm valores antigos)
- Entrada marcada com `0xE5`
- **Sim, é possível:** se os clusters não foram reutilizados, você pode restaurar a entrada e reativar os clusters

### Desafio 4: Otimização
**Pergunta:** Como você implementaria alocação de múltiplos clusters na criação?
**Resposta:**
```cpp
int clustersNeeded = (fileSize + clusterSize - 1) / clusterSize;
int firstCluster = -1, prevCluster = -1;

for (int i = 0; i < clustersNeeded; i++) {
    // Encontrar cluster livre
    int freeCluster = findFreeCluster();
    
    if (firstCluster == -1) {
        firstCluster = freeCluster;  // Marcar início
    }
    
    if (prevCluster != -1) {
        FAT[prevCluster] = freeCluster;  // Link anterior
    }
    
    FAT[freeCluster] = (i == clustersNeeded - 1) ? 0xFFFF : 0; // EOF ou será substituído
    prevCluster = freeCluster;
}
```

---

## Respostas Rápidas para Testes

**T: FAT significa?** 
R: File Allocation Table

**T: Quanto tempo manter para deletar um arquivo?**
R: Apenas o tempo de marcar como `0xE5` (muito rápido)

**T: Qual struct pode ter `0xE5` como atributo?**
R: Nenhuma - `0xE5` só aparece como primeiro byte de filename

**T: O programa usa threads?**
R: Não, é single-threaded

**T: É possível fragmentação no diretório raiz?**
R: Não, o diretório raiz é contíguo

**T: Quantas FATs existem?**
R: 2 (para redundância/backup)

**T: Se FAT1 corromper, usar FAT2?**
R: Sim, esse é o propósito da redundância

---

## Estude a Si Mesmo

**Antes da apresentação, responda sem consultar:**

- [ ] Tamanho exato de cada estrutura (32 e 512)
- [ ] Diferença entre `0x00` e `0xE5` em filename[0]
- [ ] Fórmula de `rootDirOffset`
- [ ] O que significa `0xFFFF` na FAT
- [ ] Como calcular número de clusters necessários
- [ ] Decodificação de data/hora com exemplo
- [ ] Algoritmo de criação (6 passos)
- [ ] Algoritmo de deleção (4 passos)
- [ ] O que é `#pragma pack` e por quê
- [ ] Diferença entre `seekg` e `seekp`

Se conseguir responder 8 de 10, está pronto! 🎓
