# Refactorisation Architecture v2 - Asset Engine

**Date** : 28 octobre 2025
**Objectif** : Unifier l'architecture avec Stream::Header partout

---

## 📋 Résumé des modifications

### Avant (v1)
```
game.asset
├── ArchiveHeader (magic "ASET")
├── DATA SECTION (fichiers en RAW, sans header)
└── FileEntry table (avec crc32 redondant)
```

**Problèmes** :
- CRC32 dupliqué (FileEntry.crc32 + calcul manuel)
- Pas de réutilisation de SafeReader
- Impossible d'ajouter compression/chiffrement facilement

### Après (v2)
```
game.asset
├── ArchiveHeader (magic "ASET")
├── DATA SECTION
│   ├── Stream::Header (magic "SAFE") + fichier 1
│   ├── Stream::Header (magic "SAFE") + fichier 2
│   └── ...
└── FileEntry table (SANS crc32, déjà dans Stream::Header)
```

**Avantages** :
- ✅ Hiérarchie cohérente : ArchiveHeader → Stream::Header → Data
- ✅ CRC32 unifié (une seule source de vérité)
- ✅ Prêt pour compression (flags dans Stream::Header)
- ✅ Architecture extensible

---

## 🔧 Fichiers modifiés

### 1. `Stream.h`

**Changements** :
```cpp
// Avant
struct Header {
    char   magic[4];
    UINT32 version;      // 4 bytes
    UINT32 dataSize;     // 4 bytes
    UINT32 checksum;
};
// Total: 16 bytes

// Après
struct Header {
    char   magic[4];
    UINT16 version;       // 2 bytes
    UINT16 flags;         // 2 bytes (NOUVEAU)
    UINT64 dataSize;      // 8 bytes
    UINT64 originalSize;  // 8 bytes (NOUVEAU, pour compression)
    UINT32 checksum;
};
// Total: 28 bytes

enum Flags : UINT16 {
    NONE       = 0x0000,
    COMPRESSED = 0x0001,  // Pour bonus compression
    ENCRYPTED  = 0x0002   // Pour bonus chiffrement
};
```

**Impact** :
- Taille header passe de 16 → 28 bytes (+12 bytes/fichier)
- Support flags pour features futures
- dataSize et originalSize en UINT64 (fichiers > 4 GB)

---

### 2. `Archive.h`

**Changements** :
```cpp
// Avant
struct FileEntry {
    char   filename[256];
    UINT64 offset;
    UINT64 size;
    UINT32 crc32;        // ← Redondant avec Stream::Header
};
// Total: 280 bytes

// Après
struct FileEntry {
    char   filename[256];
    UINT64 offset;       // Pointe vers Stream::Header maintenant
    UINT64 size;         // Inclut sizeof(Stream::Header) + dataSize
};
// Total: 272 bytes (-8 bytes/fichier)
```

**Impact** :
- Retrait du champ `crc32` (maintenant dans Stream::Header.checksum)
- `entry.offset` pointe vers Stream::Header (pas directement les données)
- `entry.size` = sizeof(Stream::Header) + taille données

---

### 3. `Archive.cpp`

**Fonction** `Create()` :
```cpp
// Avant
fwrite(fileData.GetData(), 1, fileData.GetSize(), archive);  // RAW data
entry.crc32 = SafeReader::CalculateCRC32(...);               // Calcul manuel

// Après
Stream::Header streamHeader;
streamHeader.magic = "SAFE";
streamHeader.version = 1;
streamHeader.flags = Stream::NONE;
streamHeader.dataSize = fileData.GetSize();
streamHeader.originalSize = fileData.GetSize();
streamHeader.checksum = SafeReader::CalculateCRC32(...);

fwrite(&streamHeader, 1, sizeof(Stream::Header), archive);   // Header d'abord
fwrite(fileData.GetData(), 1, fileData.GetSize(), archive);  // Puis data
```

**Fonction** `Extract()` :
```cpp
// Avant
fseek(archive, entry.offset, SEEK_SET);
fread(buffer, 1, entry.size, archive);                      // Lire RAW
UINT32 crc = SafeReader::CalculateCRC32(...);
if (crc != entry.crc32) return false;                       // Valider avec FileEntry

// Après
fseek(archive, entry.offset, SEEK_SET);
Stream::Header streamHeader;
fread(&streamHeader, 1, sizeof(Stream::Header), archive);   // Lire header d'abord
// Valider magic "SAFE"
fread(buffer, 1, streamHeader.dataSize, archive);           // Lire data
UINT32 crc = SafeReader::CalculateCRC32(...);
if (crc != streamHeader.checksum) return false;             // Valider avec Stream::Header
```

**Fonction** `List()` :
```cpp
// Avant
std::cout << entry.filename << " (" << entry.size << " bytes, CRC32: 0x"
          << entry.crc32 << ")\n";                            // CRC depuis FileEntry

// Après
Stream::Header streamHeader;
fread(&streamHeader, 1, sizeof(Stream::Header), archive);
std::cout << entry.filename << " (" << streamHeader.dataSize << " bytes, CRC32: 0x"
          << streamHeader.checksum << ")\n";                  // CRC depuis Stream::Header
```

**Fonction** `Validate()` :
```cpp
// Avant
UINT32 crc = SafeReader::CalculateCRC32(...);
if (crc == entry.crc32) std::cout << "[OK]";                // Compare avec FileEntry

// Après
Stream::Header streamHeader;
fread(&streamHeader, 1, sizeof(Stream::Header), archive);
// Valider magic "SAFE"
UINT32 crc = SafeReader::CalculateCRC32(...);
if (crc == streamHeader.checksum) std::cout << "[OK]";       // Compare avec Stream::Header
```

---

### 4. `SafeReader.cpp`

**Fonction** `WriteFile()` :
```cpp
// Avant
header.version = version;             // UINT32
header.dataSize = (UINT32)size;       // Cast vers UINT32
// Pas de flags

// Après
header.version = (UINT16)version;     // UINT16
header.flags = Stream::NONE;          // NOUVEAU
header.dataSize = size;               // UINT64 (pas de cast)
header.originalSize = size;           // NOUVEAU
```

**Fonction** `Validate()` :
```cpp
// Avant
long fileSize = ftell(file);
long expectedSize = sizeof(Stream::Header) + header.dataSize;

// Après
UINT64 fileSize = ftell(file);
UINT64 expectedSize = sizeof(Stream::Header) + header.dataSize;  // dataSize = UINT64
```

---

## 🔍 Structure binaire comparée

### Avant v1
```
game.asset (2682 bytes pour 2 fichiers de 2048 B + 42 B)

[0x000] ArchiveHeader (32 bytes)
[0x020] logo.png RAW data (2048 bytes)       ← Pas de header
[0x820] readme.txt RAW data (42 bytes)       ← Pas de header
[0x84A] FileEntry logo.png (280 bytes)
          - filename, offset, size, crc32
[0x962] FileEntry readme.txt (280 bytes)
          - filename, offset, size, crc32

Total: 32 + 2090 + 560 = 2682 bytes
```

### Après v2
```
game.asset (2738 bytes pour 2 fichiers de 2048 B + 42 B)

[0x000] ArchiveHeader (32 bytes)
[0x020] Stream::Header (28 bytes)            ← NOUVEAU header
[0x03C] logo.png data (2048 bytes)
[0x83C] Stream::Header (28 bytes)            ← NOUVEAU header
[0x858] readme.txt data (42 bytes)
[0x882] FileEntry logo.png (272 bytes)       ← Pas de crc32
          - filename, offset, size
[0x992] FileEntry readme.txt (272 bytes)     ← Pas de crc32
          - filename, offset, size

Total: 32 + 2076 + 56 + 544 = 2738 bytes (+56 bytes = 2 × 28 bytes header)
```

**Overhead** : +28 bytes par fichier (acceptable pour les avantages)

---

## 🎯 Compilation

### Étapes
1. Ouvrir `ide/AssetEngine.sln` dans Visual Studio 2022
2. Configuration : **x64 / Debug** (ou Release)
3. **Build → Rebuild Solution** (Ctrl + Shift + B)
4. Vérifier qu'il n'y a **aucune erreur**

### Warnings attendus
- Possible warning "cast UINT64 → long" dans SafeReader (peut être ignoré)
- Pas d'autres warnings attendus

### En cas d'erreur
- Erreur "structure size mismatch" → Rebuild All (clean cache)
- Erreur "undefined reference" → Vérifier que `Stream::NONE` est bien défini
- Erreur de linking → Vérifier que tous les .cpp sont dans le .vcxproj

---

## ✅ Tests à effectuer après compilation

### Test 1 : Créer archive
```cmd
AssetEngine.exe create test.asset file1.txt file2.txt
```

**Vérifications** :
- Archive créée sans erreur
- Taille ≈ taille fichiers + overhead (32 + N×28 + N×272)

### Test 2 : Lister archive
```cmd
AssetEngine.exe list test.asset
```

**Attendu** :
```
==========================================
Archive: test.asset
Files: 2
==========================================
[1] file1.txt (XXX bytes, CRC32: 0xXXXXXXXX)
[2] file2.txt (XXX bytes, CRC32: 0xXXXXXXXX)
==========================================
```

### Test 3 : Valider intégrité
```cmd
AssetEngine.exe validate test.asset
```

**Attendu** :
```
Validating archive: test.asset
Files to check: 2
[OK] file1.txt
[OK] file2.txt
Archive is valid.
```

### Test 4 : Extraire fichier
```cmd
AssetEngine.exe extract test.asset file1.txt extracted_file1.txt
```

**Vérifications** :
- Fichier extrait créé
- Contenu identique à l'original (diff ou comp)

### Test 5 : Extraire tout
```cmd
AssetEngine.exe extractall test.asset extracted/
```

**Vérifications** :
- Dossier `extracted/` créé
- Tous les fichiers présents
- Contenu identique aux originaux

---

## 🚀 Prochaines étapes (optionnel)

### Bonus CDC : Compression zlib
```cpp
// Dans Archive::Create()
if (streamHeader.dataSize > 1024) {  // Compresser si > 1 KB
    UINT8* compressed = new UINT8[fileSize];
    uLongf compressedSize = fileSize;

    if (compress2(compressed, &compressedSize, fileData.GetData(), fileSize, Z_BEST_COMPRESSION) == Z_OK) {
        if (compressedSize < fileSize * 0.9) {  // Gain > 10%
            streamHeader.flags = Stream::COMPRESSED;
            streamHeader.dataSize = compressedSize;
            streamHeader.originalSize = fileSize;
            fwrite(&streamHeader, 1, sizeof(Stream::Header), archive);
            fwrite(compressed, 1, compressedSize, archive);
        }
    }
}
```

### Bonus CDC : Chiffrement AES
```cpp
streamHeader.flags = Stream::ENCRYPTED;
// Chiffrer fileData avec AES-256
```

---

## 📊 Comparaison finale

| Aspect | v1 (avant) | v2 (après) |
|--------|------------|------------|
| **Hiérarchie** | ArchiveHeader → RAW data → FileEntry | ArchiveHeader → Stream::Header → Data → FileEntry |
| **CRC32** | 2 sources (FileEntry + calcul manuel) | 1 source (Stream::Header uniquement) |
| **Validation** | Code custom dans Archive.cpp | Réutilisation SafeReader (cohérent) |
| **Compression** | ❌ Impossible sans tout refaire | ✅ Ajouter flag + zlib (facile) |
| **Chiffrement** | ❌ Impossible | ✅ Ajouter flag + AES (facile) |
| **Overhead** | 0 bytes/fichier | +28 bytes/fichier (header) |
| **FileEntry size** | 280 bytes | 272 bytes (-8 bytes, crc32 retiré) |
| **Extensibilité** | ❌ Faible | ✅ Excellente (flags, version) |
| **Code quality** | ❌ Duplication (2 systèmes CRC32) | ✅ DRY (une seule source de vérité) |

---

**Conclusion** : Architecture v2 est **cohérente, extensible et prête pour les bonus du CDC** (compression, chiffrement). L'overhead de +28 bytes/fichier est négligeable par rapport aux avantages.
