# 🎨 Referência Visual - FAT16 Diagramas e Resumos

## Layout do Disco FAT16

```
┌─────────────────────────────────────────┐
│     IMAGEM FAT16 (arquivo binário)       │
├─────────────────────────────────────────┤
│                                          │
│  BOOT SECTOR (512 bytes)                │  ← Setor 0
│  ├─ bytesPerSector (512)                │
│  ├─ sectorsPerCluster (8)               │
│  ├─ reservedSectors (1)                 │
│  ├─ numFATs (2)                         │
│  ├─ rootEntryCount (224)                │
│  └─ fatSize16 (9)                       │
│                                          │
│  FAT 1 (Tabela de Alocação)             │  ← Setores 1-9
│  ├─ [0] = 0x0000 (reservado)            │
│  ├─ [1] = 0x0000 (reservado)            │
│  ├─ [2] = 0x0003 (aponta para 3)        │
│  ├─ [3] = 0x0004                        │
│  ├─ [4] = 0xFFFF (fim)                  │
│  ├─ [5] = 0x0000 (livre)                │
│  └─ ...                                  │
│                                          │
│  FAT 2 (Cópia de FAT 1)                 │  ← Setores 10-18
│  └─ (idêntica à FAT 1)                  │
│                                          │
│  ROOT DIRECTORY (224 entradas × 32)    │  ← Setores 19-31
│  ├─ [0] Entry: FILE1    | 512 bytes    │
│  │        firstCluster = 2               │
│  ├─ [1] Entry: README.TXT | 1024 bytes │
│  │        firstCluster = 5               │
│  ├─ [2] Entry: 0x00 (vazio)            │
│  └─ ...                                  │
│                                          │
│  DATA AREA (clusters de dados)          │  ← Setor 32 em diante
│  ├─ Cluster 2: dados de FILE1          │
│  ├─ Cluster 3: continuação de FILE1    │
│  ├─ Cluster 4: continuação de FILE1    │
│  ├─ Cluster 5: dados de README.TXT     │
│  ├─ Cluster 6: (livre)                 │
│  └─ ...                                  │
└─────────────────────────────────────────┘
```

---

## Cadeia de Clusters - Exemplo Visual

### Arquivo: FILE1 (1500 bytes, cluster = 512 bytes)

```
Directory Entry para FILE1:
┌──────────────────────────────┐
│ filename: "FILE    "         │
│ extension: "   "             │
│ firstCluster: 2 ◄────┐       │
│ fileSize: 1500       │       │
└──────────────────────┼───────┘
                       │
                       ▼
FAT 1 (Map dos clusters):
┌─────────────────────────────┐
│ FAT[0] = 0x0000 (reservado) │
│ FAT[1] = 0x0000 (reservado) │
│ FAT[2] = 3     ◄─ 1º cluster│
│ FAT[3] = 4     ◄─ 2º cluster│
│ FAT[4] = 0xFFFF ◄─ 3º/último│
│ FAT[5] = 0x0000 (livre)     │
└─────────────────────────────┘

Leitura do arquivo:
┌──────────┐    ┌──────────┐    ┌──────────┐
│ Cluster 2│ ─► │ Cluster 3│ ─► │ Cluster 4│
│ Bytes    │    │ Bytes    │    │ Bytes    │
│ 0-511    │    │ 512-1023 │    │ 1024-1499│
│ (512B)   │    │ (512B)   │    │ (476B)   │
└──────────┘    └──────────┘    └──────────┘
      │               │                │
      └─ Total: 1500 bytes ─────────────┘
```

---

## Estrutura de DirectoryEntry (32 bytes)

```
Offset  Tamanho  Campo            Valor Exemplo
────────────────────────────────────────────
0       8 bytes  filename         "FILE    "
8       3 bytes  extension        "TXT"
11      1 byte   attributes       0x20 (arquivo)
12      10 bytes reserved         (não usado)
22      2 bytes  time             0x7123 (14:09:06)
24      2 bytes  date             0x5335 (05/03/1989)
26      2 bytes  firstCluster     0x0005 (cluster 5)
28      4 bytes  fileSize         0x00000400 (1024 bytes)
────────────────────────────────────────────
        TOTAL: 32 bytes
```

### Exemplo real em hex:
```
46 49 4C 45 20 20 20 20 20 54 58 54 20 00 00 00
FILE____|TXT |reserved...
00 00 23 71 35 53 05 00 00 04 00 00 00 00 00 00
reserved|time|date |1st | fileSize...
         clus.
```

---

## Cálculo de Offset - Passo a Passo

### Exemplo: Ler cluster 5 em um disco FAT16 padrão

```
Inputs:
├─ bytesPerSector = 512
├─ sectorsPerCluster = 1 (1 cluster = 1 setor)
├─ reservedSectors = 1 (só boot sector)
├─ numFATs = 2
├─ fatSize16 = 9
├─ rootEntryCount = 224
└─ cluster = 5 (queremos ler este)

Passo 1: Calcular setores do diretório raiz
────────────────────────────────────
rootDirBytes = 224 × 32 = 7168 bytes
rootDirSectors = ceil(7168 / 512) = 14 setores

Passo 2: Calcular primeiro setor de dados
──────────────────────────────────────────
firstDataSector = 1 + (2 × 9) + 14
                = 1 + 18 + 14
                = 33

Passo 3: Calcular setor do cluster
──────────────────────────────────
cluster_sector = 33 + (5 - 2) × 1
               = 33 + 3
               = 36

Passo 4: Converter em offset de byte
─────────────────────────────────────
offset = 36 × 512 = 18432 bytes

Resultado:
image.seekg(18432)  // Posiciona no cluster 5
```

---

## FAT Values - Tabela de Referência Rápida

```
┌──────────────┬──────────────┬─────────────────────────┐
│ Valor (Hex)  │ Valor (Dec)  │ Significado              │
├──────────────┼──────────────┼─────────────────────────┤
│ 0x0000       │ 0            │ Cluster LIVRE            │
│ 0x0001       │ 1            │ Reservado                │
│ 0x0002-0xFF │ 2-255        │ Próximo cluster          │
│ 0xFFF6       │ 65526        │ Cluster RUIM (bad)       │
│ 0xFFF7       │ 65527        │ RESERVADO                │
│ 0xFFF8-0xFFF │ 65528-65535  │ FIM de arquivo (EOF)     │
└──────────────┴──────────────┴─────────────────────────┘

Uso:
    while (cluster < 0xFFF8) {
        // Cluster válido, continuar
        cluster = FAT[cluster];
    }
    // Encontrou EOF
```

---

## Formato 8.3 - Exemplos

```
Nome Original → Armazenamento → Exibição
─────────────────────────────────────────
README         "README  " + "   "  → README
readme.txt     "README  " + "TXT"  → README.TXT
myfile.c       "MYFILE  " + "C  "  → MYFILE.C
test           "TEST    " + "   "  → TEST
a.b            "A       " + "B  "  → A.B
verylongname   "VERYLONG" + "NAM"  → VERYLONG.NAM (truncado!)

Regras:
├─ Máximo 8 caracteres para nome
├─ Máximo 3 para extensão
├─ SEMPRE maiúsculas
└─ Preenche com espaços (0x20)
```

---

## Decodificação de Data/Hora FAT

### Data (16 bits)
```
Formato:  [YYYYYYY MMMM DDDDD]
          [  7   ] [4 ][ 5 ]
Exemplo: 0x5335 em binário = 0101 0011 0011 0101

Dia:    00101 = 5
Mês:    0011 = 3
Ano:    01001 = 9 → 1980 + 9 = 1989

Resultado: 05/03/1989

Código:
int day = date & 0x1F;
int month = (date >> 5) & 0x0F;
int year = ((date >> 9) & 0x7F) + 1980;
```

### Hora (16 bits)
```
Formato:  [HHHHH MMMMMM SSSSS]
          [ 5  ][ 6    ][ 5 ]
Exemplo: 0x7123 em binário = 0111 0001 0010 0011

Hora:    01110 = 14
Minuto:  001001 = 9
Segundo: 00011 = 3 × 2 = 6

Resultado: 14:09:06

Código:
int hour = (time >> 11) & 0x1F;
int minute = (time >> 5) & 0x3F;
int second = (time & 0x1F) * 2;
```

---

## Atributos de Arquivo (1 byte)

```
Bit  Hex    Significado
───  ────   ──────────────────────
0    0x01   READ_ONLY (somente leitura)
1    0x02   HIDDEN (oculto)
2    0x04   SYSTEM (sistema)
3    0x08   VOLUME (rótulo de volume)
4    0x10   DIRECTORY (diretório)
5    0x20   ARCHIVE (arquivo - padrão)
6    0x40   DEVICE (dispositivo)
7    0x80   RESERVED

Exemplo: 0x27
├─ Binário: 0010 0111
├─ Bit 0: 1 = READ_ONLY
├─ Bit 1: 1 = HIDDEN
├─ Bit 2: 1 = SYSTEM
├─ Bit 5: 1 = ARCHIVE
└─ Resultado: READ_ONLY, HIDDEN, SYSTEM, ARCHIVE

Verificação:
if (attributes & 0x01) cout << "READ_ONLY\n";
if (attributes & 0x02) cout << "HIDDEN\n";
if (attributes & 0x20) cout << "ARCHIVE\n";
```

---

## Operações - Diagrama de Fluxo

### LISTAR ARQUIVOS
```
┌─────────────┐
│   START     │
└──────┬──────┘
       │
       ▼
┌──────────────────────┐
│ Calcular rootDirOff │
│ = (reserved +        │
│   numFATs * fatSize) │
│   * bytesPerSector   │
└──────┬───────────────┘
       │
       ▼
┌──────────────────────┐
│ seekg(rootDirOff)   │
└──────┬───────────────┘
       │
       ▼
    ┌─────────────────────┐
    │  Para cada entry    │
    │  (32 bytes)         │
    └─────┬───────────────┘
          │
          ▼
   ┌─────────────┐
   │ filename[0] │
   │ == 0x00?    │◄────┐
   └─┬───────┬───┘     │
     │ SIM   │ NÃO     │
     │       │         │
     ▼       ▼         │
   BREAK  == 0xE5?─────┤
           │           │
        SIM│ NÃO       │
           │           │
           ▼           │
        SKIP  == 0x0F? │
              │        │
           SIM│ NÃO    │
              │        │
              ▼        │
           SKIP  EXIBIR
                  │
                  └─────┘
       │
       ▼
    ┌──────────┐
    │   END    │
    └──────────┘
```

### CRIAR ARQUIVO
```
┌──────────────────┐
│  Arquivo: x      │
│  Conteúdo: "abc" │
└────────┬─────────┘
         │
         ▼
┌────────────────────────────┐
│ 1. Encontrar cluster livre │
│    (FAT[i] == 0x0000)      │
└────────┬───────────────────┘
         │
         ▼
┌────────────────────────────┐
│ 2. Marcar EOF              │
│    FAT[cluster] = 0xFFFF   │
└────────┬───────────────────┘
         │
         ▼
┌────────────────────────────┐
│ 3. Encontrar slot dir vazio│
│    (filename[0]==0x00|0xE5)│
└────────┬───────────────────┘
         │
         ▼
┌────────────────────────────┐
│ 4. Criar DirectoryEntry    │
│    firstCluster = cluster  │
│    fileSize = 3            │
└────────┬───────────────────┘
         │
         ▼
┌────────────────────────────┐
│ 5. Gravar entry no dir     │
│    seekp(entryPos)         │
│    write(&entry, 32)       │
└────────┬───────────────────┘
         │
         ▼
┌────────────────────────────┐
│ 6. Gravar dados            │
│    seekp(clusterOffset)    │
│    write(conteudo)         │
│    flush()                 │
└────────┬───────────────────┘
         │
         ▼
    ┌────────┐
    │ SUCESSO│
    └────────┘
```

---

## Checklist de Implementação

```
Operação: LISTAR
├─ Calcular rootDirOffset .................... ✅
├─ Seekg para diretório ...................... ✅
├─ Loop por rootEntryCount ................... ✅
├─ Validar entry (0x00, 0xE5, 0x0F) ........ ✅
├─ Remover espaços do nome ................... ✅
├─ Exibir nome e tamanho ..................... ✅
└─ Tratamento de erros ....................... ✅

Operação: LER
├─ Encontrar arquivo ......................... ✅
├─ Ler firstCluster .......................... ✅
├─ Loop de clusters .......................... ✅
│  ├─ Converter cluster em offset .......... ✅
│  ├─ Seekg no offset ....................... ✅
│  ├─ Read dados ............................ ✅
│  ├─ Exibir dados .......................... ✅
│  └─ Buscar nextCluster .................... ✅
├─ Parar quando cluster >= 0xFFF8 .......... ✅
└─ Tratamento de arquivo não encontrado ... ✅

Operação: ATRIBUTOS
├─ Encontrar arquivo ......................... ✅
├─ Exibir nome .............................. ✅
├─ Exibir tamanho ........................... ✅
├─ Exibir firstCluster ....................... ✅
├─ Decodificar atributos .................... ✅
├─ Decodificar data ......................... ✅
├─ Decodificar hora ......................... ✅
└─ Formato de exibição ...................... ✅

Operação: RENOMEAR
├─ Encontrar arquivo ......................... ✅
├─ Validar novo nome ........................ ✅
├─ Converter para 8.3 ....................... ✅
├─ Manter streampos ......................... ✅
├─ Seekp para entry ......................... ✅
├─ Write entry atualizado ................... ✅
├─ Flush .................................... ✅
└─ Mensagem de sucesso ...................... ✅

Operação: CRIAR
├─ Converter nome para 8.3 .................. ✅
├─ Buscar cluster livre na FAT ............. ✅
├─ Marcar como EOF (0xFFFF) ................. ✅
├─ Buscar slot no diretório ................. ✅
├─ Criar entry ............................. ✅
├─ Manter streampos ......................... ✅
├─ Seekp para entry ......................... ✅
├─ Write entry ............................. ✅
├─ Seekp para data ......................... ✅
├─ Write conteudo .......................... ✅
├─ Flush .................................... ✅
└─ Mensagem de sucesso ...................... ✅

Operação: DELETAR
├─ Encontrar arquivo ......................... ✅
├─ Manter streampos ......................... ✅
├─ Ler firstCluster .......................... ✅
├─ Loop por clusters ........................ ✅
│  ├─ Ler nextCluster ....................... ✅
│  └─ Marcar como livre (0x0000) .......... ✅
├─ Marcar entry como apagada (0xE5) ....... ✅
├─ Flush .................................... ✅
└─ Mensagem de sucesso ...................... ✅
```

---

## Resolução Rápida de Bugs

### "Erro ao ler Boot Sector"
```
Possíveis causas:
├─ Arquivo não encontrado
├─ Arquivo não é FAT16 válido
├─ Permissão de leitura negada
└─ Caminho incorreto

Solução:
• Verificar se arquivo existe
• Verificar se é arquivo FAT16 válido
• Verificar permissões
• Usar caminho absoluto
```

### "Leitura de arquivo mostra lixo"
```
Possíveis causas:
├─ firstCluster inválido
├─ Offset calculado incorretamente
├─ FAT corrompida
└─ Cadeia de clusters quebrada

Solução:
• Verificar se firstCluster está entre 2 e 0xFFEF
• Validar cálculo de offset com print debug
• Usar FAT2 como backup
• Verificar se próximo cluster é válido
```

### "Arquivo criado mas não aparece"
```
Possíveis causas:
├─ Entry não gravada corretamente
├─ seekp em lugar errado
├─ flush() não chamado
└─ Dados não gravados

Solução:
• Verificar streampos antes de write()
• Chamar flush() após write()
• Verificar se dados foram realmente gravados
• Listar arquivo novamente para confirmar
```

---

## Resumo de 1 Página

### FAT16 em 5 pontos:

1. **Layout:** Boot (512B) → FAT1 (9 setores) → FAT2 → Root Dir → Data
2. **Navegação:** DirectoryEntry tem firstCluster → FAT[cluster] = próximo
3. **Offset:** `(firstDataSector + (cluster-2) * sectorsPerCluster) * 512`
4. **Valores especiais:** `0x0000`=livre, `0xFFFF`=EOF, `0xE5`=apagado
5. **Operações:** Listar/Ler (busca) → Atributos/Rename (update) → Criar (aloca) → Deletar (libera)

---

**Use este arquivo para revisar visualmente antes da apresentação!** 🎯
