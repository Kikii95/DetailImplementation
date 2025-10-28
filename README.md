# Architecture v3 - Système Mutable avec ID Unique

**Date** : 28 octobre 2025
**Objectif** : Archives mutables (AddFile/RemoveFile) + ID unique + Utilisation File.h

---

## 🎯 Problèmes actuels (v2)

### ❌ 1. Utilisation de FILE* au lieu de File
```cpp
// Actuellement dans Archive.cpp
FILE* archive = nullptr;
fopen_s(&archive, archivePath.c_str(), "rb");
```

**Problème** : On n'utilise PAS `File.h/File.cpp` qui existent dans le projet !

### ❌ 2. ID basé sur le nom de fichier
```cpp
struct FileEntry {
    char filename[256];  // ← Identifiant = nom (problème si renommage)
    // ...
};
```

**Problèmes** :
- Si 2 fichiers ont le même nom → conflit
- Impossible de renommer sans perdre la référence
- Pas d'identifiant stable

### ❌ 3. Archive immuable
```cpp
// Impossible actuellement
Archive::AddFile("game.asset", "newfile.txt");     // ❌ Pas implémenté
Archive::RemoveFile("game.asset", "oldfile.txt");  // ❌ Pas implémenté
```

**Problème** : Il faut recréer toute l'archive pour ajouter/retirer un fichier !

---

## ✅ Solutions : Architecture v3

### 1. Utiliser File au lieu de FILE*

```cpp
// Avant (v2)
FILE* archive = nullptr;
fopen_s(&archive, archivePath.c_str(), "rb");
fread(buffer, 1, size, archive);
fclose(archive);

// Après (v3)
File archive;
archive.OpenRead(archivePath);
archive.Read(buffer, size, 1);
archive.Close();
```

**Avantages** :
- ✅ Cohérent avec le reste du projet (Stream → File)
- ✅ Gestion automatique (destructeur appelle Close())
- ✅ Interface propre et orientée objet

---

### 2. ID unique par fichier

```cpp
struct FileEntry {
    UINT64 id;              // ID unique (timestamp + hash ou UUID)
    char   filename[256];   // Nom humainement lisible (peut changer)
    UINT64 offset;
    UINT64 size;
    UINT8  flags;           // ACTIVE | DELETED | COMPRESSED | ENCRYPTED
};
```

**Génération ID** :
```cpp
// Option 1 : Timestamp + Hash
UINT64 GenerateFileID(const std::string& filename) {
    UINT64 timestamp = GetCurrentTimestamp();  // 64 bits Unix timestamp
    UINT32 hash = CalculateCRC32((UINT8*)filename.c_str(), filename.size());
    return (timestamp << 32) | hash;
}

// Option 2 : UUID (plus robuste)
UINT64 GenerateUUID() {
    // Utiliser std::random_device + distribution
    // Ou UUID v4 (128 bits tronqué à 64)
}
```

**Avantages** :
- ✅ ID stable même si on renomme le fichier
- ✅ Pas de collision (timestamp + hash quasi unique)
- ✅ Permet référencement externe (ex: save game pointe vers ID, pas nom)

---

### 3. Index mutable avec flags

```cpp
enum FileFlags : UINT8 {
    FILE_ACTIVE     = 0x01,  // Fichier actif
    FILE_DELETED    = 0x02,  // Fichier marqué deleted (espace réutilisable)
    FILE_COMPRESSED = 0x04,  // Stream compressé (zlib)
    FILE_ENCRYPTED  = 0x08   // Stream chiffré (AES)
};
```

**Structure FileEntry étendue** :
```cpp
struct FileEntry {
    UINT64 id;              // ID unique
    char   filename[256];   // Nom (peut être renommé)
    UINT64 offset;          // Position dans l'archive
    UINT64 size;            // Taille (header + data)
    UINT8  flags;           // Flags (active, deleted, etc.)
    UINT8  padding[7];      // Alignement à 280 bytes
};
// Total: 280 bytes (même taille qu'avant pour compatibilité)
```

---

## 🔧 Nouvelles fonctions API

### Archive::AddFile()

```cpp
bool Archive::AddFile(const std::string& archivePath,
                      const std::string& filePath,
                      UINT64& outFileID)
{
    // 1. Ouvrir archive en mode WRITE (r+b)
    File archive;
    archive.Open(archivePath, WRITE);

    // 2. Lire header + index
    ArchiveHeader header;
    std::vector<FileEntry> entries = ReadIndex(archive, header);

    // 3. Chercher espace libre (entry avec flag DELETED)
    FileEntry* freeEntry = FindDeletedEntry(entries);

    if (freeEntry != nullptr && freeEntry->size >= neededSize) {
        // Réutiliser l'espace
        WriteFileToOffset(archive, freeEntry->offset, filePath);
        freeEntry->flags = FILE_ACTIVE;
        freeEntry->id = GenerateFileID(filePath);
        // ...
    } else {
        // Ajouter à la fin (avant l'index)
        UINT64 insertOffset = header.indexOffset;
        archive.Seek(insertOffset, SEEK_SET);
        WriteFileWithHeader(archive, filePath);

        // Créer nouvelle entry
        FileEntry newEntry;
        newEntry.id = GenerateFileID(filePath);
        newEntry.flags = FILE_ACTIVE;
        // ...
        entries.push_back(newEntry);
    }

    // 4. Réécrire index
    header.fileCount = CountActiveFiles(entries);
    header.indexOffset = /* nouvelle position */;
    WriteIndex(archive, entries, header);

    // 5. Mettre à jour header
    archive.Seek(0, SEEK_SET);
    archive.Write((UINT8*)&header, sizeof(ArchiveHeader), 1);

    archive.Close();
    return true;
}
```

---

### Archive::RemoveFile()

```cpp
bool Archive::RemoveFile(const std::string& archivePath, UINT64 fileID)
{
    // 1. Ouvrir archive en mode WRITE
    File archive;
    archive.Open(archivePath, WRITE);

    // 2. Lire index
    ArchiveHeader header;
    std::vector<FileEntry> entries = ReadIndex(archive, header);

    // 3. Trouver entry par ID
    auto it = std::find_if(entries.begin(), entries.end(),
        [fileID](const FileEntry& e) { return e.id == fileID; });

    if (it == entries.end())
        return false;  // Fichier non trouvé

    // 4. Marquer comme deleted (soft delete)
    it->flags = FILE_DELETED;

    // 5. Réécrire index
    WriteIndex(archive, entries, header);

    // 6. Mettre à jour header (fileCount actifs)
    header.fileCount = CountActiveFiles(entries);
    archive.Seek(0, SEEK_SET);
    archive.Write((UINT8*)&header, sizeof(ArchiveHeader), 1);

    archive.Close();
    return true;
}
```

**Note** : On ne supprime PAS physiquement les données, juste le flag. Les données restent jusqu'au `Compact()`.

---

### Archive::Compact()

```cpp
bool Archive::Compact(const std::string& archivePath)
{
    // 1. Lire archive actuelle
    File oldArchive;
    oldArchive.OpenRead(archivePath);

    ArchiveHeader header;
    std::vector<FileEntry> entries = ReadIndex(oldArchive, header);

    // 2. Créer archive temporaire
    std::string tempPath = archivePath + ".tmp";
    File newArchive;
    newArchive.OpenWrite(tempPath);

    // 3. Écrire header temporaire
    ArchiveHeader newHeader = header;
    newHeader.fileCount = CountActiveFiles(entries);
    newArchive.Write((UINT8*)&newHeader, sizeof(ArchiveHeader), 1);

    // 4. Copier UNIQUEMENT les fichiers actifs
    UINT64 currentOffset = sizeof(ArchiveHeader);
    std::vector<FileEntry> newEntries;

    for (const auto& entry : entries) {
        if (!(entry.flags & FILE_ACTIVE))
            continue;  // Skip deleted

        // Lire blob depuis ancienne archive
        oldArchive.Seek(entry.offset, SEEK_SET);
        Blob blob;
        blob.Reserve(entry.size);
        oldArchive.Read(blob.GetData(), entry.size, 1);

        // Écrire dans nouvelle archive
        newArchive.Write(blob.GetData(), entry.size, 1);

        // Créer nouvelle entry avec nouvel offset
        FileEntry newEntry = entry;
        newEntry.offset = currentOffset;
        newEntries.push_back(newEntry);

        currentOffset += entry.size;
    }

    // 5. Écrire nouvel index
    newHeader.indexOffset = currentOffset;
    WriteIndex(newArchive, newEntries, newHeader);

    // 6. Mettre à jour header
    newArchive.Seek(0, SEEK_SET);
    newArchive.Write((UINT8*)&newHeader, sizeof(ArchiveHeader), 1);

    oldArchive.Close();
    newArchive.Close();

    // 7. Remplacer ancienne archive
    remove(archivePath.c_str());
    rename(tempPath.c_str(), archivePath.c_str());

    return true;
}
```

---

### Archive::RenameFile()

```cpp
bool Archive::RenameFile(const std::string& archivePath,
                         UINT64 fileID,
                         const std::string& newName)
{
    File archive;
    archive.Open(archivePath, WRITE);

    ArchiveHeader header;
    std::vector<FileEntry> entries = ReadIndex(archive, header);

    // Trouver par ID (pas par nom !)
    auto it = std::find_if(entries.begin(), entries.end(),
        [fileID](const FileEntry& e) { return e.id == fileID; });

    if (it == entries.end())
        return false;

    // Changer le nom (l'ID reste le même)
    strncpy_s(it->filename, 256, newName.c_str(), _TRUNCATE);

    // Réécrire index
    WriteIndex(archive, entries, header);

    archive.Close();
    return true;
}
```

---

### Archive::FindByID()

```cpp
bool Archive::FindByID(const std::string& archivePath,
                       UINT64 fileID,
                       FileEntry& outEntry)
{
    File archive;
    archive.OpenRead(archivePath);

    ArchiveHeader header;
    std::vector<FileEntry> entries = ReadIndex(archive, header);

    auto it = std::find_if(entries.begin(), entries.end(),
        [fileID](const FileEntry& e) {
            return e.id == fileID && (e.flags & FILE_ACTIVE);
        });

    if (it == entries.end())
        return false;

    outEntry = *it;
    archive.Close();
    return true;
}
```

---

## 📊 Comparaison architectures

| Aspect | v2 (actuel) | v3 (proposé) | Gain |
|--------|-------------|--------------|------|
| **Classe utilisée** | FILE* (C) | File (C++ OOP) | ✅ Cohérence |
| **Identifiant** | Nom fichier | ID unique 64-bit | ✅ Stabilité |
| **Mutabilité** | Immuable (recréer tout) | Mutable (AddFile/RemoveFile) | ✅ Performance |
| **Renommage** | ❌ Perte référence | ✅ ID stable | ✅ Flexibilité |
| **Défragmentation** | N/A | Compact() | ✅ Optimisation |
| **Gestion espace** | Linéaire (append) | Réutilisation espaces libres | ✅ Efficacité |
| **API** | 5 fonctions | 9 fonctions | ✅ Complet |

---

## 🔧 Nouvelles structures

### ArchiveHeader (inchangé)
```cpp
struct ArchiveHeader {
    char   magic[4];      // "ASET"
    UINT32 version;       // 2 (nouvelle version)
    UINT32 fileCount;     // Nombre de fichiers ACTIFS
    UINT64 indexOffset;
    UINT64 dataOffset;
    UINT32 archiveCRC;
};
```

### FileEntry (refactorisé)
```cpp
struct FileEntry {
    UINT64 id;              // ID unique (nouveau)
    char   filename[256];   // Nom (peut changer)
    UINT64 offset;
    UINT64 size;
    UINT8  flags;           // Flags (nouveau)
    UINT8  padding[7];      // Alignement
};
// Total: 280 bytes
```

### Stream::Header (inchangé)
```cpp
struct Header {
    char   magic[4];        // "SAFE"
    UINT16 version;
    UINT16 flags;
    UINT64 dataSize;
    UINT64 originalSize;
    UINT32 checksum;
};
```

---

## 🎯 API complète v3

```cpp
class Archive {
public:
    // Existantes (refactorisées)
    static bool Create(const std::string& archivePath, const std::vector<std::string>& filePaths);
    static bool List(const std::string& archivePath);
    static bool Extract(const std::string& archivePath, UINT64 fileID, const std::string& outputPath);
    static bool ExtractAll(const std::string& archivePath, const std::string& outputDir);
    static bool Validate(const std::string& archivePath);

    // NOUVELLES fonctions mutabilité
    static bool AddFile(const std::string& archivePath, const std::string& filePath, UINT64& outFileID);
    static bool RemoveFile(const std::string& archivePath, UINT64 fileID);
    static bool RenameFile(const std::string& archivePath, UINT64 fileID, const std::string& newName);
    static bool Compact(const std::string& archivePath);

    // NOUVELLES fonctions recherche par ID
    static bool FindByID(const std::string& archivePath, UINT64 fileID, FileEntry& outEntry);
    static bool FindByName(const std::string& archivePath, const std::string& filename, FileEntry& outEntry);

private:
    // Helpers
    static UINT64 GenerateFileID(const std::string& filename);
    static UINT32 CountActiveFiles(const std::vector<FileEntry>& entries);
    static FileEntry* FindDeletedEntry(std::vector<FileEntry>& entries, UINT64 minSize);
    static std::vector<FileEntry> ReadIndex(File& archive, const ArchiveHeader& header);
    static bool WriteIndex(File& archive, const std::vector<FileEntry>& entries, ArchiveHeader& header);
    static bool WriteFileWithHeader(File& archive, const std::string& filePath, UINT64& outID);
};
```

---

## 🧪 Exemples d'utilisation

### Créer archive
```cpp
std::vector<std::string> files = {"file1.txt", "file2.txt"};
Archive::Create("game.asset", files);
```

### Ajouter fichier
```cpp
UINT64 fileID;
Archive::AddFile("game.asset", "newfile.txt", fileID);
std::cout << "Ajouté avec ID: " << fileID << "\n";
```

### Extraire par ID
```cpp
UINT64 fileID = 12345678;
Archive::Extract("game.asset", fileID, "extracted.txt");
```

### Renommer
```cpp
Archive::RenameFile("game.asset", fileID, "renamed.txt");
```

### Supprimer (soft delete)
```cpp
Archive::RemoveFile("game.asset", fileID);
```

### Défragmenter
```cpp
Archive::Compact("game.asset");  // Reconstruit sans les fichiers deleted
```

---

## 📐 Structure archive v3

```
game.asset (après Add/Remove)
┌─────────────────────────────────────────┐
│ ArchiveHeader                           │
│ version = 2, fileCount = 2 (actifs)    │
├─────────────────────────────────────────┤
│ Stream::Header + file1.txt data         │
│ (flags = ACTIVE)                        │
├─────────────────────────────────────────┤
│ Stream::Header + file2.txt data         │
│ (flags = DELETED) ← Espace réutilisable│
├─────────────────────────────────────────┤
│ Stream::Header + file3.txt data         │
│ (flags = ACTIVE)                        │
├─────────────────────────────────────────┤
│ FileEntry table                         │
│ [1] id=123, filename="file1.txt", flags=ACTIVE
│ [2] id=456, filename="file2.txt", flags=DELETED
│ [3] id=789, filename="file3.txt", flags=ACTIVE
└─────────────────────────────────────────┘
```

---

## ✅ Checklist implémentation

- [ ] Refactoriser Archive.h avec nouvelles structures
- [ ] Remplacer FILE* par File dans Archive.cpp
- [ ] Implémenter GenerateFileID()
- [ ] Refactoriser Create/List/Extract/Validate pour utiliser File
- [ ] Implémenter AddFile()
- [ ] Implémenter RemoveFile()
- [ ] Implémenter RenameFile()
- [ ] Implémenter Compact()
- [ ] Implémenter FindByID() / FindByName()
- [ ] Mettre à jour main.cpp avec nouvelles commandes CLI
- [ ] Tester workflow complet
- [ ] Mettre à jour documentation

---

## 🎁 Bonus : CLI étendu

```bash
# Créer archive
AssetEngine.exe create game.asset file1.txt file2.txt

# Lister avec IDs
AssetEngine.exe list game.asset
# Output:
# [ID: 123456] file1.txt (100 bytes, ACTIVE)
# [ID: 789012] file2.txt (200 bytes, ACTIVE)

# Ajouter fichier
AssetEngine.exe add game.asset newfile.txt
# Output: Added with ID: 345678

# Extraire par ID
AssetEngine.exe extract-id game.asset 123456 output.txt

# Renommer
AssetEngine.exe rename game.asset 123456 renamed.txt

# Supprimer
AssetEngine.exe remove game.asset 123456

# Défragmenter
AssetEngine.exe compact game.asset
```

---

**Conclusion** : Architecture v3 = Archives mutables + ID unique + Cohérence File.h

**Prêt à implémenter ?** Cette architecture est extensible et optimale.
