#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include <cstring>

using namespace std;

#pragma pack(push, 1)

// Estrutura do Boot Sector FAT16
struct BootSector {
    uint8_t jump[3];
    char oem[8];

    uint16_t bytesPerSector;
    uint8_t sectorsPerCluster;
    uint16_t reservedSectors;
    uint8_t numFATs;
    uint16_t rootEntryCount;
    uint16_t totalSectors16;
    uint8_t media;
    uint16_t fatSize16;
    uint16_t sectorsPerTrack;
    uint16_t numHeads;
    uint32_t hiddenSectors;
    uint32_t totalSectors32;

    uint8_t driveNumber;
    uint8_t reserved1;
    uint8_t bootSignature;
    uint32_t volumeID;
    char volumeLabel[11];
    char fileSystemType[8];
};

// Entrada do diretório raiz
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

#pragma pack(pop)

class FAT16 {
private:
    fstream image;
    BootSector bs;

public:

    bool openImage(const string& filename) {
        image.open(filename, ios::in | ios::out | ios::binary);

        if (!image.is_open()) {
            cout << "Erro ao abrir imagem.\n";
            return false;
        }

        image.read(reinterpret_cast<char*>(&bs), sizeof(bs));

        if (!image) {
            cout << "Erro ao ler Boot Sector.\n";
            return false;
        }

        return true;
    }

    void printInfo() {
        cout << "\n=== Informacoes FAT16 ===\n";
        cout << "Bytes por setor: "
             << bs.bytesPerSector << endl;

        cout << "Setores por cluster: "
             << (int)bs.sectorsPerCluster << endl;

        cout << "Numero de FATs: "
             << (int)bs.numFATs << endl;

        cout << "Entradas no diretorio raiz: "
             << bs.rootEntryCount << endl;

        cout << "Setores por FAT: "
             << bs.fatSize16 << endl;
    }

    void listRootDirectory() {

        uint32_t rootDirSector =
            bs.reservedSectors +
            (bs.numFATs * bs.fatSize16);

        uint32_t rootDirOffset =
            rootDirSector * bs.bytesPerSector;

        image.seekg(rootDirOffset);

        cout << "\n=== Diretorio Raiz ===\n\n";

        for (int i = 0; i < bs.rootEntryCount; i++) {

            DirectoryEntry entry;

            image.read(
                reinterpret_cast<char*>(&entry),
                sizeof(entry)
            );

            if (entry.filename[0] == 0x00)
                break;

            if ((unsigned char)entry.filename[0] == 0xE5)
                continue;

            if (entry.attributes == 0x0F)
                continue;

            string name(entry.filename, 8);
            string ext(entry.extension, 3);

            while (!name.empty() && name.back() == ' ')
                name.pop_back();

            while (!ext.empty() && ext.back() == ' ')
                ext.pop_back();

            cout << left
                 << setw(20)
                 << (ext.empty()
                     ? name
                     : name + "." + ext);

            cout << "Tamanho: "
                 << setw(10)
                 << entry.fileSize;

            if (entry.attributes & 0x10)
                cout << "[DIR]";

            cout << endl;
        }
    }

    bool isOpen() {
        return image.is_open();
    }


bool findFile(const string& targetName, DirectoryEntry& foundEntry) {

    uint32_t rootDirSector =
        bs.reservedSectors +
        (bs.numFATs * bs.fatSize16);

    uint32_t rootDirOffset =
        rootDirSector * bs.bytesPerSector;

    image.seekg(rootDirOffset);

    for (int i = 0; i < bs.rootEntryCount; i++) {

        DirectoryEntry entry;

        image.read(
            reinterpret_cast<char*>(&entry),
            sizeof(entry)
        );

        if (entry.filename[0] == 0x00)
            break;

        if ((unsigned char)entry.filename[0] == 0xE5)
            continue;

        if (entry.attributes == 0x0F)
            continue;

        string name(entry.filename, 8);
        string ext(entry.extension, 3);

        while (!name.empty() && name.back() == ' ')
            name.pop_back();

        while (!ext.empty() && ext.back() == ' ')
            ext.pop_back();

        string fullName =
            ext.empty()
                ? name
                : name + "." + ext;

        if (fullName == targetName) {
            foundEntry = entry;
            return true;
        }
    }

    return false;
}

uint16_t getNextCluster(uint16_t cluster) {

    uint32_t fatOffset =
        bs.reservedSectors *
        bs.bytesPerSector;

    uint32_t entryOffset =
        fatOffset +
        cluster * 2;

    uint16_t nextCluster;

    image.seekg(entryOffset);

    image.read(
        reinterpret_cast<char*>(&nextCluster),
        sizeof(nextCluster)
    );

    return nextCluster;
}

uint32_t clusterToOffset(uint16_t cluster) {

    uint32_t rootDirSectors =
        ((bs.rootEntryCount * 32) +
        (bs.bytesPerSector - 1))
        / bs.bytesPerSector;

    uint32_t firstDataSector =
        bs.reservedSectors +
        (bs.numFATs * bs.fatSize16) +
        rootDirSectors;

    uint32_t sector =
        firstDataSector +
        (cluster - 2) *
        bs.sectorsPerCluster;

    return sector *
           bs.bytesPerSector;
}

void readFile(const string& filename) {

    DirectoryEntry fileEntry;

    if (!findFile(filename, fileEntry)) {

        cout << "Arquivo nao encontrado.\n";
        return;
    }

    uint32_t remainingBytes =
        fileEntry.fileSize;

    uint16_t cluster =
        fileEntry.firstCluster;

    cout << "\n===== CONTEUDO =====\n\n";

    while (
        cluster >= 2 &&
        cluster < 0xFFF8 &&
        remainingBytes > 0
    ) {

        uint32_t offset =
            clusterToOffset(cluster);

        image.seekg(offset);

        uint32_t clusterSize =
            bs.bytesPerSector *
            bs.sectorsPerCluster;

        vector<char> buffer(clusterSize);

        image.read(
            buffer.data(),
            clusterSize
        );

        uint32_t bytesToPrint =
            min(clusterSize,
                remainingBytes);

        for (uint32_t i = 0;
             i < bytesToPrint;
             i++) {

            cout << buffer[i];
        }

        remainingBytes -= bytesToPrint;

        cluster =
            getNextCluster(cluster);
    }

    cout << "\n\n====================\n";
}

bool renameFile(const string& oldName, const string& newName) {

    uint32_t rootDirSector =
        bs.reservedSectors +
        (bs.numFATs * bs.fatSize16);

    uint32_t rootDirOffset =
        rootDirSector * bs.bytesPerSector;

    image.seekg(rootDirOffset);

    for (int i = 0; i < bs.rootEntryCount; i++) {

        streampos entryPos = image.tellg();

        DirectoryEntry entry;
        image.read(reinterpret_cast<char*>(&entry), sizeof(entry));

        if (entry.filename[0] == 0x00)
            break;

        if ((unsigned char)entry.filename[0] == 0xE5)
            continue;

        if (entry.attributes == 0x0F)
            continue;

        // Monta nome atual
        string name(entry.filename, 8);
        string ext(entry.extension, 3);

        while (!name.empty() && name.back() == ' ')
            name.pop_back();

        while (!ext.empty() && ext.back() == ' ')
            ext.pop_back();

        string fullName =
            ext.empty() ? name : name + "." + ext;

        // Se encontrou o arquivo
        if (fullName == oldName) {

            // Separar novo nome
            string newBase = newName;
            string newExt = "";

            size_t dot = newName.find('.');

            if (dot != string::npos) {
                newBase = newName.substr(0, dot);
                newExt = newName.substr(dot + 1);
            }

            // Ajustar FAT 8.3
            memset(entry.filename, ' ', 8);
            memset(entry.extension, ' ', 3);

            for (size_t j = 0; j < min((size_t)8, newBase.size()); j++)
                entry.filename[j] = toupper(newBase[j]);

            for (size_t j = 0; j < min((size_t)3, newExt.size()); j++)
                entry.extension[j] = toupper(newExt[j]);

            // Volta para posição exata do entry
            image.seekp(entryPos);

            // Reescreve entrada
            image.write(reinterpret_cast<char*>(&entry), sizeof(entry));

            image.flush();

            cout << "Arquivo renomeado com sucesso!\n";
            return true;
        }
    }

    cout << "Arquivo nao encontrado.\n";
    return false;
}

bool createFile(const string& filename, const string& content) {

    // =========================
    // 1. Converter nome FAT16
    // =========================
    string base = filename;
    string ext = "";

    size_t dot = filename.find('.');

    if (dot != string::npos) {
        base = filename.substr(0, dot);
        ext = filename.substr(dot + 1);
    }

    char fatName[8];
    char fatExt[3];

    memset(fatName, ' ', 8);
    memset(fatExt, ' ', 3);

    for (size_t i = 0; i < min((size_t)8, base.size()); i++)
        fatName[i] = toupper(base[i]);

    for (size_t i = 0; i < min((size_t)3, ext.size()); i++)
        fatExt[i] = toupper(ext[i]);

    // =========================
    // 2. Encontrar cluster livre
    // =========================
    uint16_t freeCluster = 0;

    uint32_t fatOffset = bs.reservedSectors * bs.bytesPerSector;

    for (int i = 2; i < 0xFFF0; i++) {

        uint16_t value;

        image.seekg(fatOffset + i * 2);
        image.read(reinterpret_cast<char*>(&value), 2);

        if (value == 0x0000) {
            freeCluster = i;
            break;
        }
    }

    if (freeCluster == 0) {
        cout << "Sem espaco na FAT.\n";
        return false;
    }

    // =========================
    // 3. Marcar cluster como EOF
    // =========================
    uint16_t eof = 0xFFFF;

    image.seekp(fatOffset + freeCluster * 2);
    image.write(reinterpret_cast<char*>(&eof), 2);

    // =========================
    // 4. Encontrar entrada livre no diretório raiz
    // =========================
    uint32_t rootDirSector =
        bs.reservedSectors +
        (bs.numFATs * bs.fatSize16);

    uint32_t rootDirOffset =
        rootDirSector * bs.bytesPerSector;

    image.seekg(rootDirOffset);

    DirectoryEntry entry;
    streampos entryPos;

    bool foundSlot = false;

    for (int i = 0; i < bs.rootEntryCount; i++) {

        entryPos = image.tellg();

        image.read(reinterpret_cast<char*>(&entry), sizeof(entry));

        if (entry.filename[0] == 0x00 ||
            (unsigned char)entry.filename[0] == 0xE5) {

            foundSlot = true;
            break;
        }
    }

    if (!foundSlot) {
        cout << "Diretorio raiz cheio.\n";
        return false;
    }

    // =========================
    // 5. Criar entrada
    // =========================
    memset(&entry, 0, sizeof(entry));

    memcpy(entry.filename, fatName, 8);
    memcpy(entry.extension, fatExt, 3);

    entry.attributes = 0x20; // arquivo normal
    entry.firstCluster = freeCluster;
    entry.fileSize = content.size();

    // =========================
    // 6. Escrever entrada
    // =========================
    image.seekp(entryPos);
    image.write(reinterpret_cast<char*>(&entry), sizeof(entry));

    // =========================
    // 7. Escrever conteúdo no cluster
    // =========================
    uint32_t dataOffset =
        clusterToOffset(freeCluster);

    image.seekp(dataOffset);
    image.write(content.c_str(), content.size());

    image.flush();

    cout << "Arquivo criado com sucesso!\n";
    return true;
}

bool deleteFile(const string& filename) {

    // =========================
    // 1. Procurar arquivo no diretório raiz
    // =========================
    uint32_t rootDirSector =
        bs.reservedSectors +
        (bs.numFATs * bs.fatSize16);

    uint32_t rootDirOffset =
        rootDirSector * bs.bytesPerSector;

    image.seekg(rootDirOffset);

    DirectoryEntry entry;
    streampos entryPos;

    bool found = false;

    for (int i = 0; i < bs.rootEntryCount; i++) {

        entryPos = image.tellg();

        image.read(reinterpret_cast<char*>(&entry), sizeof(entry));

        if (entry.filename[0] == 0x00)
            break;

        if ((unsigned char)entry.filename[0] == 0xE5)
            continue;

        if (entry.attributes == 0x0F)
            continue;

        // Monta nome
        string name(entry.filename, 8);
        string ext(entry.extension, 3);

        while (!name.empty() && name.back() == ' ')
            name.pop_back();

        while (!ext.empty() && ext.back() == ' ')
            ext.pop_back();

        string fullName =
            ext.empty() ? name : name + "." + ext;

        if (fullName == filename) {
            found = true;
            break;
        }
    }

    if (!found) {
        cout << "Arquivo nao encontrado.\n";
        return false;
    }

    // =========================
    // 2. Liberar clusters na FAT
    // =========================
    uint16_t cluster = entry.firstCluster;

    uint32_t fatOffset =
        bs.reservedSectors * bs.bytesPerSector;

    while (cluster >= 2 && cluster < 0xFFF8) {

        uint16_t next;

        image.seekg(fatOffset + cluster * 2);
        image.read(reinterpret_cast<char*>(&next), 2);

        // Marca cluster como livre
        uint16_t free = 0x0000;

        image.seekp(fatOffset + cluster * 2);
        image.write(reinterpret_cast<char*>(&free), 2);

        cluster = next;
    }

    // =========================
    // 3. Marcar entrada como deletada
    // =========================
    char deleted = (char)0xE5;

    image.seekp(entryPos);
    image.write(&deleted, 1);

    image.flush();

    cout << "Arquivo removido com sucesso!\n";
    return true;
}

};


/// MAIN 
int main() {

    FAT16 fat;

    int opcao;
    string imagem;

    do {

        cout << "\n========== FAT16 ==========\n";
        cout << "1 - Abrir imagem FAT16\n";
        cout << "2 - Mostrar informacoes\n";
        cout << "3 - Listar diretorio raiz\n";
        cout << "4 - Ler arquivo\n";
        cout << "5 - Renomear arquivo\n";
        cout << "6 - Criar arquivo\n";
        cout << "7 - Remover arquivo\n";
        cout << "0 - Sair\n";
        cout << "Opcao: ";

        cin >> opcao;

        switch (opcao) {

        case 1:

            cout << "Nome da imagem: ";
            cin >> imagem;

            if (fat.openImage(imagem))
                cout << "Imagem aberta com sucesso!\n";

            break;

        case 2:

            if (fat.isOpen())
                fat.printInfo();
            else
                cout << "Abra uma imagem primeiro.\n";

            break;

        case 3:

            if (fat.isOpen())
                fat.listRootDirectory();
            else
                cout << "Abra uma imagem primeiro.\n";

            break;

        case 4: {

            if (!fat.isOpen()) {
                cout << "Abra uma imagem primeiro.\n";
                break;
            }
            string nome;

            cout << "Nome do arquivo: ";
            cin >> nome;

            fat.readFile(nome);

            break;
        }
        case 5: {

            if (!fat.isOpen()) {
                cout << "Abra uma imagem primeiro.\n";
                break;
            }

            string oldName, newName;

            cout << "Nome atual: ";
            cin >> oldName;

            cout << "Novo nome: ";
            cin >> newName;

            fat.renameFile(oldName, newName);

            break;
        }
            case 6: {

            if (!fat.isOpen()) {
                cout << "Abra uma imagem primeiro.\n";
                break;
            }

            string nome, conteudo;

            cout << "Nome do arquivo: ";
            cin >> nome;

            cout << "Conteudo: ";
            cin.ignore();
            getline(cin, conteudo);

            fat.createFile(nome, conteudo);

            break;  
        }

            case 7: {

            if (!fat.isOpen()) {
                cout << "Abra uma imagem primeiro.\n";
                break;
            }

            string nome;

            cout << "Nome do arquivo: ";
            cin >> nome;

            fat.deleteFile(nome);

            break;
        }
        case 0:
            cout << "Encerrando...\n";
            break;

        default:
            cout << "Opcao invalida.\n";
        }

    } while (opcao != 0);

    return 0;
}