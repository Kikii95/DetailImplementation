# Asset Engine - Guide Détaillé Complet

**Date** : 28 octobre 2025
**Projet** : Asset Engine (GTECH 3)
**Version** : 2.0 (Version Pédagogique Détaillée)

> Ce document explique EN DÉTAIL chaque concept, chaque fonction, et chaque mécanisme du système d'archivage. Parfait si tu veux comprendre exactement comment tout fonctionne sous le capot.

---

## 📚 Table des matières

1. [Concepts de base](#concepts-de-base)
2. [Qu'est-ce qu'une archive ?](#quest-ce-quune-archive-)
3. [Le système CRC32](#le-système-crc32)
4. [SafeReader expliqué en détail](#safereader-expliqué-en-détail)
5. [Le format d'archive .asset](#le-format-darchive-asset)
6. [Archive.cpp : Chaque fonction expliquée](#archivecpp--chaque-fonction-expliquée)
7. [Flux de données visuels](#flux-de-données-visuels)
8. [Exemples concrets pas à pas](#exemples-concrets-pas-à-pas)
9. [Debugging et troubleshooting](#debugging-et-troubleshooting)

---

## 🧩 Concepts de base

### Qu'est-ce qu'un fichier binaire ?

Un fichier contient des **bytes** (octets). Chaque byte = 8 bits = valeur 0-255.

**Exemple fichier texte "Hello"** :
```
Offset | Byte (hex) | Byte (dec) | Caractère ASCII
-------|------------|------------|----------------
0x00   | 0x48       | 72         | 'H'
0x01   | 0x6C       | 108        | 'e'
0x02   | 0x6C       | 108        | 'l'
0x03   | 0x6C       | 108        | 'l'
0x04   | 0x6F       | 111        | 'o'
```

**Fichier binaire** = données brutes (pas forcément du texte) :
```
Offset | Byte (hex) | Signification
-------|------------|---------------
0x00   | 0x41       | Magic 'A'
0x01   | 0x53       | Magic 'S'
0x02   | 0x45       | Magic 'E'
0x03   | 0x54       | Magic 'T'
0x04   | 0x01       | Version (1, little-endian part 1)
0x05   | 0x00       | Version (part 2)
0x06   | 0x00       | Version (part 3)
0x07   | 0x00       | Version (part 4)
```

---

## 📦 Qu'est-ce qu'une archive ?

### Analogie du monde réel

Imagine une **valise** :
- Tu as **3 vêtements** (fichiers)
- Tu les ranges dans **1 valise** (archive)
- La valise a une **étiquette** (header) qui dit "3 vêtements à l'intérieur"
- Tu peux **ouvrir la valise** (list) et voir ce qu'il y a
- Tu peux **sortir un vêtement** (extract) pour le porter
- La valise reste intacte, tu as juste une **copie** du vêtement

### Dans le monde informatique

**AVANT l'archive** :
```
Disque dur :
├── file1.txt (25 bytes)
├── file2.txt (42 bytes)
└── file3.dat (100 bytes)

Total : 3 fichiers physiques
```

**APRÈS création de l'archive** :
```
Disque dur :
├── file1.txt (25 bytes)      ← Toujours là
├── file2.txt (42 bytes)      ← Toujours là
├── file3.dat (100 bytes)     ← Toujours là
└── archive.asset (502 bytes) ← NOUVEAU (contient les 3 autres)

Contenu de archive.asset :
  [Header 32 bytes]
  [file1.txt data: 25 bytes]
  [file2.txt data: 42 bytes]
  [file3.dat data: 100 bytes]
  [Index table: 3 x 280 = 840 bytes]

Total archive = 32 + 167 + 840 = 1039 bytes
```

### Pourquoi créer des archives ?

1. **Transport** : 1 fichier à envoyer au lieu de 1000
2. **Organisation** : Tous les assets d'un jeu dans 1 seul .asset
3. **Intégrité** : CRC32 garantit qu'aucun fichier n'est corrompu
4. **Performance** : Lecture séquentielle plus rapide que 1000 petits fichiers

---

## 🔐 Le système CRC32

### Qu'est-ce qu'un checksum ?

Un **checksum** (somme de contrôle) = empreinte numérique d'un fichier.

**Analogie** : C'est comme un **code-barres unique** sur un produit.
- Si le produit change, le code-barres ne correspond plus
- Tu peux détecter si quelqu'un a modifié le produit

### CRC32 expliqué simplement

**CRC** = Cyclic Redundancy Check
**32** = Le résultat est un nombre de 32 bits (4 bytes)

**Propriétés** :
- Même fichier → **toujours** le même CRC32
- Fichier modifié (1 seul bit) → CRC32 **complètement différent**
- Probabilité de collision : ~1 sur 4 milliards

**Exemple concret** :
```
Fichier "Hello" :
  Données : 0x48 0x65 0x6C 0x6C 0x6F
  CRC32   : 0xF7D18982

Fichier "Hello!" (ajout d'un '!') :
  Données : 0x48 0x65 0x6C 0x6C 0x6F 0x21
  CRC32   : 0x3610A686  ← Complètement différent !
```

### Comment on calcule le CRC32 ?

**Méthode naïve (LENTE)** :
```cpp
// Calcul bit par bit (très lent)
for chaque bit du fichier :
    if (bit XOR crc_bit) :
        crc = (crc >> 1) XOR polynomial
    else:
        crc = crc >> 1
```

**Méthode optimisée (RAPIDE)** :
```cpp
// Utilise une table pré-calculée
static UINT32 table[256] = { /* 256 valeurs pré-calculées */ };

UINT32 crc = 0xFFFFFFFF;  // Valeur de départ

for (chaque byte du fichier) {
    index = (crc ^ byte) & 0xFF;     // Prendre les 8 bits de poids faible
    crc = (crc >> 8) ^ table[index]; // Shift et XOR avec table
}

return ~crc;  // Inverser tous les bits
```

**Pourquoi c'est rapide ?**
- On traite **1 byte à la fois** au lieu de 1 bit
- Table de 256 entrées = **lookup instantané**
- Pas de divisions ni de calculs complexes

**Table CRC32 (extrait)** :
```cpp
static const UINT32 CRC32_TABLE[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, // Indices 0-3
    0x076DC419, 0x706AF48F, 0xE963A535, 0x9E6495A3, // Indices 4-7
    // ... 248 autres entrées
};
```

Ces valeurs sont calculées **une seule fois** avec le polynomial `0xEDB88320`.

### Exemple pas à pas

**Fichier** : "AB" (2 bytes)
```
Données : 0x41 ('A'), 0x42 ('B')
```

**Calcul** :
```
Étape 1: Initialisation
  crc = 0xFFFFFFFF

Étape 2: Traiter byte 0x41
  index = (0xFFFFFFFF ^ 0x41) & 0xFF = 0xBE
  crc = (0xFFFFFFFF >> 8) ^ CRC32_TABLE[0xBE]
  crc = 0x00FFFFFF ^ 0x7F6EF5C6
  crc = 0x7F910AC9

Étape 3: Traiter byte 0x42
  index = (0x7F910AC9 ^ 0x42) & 0xFF = 0x8B
  crc = (0x7F910AC9 >> 8) ^ CRC32_TABLE[0x8B]
  crc = 0x007F910A ^ 0xB40BBE37
  crc = 0xB3742F3D

Étape 4: Finalisation
  result = ~0xB3742F3D = 0x4C8BD0C2
```

**Résultat** : `CRC32("AB") = 0x4C8BD0C2`

---

## 🛠️ SafeReader expliqué en détail

### Vue d'ensemble

**SafeReader** = Boîte à outils pour manipuler des fichiers de manière **sécurisée**.

**4 fonctions principales** :
1. `FileCheck()` - Vérifier qu'un fichier existe et est accessible
2. `CalculateCRC32()` - Calculer l'empreinte d'un fichier
3. `Validate()` - Lire un fichier avec header "SAFE" et valider son intégrité
4. `WriteFile()` - Écrire un fichier avec header "SAFE" et CRC32

---

### 1. `FileCheck(filename)` - Vérifier un fichier

**But** : Savoir si un fichier existe et est accessible.

**Code simplifié** :
```cpp
bool SafeReader::FileCheck(const std::string& filename)
{
    // 1. Vérifier que le nom n'est pas vide
    if (filename.empty())
        return false;

    // 2. stat() = fonction système pour obtenir infos fichier
    struct stat fileInfo;
    if (stat(filename.c_str(), &fileInfo) != 0)
        return false;  // Fichier n'existe pas

    // 3. Vérifier que c'est un fichier régulier (pas un dossier)
    if (!(fileInfo.st_mode & S_IFREG))
        return false;

    // 4. Tenter d'ouvrir en lecture
    FILE* testFile = nullptr;
    errno_t err = fopen_s(&testFile, filename.c_str(), "rb");

    if (err != 0 || testFile == nullptr)
        return false;  // Pas de permissions ou erreur

    fclose(testFile);
    return true;  // Tout OK !
}
```

**Exemple d'utilisation** :
```cpp
if (!SafeReader::FileCheck("texture.png")) {
    std::cerr << "Erreur : fichier inaccessible\n";
    return false;
}
// Fichier OK, on peut continuer
```

**Cas d'erreur détectés** :
- ❌ Nom vide : `""`
- ❌ Fichier inexistant : `"notfound.txt"`
- ❌ Dossier au lieu de fichier : `"C:\Windows\"`
- ❌ Permissions insuffisantes : fichier en lecture seule ou verrouillé

---

### 2. `CalculateCRC32(data, size)` - Calculer l'empreinte

**But** : Calculer le CRC32 d'un bloc de données en mémoire.

**Code complet** :
```cpp
UINT32 SafeReader::CalculateCRC32(const UINT8* data, UINT64 size)
{
    // Validation
    if (data == nullptr || size == 0)
        return 0;

    // Initialisation : tous les bits à 1
    UINT32 crc = 0xFFFFFFFF;

    // Boucle sur chaque byte
    for (UINT64 i = 0; i < size; i++)
    {
        // Prendre les 8 bits de poids faible
        UINT8 index = (crc ^ data[i]) & 0xFF;

        // Shift de 8 bits + XOR avec la table
        crc = (crc >> 8) ^ CRC32_TABLE[index];
    }

    // Inverser tous les bits et retourner
    return ~crc;
}
```

**Exemple visuel** :

```
Données : [0x48, 0x65, 0x6C, 0x6C, 0x6F] = "Hello"

crc = 0xFFFFFFFF
  ↓
[0x48] → index=0xB7 → crc = 0xFF000000 ^ TABLE[0xB7] = 0x7C8C5F9A
  ↓
[0x65] → index=0xFF → crc = 0x007C8C5F ^ TABLE[0xFF] = 0x15A2B9C3
  ↓
[0x6C] → index=0xAF → crc = 0x0015A2B9 ^ TABLE[0xAF] = 0x82F18998
  ↓
[0x6C] → index=0xF4 → crc = 0x0082F189 ^ TABLE[0xF4] = 0x21E7D67C
  ↓
[0x6F] → index=0x13 → crc = 0x0021E7D6 ^ TABLE[0x13] = 0x082E767D
  ↓
~crc = ~0x082E767D = 0xF7D18982

Résultat : CRC32("Hello") = 0xF7D18982
```

---

### 3. `Validate(filename, outHeader, outData)` - Valider un fichier

**But** : Lire un fichier avec le format **Stream::Header** (magic "SAFE") et vérifier son intégrité.

**Format Stream::Header** (16 bytes) :
```cpp
struct Header {
    char   magic[4];    // "SAFE"
    UINT32 version;     // Numéro de version
    UINT64 dataSize;    // Taille des données
    UINT32 checksum;    // CRC32 des données
};
```

**Code simplifié** :
```cpp
bool SafeReader::Validate(const std::string& filename,
                          Stream::Header& outHeader,
                          Blob& outData)
{
    // 1. Vérifier que le fichier existe
    if (!FileCheck(filename))
        return false;

    // 2. Ouvrir le fichier
    FILE* file = nullptr;
    fopen_s(&file, filename.c_str(), "rb");
    if (!file) return false;

    // 3. Lire le header (16 bytes)
    fread(&outHeader, sizeof(Stream::Header), 1, file);

    // 4. Vérifier le magic "SAFE"
    if (strncmp(outHeader.magic, "SAFE", 4) != 0) {
        fclose(file);
        return false;  // Pas un fichier SAFE valide
    }

    // 5. Vérifier que la taille est cohérente
    fseek(file, 0, SEEK_END);
    UINT64 fileSize = ftell(file);
    UINT64 expectedSize = sizeof(Stream::Header) + outHeader.dataSize;

    if (fileSize != expectedSize) {
        fclose(file);
        return false;  // Fichier tronqué ou corrompu
    }

    // 6. Lire les données
    fseek(file, sizeof(Stream::Header), SEEK_SET);

    UINT8 buffer[1024];
    UINT64 bytesRead = 0;

    while (bytesRead < outHeader.dataSize) {
        UINT64 toRead = min(1024, outHeader.dataSize - bytesRead);
        size_t read = fread(buffer, 1, toRead, file);

        outData.Append(buffer, read);  // Ajouter au Blob
        bytesRead += read;
    }

    fclose(file);

    // 7. Calculer CRC32 et comparer
    UINT32 calculatedCRC = CalculateCRC32(outData.GetData(), outData.GetSize());

    if (calculatedCRC != outHeader.checksum) {
        return false;  // CRC invalide = fichier corrompu !
    }

    return true;  // Fichier valide !
}
```

**Exemple d'utilisation** :
```cpp
Stream::Header header;
Blob data;

if (SafeReader::Validate("myfile.safe", header, data)) {
    std::cout << "Fichier valide !\n";
    std::cout << "Version: " << header.version << "\n";
    std::cout << "Taille: " << header.dataSize << " bytes\n";
    // Utiliser data...
} else {
    std::cerr << "Fichier invalide ou corrompu !\n";
}
```

**Flux de validation visuel** :
```
myfile.safe (50 bytes)
  ↓
[Lire header: 16 bytes]
  magic    = "SAFE" ✓
  version  = 1
  dataSize = 34
  checksum = 0x12345678
  ↓
[Vérifier taille fichier]
  Taille attendue = 16 + 34 = 50 bytes ✓
  Taille réelle   = 50 bytes ✓
  ↓
[Lire données: 34 bytes en chunks de 1024]
  Chunk 1: 34 bytes → Blob
  ↓
[Calculer CRC32 des données lues]
  CRC calculé = 0x12345678
  CRC header  = 0x12345678 ✓
  ↓
Résultat : VALIDE !
```

---

### 4. `WriteFile(filename, data, size, version)` - Écrire un fichier validé

**But** : Créer un fichier avec header "SAFE" et CRC32 automatiquement calculé.

**Code simplifié** :
```cpp
bool SafeReader::WriteFile(const std::string& filename,
                           const UINT8* data,
                           UINT64 size,
                           UINT32 version)
{
    // 1. Validations
    assert(data != nullptr);
    assert(size > 0);

    // 2. Construire le header
    Stream::Header header;
    memcpy(header.magic, "SAFE", 4);
    header.version = version;
    header.dataSize = size;
    header.checksum = CalculateCRC32(data, size);  // Calcul auto

    // 3. Ouvrir fichier en écriture binaire
    FILE* file = nullptr;
    errno_t err = fopen_s(&file, filename.c_str(), "wb");

    if (err != 0 || file == nullptr)
        return false;

    // 4. Écrire le header
    fwrite(&header, sizeof(Stream::Header), 1, file);

    // 5. Écrire les données
    fwrite(data, 1, size, file);

    // 6. Fermer et retourner succès
    fclose(file);
    return true;
}
```

**Exemple d'utilisation** :
```cpp
const char* text = "Hello World!";
UINT64 size = strlen(text);

bool success = SafeReader::WriteFile("output.safe",
                                     (UINT8*)text,
                                     size,
                                     1);  // version = 1

if (success) {
    std::cout << "Fichier créé avec succès\n";
}
```

**Structure du fichier créé** :
```
output.safe (28 bytes)

Offset | Bytes          | Description
-------|----------------|---------------------------
0x00   | 53 41 46 45    | Magic "SAFE"
0x04   | 01 00 00 00    | Version = 1 (little-endian)
0x08   | 0C 00 00 00    | DataSize = 12 bytes
       | 00 00 00 00    |
0x10   | AB CD 12 34    | Checksum (CRC32 calculé)
0x14   | 48 65 6C 6C    | Données: "Hello World!"
0x18   | 6F 20 57 6F    |
0x1C   | 72 6C 64 21    |
```

---

## 🗂️ Le format d'archive .asset

### Vue d'ensemble

Une archive `.asset` contient **3 sections** :

```
╔═══════════════════════════════════════════════════════════╗
║                    ARCHIVE STRUCTURE                      ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  ┌─────────────────────────────────────────────────┐     ║
║  │  SECTION 1: ARCHIVE HEADER (32 bytes)          │     ║
║  │  - Magic "ASET"                                 │     ║
║  │  - Version, file count                          │     ║
║  │  - Offsets vers data et index                   │     ║
║  └─────────────────────────────────────────────────┘     ║
║                     ↓                                     ║
║  ┌─────────────────────────────────────────────────┐     ║
║  │  SECTION 2: DATA SECTION (variable)            │     ║
║  │  - Blob du fichier 1                            │     ║
║  │  - Blob du fichier 2                            │     ║
║  │  - ...                                          │     ║
║  │  - Blob du fichier N                            │     ║
║  └─────────────────────────────────────────────────┘     ║
║                     ↓                                     ║
║  ┌─────────────────────────────────────────────────┐     ║
║  │  SECTION 3: FILE INDEX TABLE (280 bytes/file)  │     ║
║  │  - FileEntry 1 (nom, offset, size, CRC32)      │     ║
║  │  - FileEntry 2                                  │     ║
║  │  - ...                                          │     ║
║  │  - FileEntry N                                  │     ║
║  └─────────────────────────────────────────────────┘     ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

### Section 1 : ArchiveHeader (32 bytes)

**Structure C++** :
```cpp
struct ArchiveHeader {
    char   magic[4];      // "ASET" (0x41534554)
    UINT32 version;       // Version du format (actuellement: 1)
    UINT32 fileCount;     // Nombre de fichiers dans l'archive
    UINT64 indexOffset;   // Position de la table d'index
    UINT64 dataOffset;    // Position des données (toujours 32)
    UINT32 archiveCRC;    // CRC32 global (réservé, pas utilisé)
};
```

**Disposition mémoire** :
```
Offset | Taille | Type   | Nom          | Description
-------|--------|--------|--------------|---------------------------
0x00   | 4      | char[] | magic        | Signature "ASET"
0x04   | 4      | UINT32 | version      | Numéro de version
0x08   | 4      | UINT32 | fileCount    | Nombre de fichiers
0x0C   | 8      | UINT64 | indexOffset  | Offset vers la table d'index
0x14   | 8      | UINT64 | dataOffset   | Offset vers les données (= 32)
0x1C   | 4      | UINT32 | archiveCRC   | CRC32 global (réservé)
-------|--------|--------|--------------|---------------------------
Total: 32 bytes
```

**Exemple concret** (archive avec 2 fichiers de 100 bytes chacun) :
```
Offset | Hex bytes               | Interprétation
-------|-------------------------|--------------------------------
0x00   | 41 53 45 54             | magic = "ASET"
0x04   | 01 00 00 00             | version = 1 (little-endian)
0x08   | 02 00 00 00             | fileCount = 2
0x0C   | E8 00 00 00 00 00 00 00 | indexOffset = 232 (0xE8)
0x14   | 20 00 00 00 00 00 00 00 | dataOffset = 32 (0x20)
0x1C   | 00 00 00 00             | archiveCRC = 0 (non utilisé)
```

**Pourquoi indexOffset = 232 ?**
```
Header      : 32 bytes
File 1 data : 100 bytes
File 2 data : 100 bytes
--------------------------
Total data  : 232 bytes ← C'est là que commence l'index
```

---

### Section 2 : Data Section (variable)

Cette section contient les **données brutes** de tous les fichiers, bout à bout.

**Exemple** : Archive de 3 fichiers :
```
file1.txt : "Hello"     (5 bytes)
file2.txt : "World!"    (6 bytes)
file3.dat : [0x01 0x02] (2 bytes)
```

**Data Section** :
```
Offset dans archive | Contenu             | Description
--------------------|---------------------|------------------
0x0020              | 48 65 6C 6C 6F      | "Hello" (file1.txt)
0x0025              | 57 6F 72 6C 64 21   | "World!" (file2.txt)
0x002B              | 01 02               | Binary data (file3.dat)
--------------------|---------------------|------------------
Fin de data section : 0x002D (45 bytes après le début)
```

**Important** : Pas de séparateurs entre les fichiers. On utilise les **offsets** dans la table d'index pour savoir où commence/finit chaque fichier.

---

### Section 3 : File Index Table (280 bytes par fichier)

**Structure C++** :
```cpp
struct FileEntry {
    char   filename[256]; // Nom du fichier (null-terminated)
    UINT64 offset;        // Position dans la data section
    UINT64 size;          // Taille en bytes
    UINT32 crc32;         // CRC32 du fichier
    // Padding implicite: 4 bytes pour aligner à 8
};
```

**Disposition mémoire** :
```
Offset | Taille | Type     | Nom      | Description
-------|--------|----------|----------|---------------------------
0x00   | 256    | char[]   | filename | Nom du fichier + '\0'
0x100  | 8      | UINT64   | offset   | Position dans l'archive
0x108  | 8      | UINT64   | size     | Taille du fichier
0x110  | 4      | UINT32   | crc32    | CRC32 du fichier
0x114  | 4      | (padding)| -        | Alignement automatique
-------|--------|----------|----------|---------------------------
Total: 280 bytes
```

**Exemple concret** (pour file1.txt ci-dessus) :
```
Offset | Hex bytes                  | Interprétation
-------|----------------------------|--------------------------------
0x00   | 66 69 6C 65 31 2E 74 78 74 | "file1.txt\0" + 246 bytes de padding
       | 00 00 00 ... (246 zeros)   |
0x100  | 20 00 00 00 00 00 00 00    | offset = 0x0020 (32 bytes)
0x108  | 05 00 00 00 00 00 00 00    | size = 5 bytes
0x110  | 82 89 D1 F7                | crc32 = 0xF7D18982
0x114  | 00 00 00 00                | padding
```

---

### Archive complète - Exemple visuel

**Fichiers à archiver** :
```
logo.png : 2048 bytes (CRC: 0xAABBCCDD)
data.txt : 42 bytes   (CRC: 0x11223344)
```

**Résultat : archive.asset (2682 bytes)** :
```
╔═══════════════════════════════════════════════════════════════════╗
║ OFFSET     │ SIZE      │ SECTION                                  ║
╠═══════════════════════════════════════════════════════════════════╣
║ 0x00000000 │ 32 bytes  │ ArchiveHeader                            ║
║            │           │   magic = "ASET"                         ║
║            │           │   version = 1                            ║
║            │           │   fileCount = 2                          ║
║            │           │   indexOffset = 2122 (0x084A)            ║
║            │           │   dataOffset = 32 (0x0020)               ║
╠═══════════════════════════════════════════════════════════════════╣
║ 0x00000020 │ 2048      │ DATA: logo.png                           ║
║            │ bytes     │   [binary data...]                       ║
╠═══════════════════════════════════════════════════════════════════╣
║ 0x00000820 │ 42 bytes  │ DATA: data.txt                           ║
║            │           │   "This is some text content..."         ║
╠═══════════════════════════════════════════════════════════════════╣
║ 0x0000084A │ 280 bytes │ INDEX: FileEntry 1                       ║
║            │           │   filename = "logo.png\0..."             ║
║            │           │   offset = 32 (0x20)                     ║
║            │           │   size = 2048                            ║
║            │           │   crc32 = 0xAABBCCDD                     ║
╠═══════════════════════════════════════════════════════════════════╣
║ 0x00000962 │ 280 bytes │ INDEX: FileEntry 2                       ║
║            │           │   filename = "data.txt\0..."             ║
║            │           │   offset = 2080 (0x820)                  ║
║            │           │   size = 42                              ║
║            │           │   crc32 = 0x11223344                     ║
╠═══════════════════════════════════════════════════════════════════╣
║ 0x00000A7A │ EOF       │                                          ║
╚═══════════════════════════════════════════════════════════════════╝

Total: 2682 bytes (0xA7A)
Calcul: 32 + 2048 + 42 + (2 × 280) = 2682
```

---

## 🔧 Archive.cpp : Chaque fonction expliquée

### 1. `Create(archivePath, filePaths)` - Créer une archive

**But** : Prendre plusieurs fichiers et les combiner dans une seule archive.

**Algorithme détaillé** :

```
┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 1 : INITIALISATION                                  │
└────────────────────────────────────────────────────────────┘
  1.1. Ouvrir archive.asset en mode écriture binaire ("wb")
  1.2. Créer un ArchiveHeader avec valeurs initiales :
       - magic = "ASET"
       - version = 1
       - fileCount = nombre de fichiers à archiver
       - dataOffset = 32 (juste après le header)
       - indexOffset = 0 (on le calculera plus tard)
  1.3. Écrire ce header temporaire (on le corrigera à la fin)
  1.4. Créer un vecteur vide pour stocker les FileEntry

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 2 : TRAITER CHAQUE FICHIER SOURCE                   │
└────────────────────────────────────────────────────────────┘
  Pour chaque fichier dans filePaths :

    2.1. Ouvrir le fichier source en lecture binaire
         Si échec → skip ce fichier, continuer avec les autres

    2.2. Obtenir la taille du fichier :
         - fseek(file, 0, SEEK_END)  // Aller à la fin
         - size = ftell(file)         // Lire la position = taille
         - fseek(file, 0, SEEK_SET)   // Retour au début

    2.3. Mémoriser la position actuelle dans l'archive :
         currentOffset = ftell(archiveFile)

    2.4. Lire le fichier par chunks de 1024 bytes :
         buffer[1024]
         while (pas fini) {
             bytesRead = fread(buffer, 1, 1024, sourceFile)
             fwrite(buffer, 1, bytesRead, archiveFile)

             // En même temps, calculer CRC32
             Append buffer au Blob temporaire
         }

    2.5. Calculer le CRC32 final :
         crc32 = SafeReader::CalculateCRC32(blob.GetData(), blob.GetSize())

    2.6. Extraire juste le nom du fichier (sans le chemin) :
         Exemple: "C:\textures\logo.png" → "logo.png"

    2.7. Créer un FileEntry :
         entry.filename = "logo.png" (copier dans char[256])
         entry.offset = currentOffset
         entry.size = size
         entry.crc32 = crc32

    2.8. Ajouter l'entry au vecteur

    2.9. Fermer le fichier source

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 3 : ÉCRIRE LA TABLE D'INDEX                         │
└────────────────────────────────────────────────────────────┘
  3.1. Mémoriser la position actuelle :
       indexOffset = ftell(archiveFile)
       (C'est là que commence la table d'index)

  3.2. Pour chaque FileEntry dans le vecteur :
       fwrite(&entry, sizeof(FileEntry), 1, archiveFile)

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 4 : METTRE À JOUR LE HEADER                         │
└────────────────────────────────────────────────────────────┘
  4.1. Retourner au début du fichier :
       fseek(archiveFile, 0, SEEK_SET)

  4.2. Mettre à jour le header avec le bon indexOffset :
       header.indexOffset = indexOffset

  4.3. Réécrire le header :
       fwrite(&header, sizeof(ArchiveHeader), 1, archiveFile)

  4.4. Fermer l'archive

┌────────────────────────────────────────────────────────────┐
│ RÉSULTAT : Archive créée avec succès !                    │
└────────────────────────────────────────────────────────────┘
```

**Exemple concret pas à pas** :

```
Input: Create("game.asset", {"texture.png", "sound.wav"})

─────────────────────────────────────────────────────────────
ÉTAPE 1 : Initialisation
─────────────────────────────────────────────────────────────
Créer game.asset
Écrire header temporaire:
  [0x00-0x1F] ArchiveHeader (indexOffset = 0 pour l'instant)

Position dans archive : 0x20 (32 bytes)

─────────────────────────────────────────────────────────────
ÉTAPE 2A : Traiter texture.png (2048 bytes)
─────────────────────────────────────────────────────────────
Ouvrir texture.png
Lire taille: 2048 bytes
Position actuelle: 0x20 (32)

Lire par chunks:
  Chunk 1: 1024 bytes → écrire dans archive à 0x20
  Chunk 2: 1024 bytes → écrire dans archive à 0x420

CRC32: 0xAABBCCDD

Créer FileEntry:
  filename = "texture.png\0" + padding
  offset = 0x20
  size = 2048
  crc32 = 0xAABBCCDD

Position dans archive : 0x820 (2080 bytes)

─────────────────────────────────────────────────────────────
ÉTAPE 2B : Traiter sound.wav (10240 bytes)
─────────────────────────────────────────────────────────────
Ouvrir sound.wav
Lire taille: 10240 bytes
Position actuelle: 0x820 (2080)

Lire par chunks:
  Chunk 1: 1024 bytes → écrire dans archive à 0x820
  Chunk 2: 1024 bytes → écrire à 0xC20
  ...
  Chunk 10: 1024 bytes → écrire à 0x2C20

CRC32: 0x12345678

Créer FileEntry:
  filename = "sound.wav\0" + padding
  offset = 0x820
  size = 10240
  crc32 = 0x12345678

Position dans archive : 0x3020 (12320 bytes)

─────────────────────────────────────────────────────────────
ÉTAPE 3 : Écrire la table d'index
─────────────────────────────────────────────────────────────
Position actuelle: 0x3020 = indexOffset

Écrire FileEntry 1 (texture.png) : 280 bytes à 0x3020
Écrire FileEntry 2 (sound.wav)   : 280 bytes à 0x3138

Position finale : 0x3250 (12880 bytes)

─────────────────────────────────────────────────────────────
ÉTAPE 4 : Mettre à jour le header
─────────────────────────────────────────────────────────────
Retour au début (fseek 0)
Mettre à jour header.indexOffset = 0x3020 (12320)
Réécrire header à 0x00

Fermer archive

─────────────────────────────────────────────────────────────
RÉSULTAT FINAL
─────────────────────────────────────────────────────────────
game.asset : 12880 bytes

Structure:
  [0x0000-0x001F] Header (32 bytes)
  [0x0020-0x081F] texture.png data (2048 bytes)
  [0x0820-0x301F] sound.wav data (10240 bytes)
  [0x3020-0x3137] FileEntry texture.png (280 bytes)
  [0x3138-0x324F] FileEntry sound.wav (280 bytes)
```

---

### 2. `List(archivePath)` - Lister le contenu

**But** : Afficher tous les fichiers contenus dans l'archive.

**Algorithme** :

```
┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 1 : OUVRIR ET LIRE LE HEADER                        │
└────────────────────────────────────────────────────────────┘
  1.1. Ouvrir archive en lecture binaire ("rb")
  1.2. Lire ArchiveHeader (32 bytes)
  1.3. Vérifier magic == "ASET"
       Si non → retourner false (pas une archive valide)
  1.4. Extraire fileCount et indexOffset

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 2 : AFFICHER LES INFOS GÉNÉRALES                    │
└────────────────────────────────────────────────────────────┘
  2.1. Afficher:
       "=========================================="
       "Archive: <nom du fichier>"
       "Files: <fileCount>"
       "=========================================="

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 3 : LIRE ET AFFICHER CHAQUE FICHIER                 │
└────────────────────────────────────────────────────────────┘
  3.1. Se déplacer vers l'index table :
       fseek(archiveFile, indexOffset, SEEK_SET)

  3.2. Pour i de 1 à fileCount :
       - Lire FileEntry (280 bytes)
       - Afficher:
         "[<i>] <filename> (<size> bytes, CRC32: 0x<crc32>)"

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 4 : TERMINER                                         │
└────────────────────────────────────────────────────────────┘
  4.1. Afficher:
       "=========================================="
  4.2. Fermer l'archive
  4.3. Retourner true
```

**Exemple concret** :

```
Input: List("game.asset")

Output:
==========================================
Archive: game.asset
Files: 3
==========================================
[1] texture.png (2048 bytes, CRC32: 0xAABBCCDD)
[2] mesh.obj (5120 bytes, CRC32: 0x11223344)
[3] audio.wav (10240 bytes, CRC32: 0x55667788)
==========================================
```

---

### 3. `Extract(archivePath, filename, outputPath)` - Extraire un fichier

**But** : Copier un fichier spécifique de l'archive vers le disque.

**Algorithme détaillé** :

```
┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 1 : OUVRIR L'ARCHIVE ET LIRE LE HEADER              │
└────────────────────────────────────────────────────────────┘
  1.1. Ouvrir archive en lecture ("rb")
  1.2. Lire ArchiveHeader
  1.3. Valider magic "ASET"

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 2 : CHERCHER LE FICHIER DANS L'INDEX                │
└────────────────────────────────────────────────────────────┘
  2.1. Se déplacer vers l'index table :
       fseek(archive, header.indexOffset, SEEK_SET)

  2.2. Pour chaque fichier (0 à fileCount) :
       - Lire FileEntry
       - Comparer entry.filename avec le nom recherché (strcmp)
       - Si match trouvé → GOTO ÉTAPE 3

  2.3. Si aucun match → retourner false (fichier non trouvé)

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 3 : LIRE LES DONNÉES DU FICHIER                     │
└────────────────────────────────────────────────────────────┘
  3.1. Se déplacer vers les données :
       fseek(archive, entry.offset, SEEK_SET)

  3.2. Lire entry.size bytes en chunks de 1024 :
       buffer[1024]
       Blob data

       bytesRead = 0
       while (bytesRead < entry.size) {
           toRead = min(1024, entry.size - bytesRead)
           fread(buffer, 1, toRead, archive)
           data.Append(buffer, toRead)
           bytesRead += toRead
       }

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 4 : VALIDER LE CRC32                                │
└────────────────────────────────────────────────────────────┘
  4.1. Calculer CRC32 des données lues :
       calculatedCRC = SafeReader::CalculateCRC32(data.GetData(), data.GetSize())

  4.2. Comparer avec entry.crc32 :
       Si différent → retourner false (fichier corrompu !)

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 5 : ÉCRIRE LE FICHIER DE SORTIE                     │
└────────────────────────────────────────────────────────────┘
  5.1. Ouvrir outputPath en écriture binaire ("wb")
  5.2. Écrire data.GetData() (entry.size bytes)
  5.3. Fermer le fichier de sortie
  5.4. Fermer l'archive
  5.5. Retourner true (succès)
```

**Exemple visuel** :

```
Input: Extract("game.asset", "mesh.obj", "extracted_mesh.obj")

─────────────────────────────────────────────────────────────
ÉTAPE 1 : Lecture header
─────────────────────────────────────────────────────────────
Ouvrir game.asset
Lire header:
  magic = "ASET" ✓
  version = 1
  fileCount = 3
  indexOffset = 0x3020

─────────────────────────────────────────────────────────────
ÉTAPE 2 : Chercher "mesh.obj" dans l'index
─────────────────────────────────────────────────────────────
Seek vers 0x3020 (début de l'index)

Lire FileEntry 1:
  filename = "texture.png"
  strcmp("texture.png", "mesh.obj") → NON

Lire FileEntry 2:
  filename = "mesh.obj"
  strcmp("mesh.obj", "mesh.obj") → OUI ! ✓

  Mémoriser:
    offset = 0x820
    size = 5120
    crc32 = 0x11223344

─────────────────────────────────────────────────────────────
ÉTAPE 3 : Lire les données
─────────────────────────────────────────────────────────────
Seek vers 0x820 (position du fichier)

Lire 5120 bytes en chunks:
  Chunk 1: lire 1024 bytes à 0x820
  Chunk 2: lire 1024 bytes à 0xC20
  Chunk 3: lire 1024 bytes à 0x1020
  Chunk 4: lire 1024 bytes à 0x1420
  Chunk 5: lire 1024 bytes à 0x1820

Blob data : 5120 bytes

─────────────────────────────────────────────────────────────
ÉTAPE 4 : Validation CRC32
─────────────────────────────────────────────────────────────
CalculateCRC32(data, 5120) = 0x11223344
Comparaison:
  Calculé : 0x11223344
  Attendu : 0x11223344
  → MATCH ✓ Fichier intègre

─────────────────────────────────────────────────────────────
ÉTAPE 5 : Écriture du fichier
─────────────────────────────────────────────────────────────
Créer extracted_mesh.obj
Écrire 5120 bytes
Fermer le fichier

Résultat: extracted_mesh.obj créé (5120 bytes)
```

---

### 4. `ExtractAll(archivePath, outputDir)` - Extraire tout

**But** : Extraire tous les fichiers de l'archive dans un dossier.

**Algorithme** :

```
┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 1 : PRÉPARATION                                      │
└────────────────────────────────────────────────────────────┘
  1.1. Créer le dossier de sortie :
       _mkdir(outputDir)
       (Ignore l'erreur si le dossier existe déjà)

  1.2. Ouvrir archive et lire header
  1.3. Se déplacer vers l'index table

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 2 : BOUCLE SUR TOUS LES FICHIERS                    │
└────────────────────────────────────────────────────────────┘
  Pour chaque FileEntry (0 à fileCount) :

    2.1. Lire FileEntry

    2.2. Construire chemin de sortie :
         outputPath = outputDir + "/" + entry.filename
         Exemple: "extracted/mesh.obj"

    2.3. Sauvegarder position actuelle dans l'index :
         fgetpos(archive, &indexPos)

    2.4. Se déplacer vers les données du fichier :
         fseek(archive, entry.offset, SEEK_SET)

    2.5. Lire entry.size bytes en chunks

    2.6. Valider CRC32
         Si invalide → afficher erreur mais continuer

    2.7. Écrire fichier de sortie

    2.8. Retourner à la position dans l'index :
         fsetpos(archive, &indexPos)

    2.9. Continuer avec le prochain fichier

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 3 : TERMINER                                         │
└────────────────────────────────────────────────────────────┘
  3.1. Fermer l'archive
  3.2. Retourner true si au moins 1 fichier extrait
```

**Exemple visuel** :

```
Input: ExtractAll("game.asset", "extracted_files")

─────────────────────────────────────────────────────────────
ÉTAPE 1 : Préparation
─────────────────────────────────────────────────────────────
Créer dossier: extracted_files/
Lire header: fileCount = 3

─────────────────────────────────────────────────────────────
ÉTAPE 2 : Extraction
─────────────────────────────────────────────────────────────
Position dans index: 0x3020

[Fichier 1/3]
  Lire FileEntry: "texture.png"
  Sauvegarder position index: 0x3020
  Seek vers données: 0x0020
  Lire 2048 bytes
  Valider CRC: OK ✓
  Écrire: extracted_files/texture.png
  Retour à position index: 0x3020
  Avancer vers prochain entry: 0x3138

[Fichier 2/3]
  Lire FileEntry: "mesh.obj"
  Sauvegarder position: 0x3138
  Seek vers données: 0x0820
  Lire 5120 bytes
  Valider CRC: OK ✓
  Écrire: extracted_files/mesh.obj
  Retour à position: 0x3138
  Avancer vers prochain entry: 0x3250

[Fichier 3/3]
  Lire FileEntry: "audio.wav"
  Sauvegarder position: 0x3250
  Seek vers données: 0x2C20
  Lire 10240 bytes
  Valider CRC: OK ✓
  Écrire: extracted_files/audio.wav

─────────────────────────────────────────────────────────────
RÉSULTAT
─────────────────────────────────────────────────────────────
extracted_files/
  ├── texture.png (2048 bytes)
  ├── mesh.obj (5120 bytes)
  └── audio.wav (10240 bytes)

3 fichiers extraits avec succès !
```

---

### 5. `Validate(archivePath)` - Valider l'intégrité

**But** : Vérifier que tous les fichiers de l'archive sont intacts (pas corrompus).

**Algorithme** :

```
┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 1 : INITIALISATION                                   │
└────────────────────────────────────────────────────────────┘
  1.1. Ouvrir archive et lire header
  1.2. Afficher: "Validating archive: <nom>"
  1.3. Afficher: "Files to check: <fileCount>"
  1.4. Initialiser compteur d'erreurs: errorCount = 0

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 2 : VALIDER CHAQUE FICHIER                          │
└────────────────────────────────────────────────────────────┘
  Pour chaque FileEntry dans l'index :

    2.1. Lire FileEntry

    2.2. Seek vers entry.offset

    2.3. Lire entry.size bytes

    2.4. Calculer CRC32

    2.5. Comparer avec entry.crc32 :

         Si MATCH :
           Afficher: "[OK] <filename>"

         Si MISMATCH :
           Afficher: "[FAIL] <filename> (CRC mismatch)"
           Incrémenter errorCount

┌────────────────────────────────────────────────────────────┐
│ ÉTAPE 3 : RÉSULTAT FINAL                                   │
└────────────────────────────────────────────────────────────┘
  3.1. Si errorCount == 0 :
       Afficher: "Archive is valid."
       Retourner true

  3.2. Si errorCount > 0 :
       Afficher: "Archive has <errorCount> corrupted file(s)."
       Retourner false
```

**Exemple** :

```
Input: Validate("game.asset")

Output:
Validating archive: game.asset
Files to check: 3
[OK] texture.png
[FAIL] mesh.obj (CRC mismatch)
[OK] audio.wav
Archive has 1 corrupted file(s).

Retour: false
```

---

## 🌊 Flux de données visuels

### Création d'archive - Flux complet

```
┌─────────────────────────────────────────────────────────────────┐
│                    FLUX DE CRÉATION                             │
└─────────────────────────────────────────────────────────────────┘

Fichiers source                  Mémoire/Traitement              Archive finale
─────────────                    ──────────────────              ──────────────

file1.txt (100 B)                                                archive.asset
    │                                                            ┌────────────┐
    │ fread()                                                    │   Header   │
    ├──────────→ [Buffer 1024B] ──────────→ Blob (100 B)       │  32 bytes  │
    │                                            │               ├────────────┤
    │                                            │ CRC32         │            │
    │                                            └───────→ 0xABCD│ file1 data │
    │                                                            │ 100 bytes  │
file2.txt (200 B)                                                │            │
    │                                                            ├────────────┤
    │ fread()                                                    │            │
    ├──────────→ [Buffer 1024B] ──────────→ Blob (200 B)       │ file2 data │
    │                                            │               │ 200 bytes  │
    │                                            │ CRC32         │            │
    │                                            └───────→ 0x1234├────────────┤
    │                                                            │ FileEntry1 │
    │                                                            │ 280 bytes  │
    │                                                            ├────────────┤
    │                                                            │ FileEntry2 │
    │                                                            │ 280 bytes  │
    │                                                            └────────────┘
    │
    │                         Vecteur FileEntry                  Total: 892 B
    │                         ─────────────────
    │                         [Entry 1] → filename, offset, size, crc32
    │                         [Entry 2] → filename, offset, size, crc32
    │
    └─────────→ fwrite() vers Index Table
```

---

### Extraction - Flux complet

```
┌─────────────────────────────────────────────────────────────────┐
│                    FLUX D'EXTRACTION                            │
└─────────────────────────────────────────────────────────────────┘

Archive source              Recherche dans index           Fichier extrait
──────────────              ────────────────────           ───────────────

archive.asset
┌────────────┐
│   Header   │ ←─ fread(32 bytes)
├────────────┤       │
│  Data...   │       │ magic == "ASET" ? ✓
├────────────┤       │
│   Index    │ ←─────┘ fseek(indexOffset)
│            │
│ Entry 1    │ ←─── fread(FileEntry)
│ "logo.png" │      strcmp("logo.png", "mesh.obj") → NON
│            │
│ Entry 2    │ ←─── fread(FileEntry)
│ "mesh.obj" │      strcmp("mesh.obj", "mesh.obj") → OUI !
│ offset:0x20│      ───────────────────────┐
│ size: 5120 │                             │
│ crc: 0x... │                             │
│            │                             │
│ Entry 3    │                             │
│ "data.txt" │                             │
└────────────┘                             │
       │                                   │
       │ fseek(0x20)                       │
       ├───────────────────────────────────┘
       │
       │ fread(5120 bytes)
       ├──────────→ [Buffer] ──────→ Blob (5120 B)
       │                                   │
       │                                   │ CalculateCRC32()
       │                                   │
       │                                   ├─→ 0x11223344
       │                                   │   Compare avec entry.crc32
       │                                   │   → MATCH ✓
       │                                   │
       │                                   │ fwrite()
       │                                   └──────────→ extracted_mesh.obj
       │                                                   (5120 bytes)
       │
       └─── Fermer archive
```

---

### Validation - Flux complet

```
┌─────────────────────────────────────────────────────────────────┐
│                    FLUX DE VALIDATION                           │
└─────────────────────────────────────────────────────────────────┘

archive.asset            Vérification                     Résultat
─────────────            ─────────────                    ─────────

┌────────────┐
│ Header     │ ←─ Lire
│ fileCount:3│
├────────────┤
│ Index      │
│            │
│ [Entry 1]  │ ←─┐
│ offset:0x20│   │
│ size: 100  │   │ 1. Seek vers 0x20
│ crc:0xAAAA │   │ 2. Lire 100 bytes
│            │   │ 3. CalculateCRC32() → 0xAAAA
├────────────┤   │ 4. Compare: 0xAAAA == 0xAAAA ✓    "[OK] file1.txt"
│ Data 0x20  │ ←─┘
│ (100 B)    │
├────────────┤
│ [Entry 2]  │ ←─┐
│ offset:0x84│   │
│ size: 200  │   │ 1. Seek vers 0x84
│ crc:0xBBBB │   │ 2. Lire 200 bytes
│            │   │ 3. CalculateCRC32() → 0xCCCC
├────────────┤   │ 4. Compare: 0xCCCC != 0xBBBB ✗    "[FAIL] file2.txt"
│ Data 0x84  │ ←─┘
│ (200 B)    │
│ (CORROMPU) │
├────────────┤
│ [Entry 3]  │ ←─┐
│ offset:0x14C│  │
│ size: 50   │   │ 1. Seek vers 0x14C
│ crc:0xDDDD │   │ 2. Lire 50 bytes
│            │   │ 3. CalculateCRC32() → 0xDDDD
├────────────┤   │ 4. Compare: 0xDDDD == 0xDDDD ✓    "[OK] file3.dat"
│ Data 0x14C │ ←─┘
│ (50 B)     │
└────────────┘

                                                        Résultat final:
                                                        "Archive has 1 corrupted file(s)."
                                                        return false
```

---

## 🔍 Exemples concrets pas à pas

### Scénario complet : De zéro à l'extraction

#### Setup initial
```
Dossier: C:\project\
  ├── AssetEngine.exe
  ├── assets\
  │   ├── logo.png (2048 bytes)
  │   └── readme.txt (42 bytes, contenu: "Welcome to the game!")
  └── (vide pour l'instant)
```

#### Étape 1 : Créer l'archive

**Commande** :
```cmd
AssetEngine.exe create game.asset assets\logo.png assets\readme.txt
```

**Qu'est-ce qui se passe ?**

1. **Parser les arguments** :
   ```
   argv[0] = "AssetEngine.exe"
   argv[1] = "create"
   argv[2] = "game.asset"
   argv[3] = "assets\logo.png"
   argv[4] = "assets\readme.txt"
   ```

2. **Appeler `Archive::Create()`** :
   ```cpp
   std::vector<std::string> files = {
       "assets\\logo.png",
       "assets\\readme.txt"
   };
   Archive::Create("game.asset", files);
   ```

3. **Traitement interne** :

   **Phase A : Header initial**
   ```
   Créer game.asset
   Position: 0x00

   Écrire ArchiveHeader:
     magic = "ASET"
     version = 1
     fileCount = 2
     indexOffset = 0 (temporaire)
     dataOffset = 32
     archiveCRC = 0

   Position après header: 0x20 (32 bytes)
   ```

   **Phase B : Traiter logo.png**
   ```
   Ouvrir assets\logo.png
   Taille: 2048 bytes
   Position actuelle dans archive: 0x20

   Lire et écrire par chunks:
     Chunk 1: lire 1024 bytes → écrire à 0x20
     Chunk 2: lire 1024 bytes → écrire à 0x420

   Calculer CRC32: 0xAABBCCDD

   Créer FileEntry:
     filename[256] = "logo.png\0" + 247 bytes de padding
     offset = 0x20
     size = 2048
     crc32 = 0xAABBCCDD

   Position après logo.png: 0x820 (2080 bytes)
   ```

   **Phase C : Traiter readme.txt**
   ```
   Ouvrir assets\readme.txt
   Taille: 42 bytes
   Position actuelle: 0x820

   Lire et écrire:
     Chunk 1: lire 42 bytes → écrire à 0x820

   Calculer CRC32: 0x11223344

   Créer FileEntry:
     filename[256] = "readme.txt\0" + 246 bytes de padding
     offset = 0x820
     size = 42
     crc32 = 0x11223344

   Position après readme.txt: 0x84A (2122 bytes)
   ```

   **Phase D : Écrire l'index table**
   ```
   Position actuelle: 0x84A
   indexOffset = 0x84A (mémorisé)

   Écrire FileEntry logo.png (280 bytes) à 0x84A
   Écrire FileEntry readme.txt (280 bytes) à 0x962

   Position finale: 0xA7A (2682 bytes)
   ```

   **Phase E : Mettre à jour le header**
   ```
   fseek(0, SEEK_SET) → retour au début

   Mettre à jour header:
     indexOffset = 0x84A (2122)

   Réécrire header à 0x00
   Fermer game.asset
   ```

4. **Résultat final** :
   ```
   game.asset créé (2682 bytes)

   Structure:
     [0x000-0x01F] Header (32 bytes)
     [0x020-0x81F] logo.png data (2048 bytes)
     [0x820-0x849] readme.txt data (42 bytes)
     [0x84A-0x961] FileEntry logo.png (280 bytes)
     [0x962-0xA79] FileEntry readme.txt (280 bytes)

   Calcul: 32 + 2048 + 42 + 560 = 2682 bytes ✓
   ```

---

#### Étape 2 : Lister le contenu

**Commande** :
```cmd
AssetEngine.exe list game.asset
```

**Output** :
```
==========================================
Archive: game.asset
Files: 2
==========================================
[1] logo.png (2048 bytes, CRC32: 0xAABBCCDD)
[2] readme.txt (42 bytes, CRC32: 0x11223344)
==========================================
```

**Qu'est-ce qui s'est passé ?**
```
1. Ouvrir game.asset
2. Lire header → fileCount = 2, indexOffset = 0x84A
3. Seek vers 0x84A
4. Lire Entry 1 → "logo.png", afficher
5. Lire Entry 2 → "readme.txt", afficher
6. Fermer
```

---

#### Étape 3 : Extraire readme.txt

**Commande** :
```cmd
AssetEngine.exe extract game.asset readme.txt extracted_readme.txt
```

**Déroulement détaillé** :

```
┌─────────────────────────────────────────────────────────────┐
│ RECHERCHE DANS L'INDEX                                      │
└─────────────────────────────────────────────────────────────┘

Ouvrir game.asset
Lire header → indexOffset = 0x84A
Seek vers 0x84A

Lire Entry 1:
  filename = "logo.png"
  strcmp("logo.png", "readme.txt") → NON, continuer

Lire Entry 2:
  filename = "readme.txt"
  strcmp("readme.txt", "readme.txt") → OUI ! ✓

  Données trouvées:
    offset = 0x820
    size = 42
    crc32 = 0x11223344

┌─────────────────────────────────────────────────────────────┐
│ EXTRACTION DES DONNÉES                                      │
└─────────────────────────────────────────────────────────────┘

Seek vers 0x820
Lire 42 bytes:
  57 65 6C 63 6F 6D 65 20   |Welcome |
  74 6F 20 74 68 65 20 67   |to the g|
  61 6D 65 21               |ame!    |

Stocker dans Blob (42 bytes)

┌─────────────────────────────────────────────────────────────┐
│ VALIDATION CRC32                                            │
└─────────────────────────────────────────────────────────────┘

CalculateCRC32(Blob, 42) → 0x11223344
Comparer:
  Calculé : 0x11223344
  Attendu : 0x11223344
  → MATCH ✓ Données intègres

┌─────────────────────────────────────────────────────────────┐
│ ÉCRITURE FICHIER DE SORTIE                                  │
└─────────────────────────────────────────────────────────────┘

Créer extracted_readme.txt
Écrire 42 bytes
Fermer fichier

SUCCÈS !
```

**Résultat** :
```
extracted_readme.txt créé (42 bytes)
Contenu: "Welcome to the game!"
```

---

#### Étape 4 : Valider l'intégrité

**Commande** :
```cmd
AssetEngine.exe validate game.asset
```

**Output** :
```
Validating archive: game.asset
Files to check: 2
[OK] logo.png
[OK] readme.txt
Archive is valid.
```

**Scénario : Fichier corrompu**

Simulons une corruption (modification manuelle avec hex editor) :
```
Offset 0x825 : Changer 0x6F ('o') → 0x00
```

Maintenant :
```cmd
AssetEngine.exe validate game.asset
```

**Output** :
```
Validating archive: game.asset
Files to check: 2
[OK] logo.png
[FAIL] readme.txt (CRC mismatch)
Archive has errors.
```

**Pourquoi ?**
```
Données originales (0x820-0x849):
  "Welcome to the game!"
  CRC32 → 0x11223344

Données corrompues:
  "Welc\0me to the game!"  (byte modifié)
  CRC32 → 0x99887766

Comparaison:
  0x99887766 != 0x11223344 → ÉCHEC
```

---

## 🐛 Debugging et troubleshooting

### Problème 1 : "Fichier non trouvé lors de l'extraction"

**Symptôme** :
```
AssetEngine.exe extract game.asset Logo.png output.png
Error: File not found in archive
```

**Cause** : Sensibilité à la casse.

**Debug** :
```cmd
AssetEngine.exe list game.asset
```
**Output** :
```
[1] logo.png  ← Minuscule 'l'
```

**Solution** : Utiliser le nom EXACT :
```cmd
AssetEngine.exe extract game.asset logo.png output.png
```

---

### Problème 2 : "CRC mismatch après extraction"

**Symptôme** :
```
[FAIL] file.txt (CRC mismatch)
```

**Causes possibles** :
1. Archive corrompue (transfert réseau incomplet, disque défectueux)
2. Modification manuelle de l'archive
3. Bug dans le code (peu probable si tests passent)

**Debug** :

**Étape 1 : Vérifier taille du fichier**
```cmd
dir game.asset
```
Si taille != attendue → fichier tronqué

**Étape 2 : Hex dump de l'archive**
Utiliser un hex editor (HxD, 010 Editor) :
```
0x00: 41 53 45 54  ← "ASET" OK
0x04: 01 00 00 00  ← version = 1 OK
0x08: 02 00 00 00  ← fileCount = 2 OK
```

**Étape 3 : Recalculer CRC32 manuellement**
```cpp
// Code de test
FILE* f = fopen("extracted_file.txt", "rb");
// Lire contenu
UINT32 crc = SafeReader::CalculateCRC32(data, size);
printf("CRC calculé: 0x%08X\n", crc);
```

Comparer avec valeur dans l'index.

---

### Problème 3 : "Archive vide après création"

**Symptôme** :
```
game.asset créé (32 bytes)  ← Trop petit !
```

**Cause** : Fichiers sources introuvables.

**Debug** :
```cpp
// Dans Archive::Create(), ajouter log temporaire:
if (!FileCheck(filepath)) {
    std::cout << "SKIP: " << filepath << " (not found)\n";
}
```

**Solution** : Vérifier chemins :
```cmd
AssetEngine.exe create game.asset assets\*.txt
```
Si `assets\` n'existe pas → aucun fichier ajouté.

---

### Problème 4 : "Crash lors de l'extraction"

**Symptôme** :
```
AssetEngine.exe extract game.asset file.txt output.txt
[Crash / Segmentation fault]
```

**Cause possible** : Blob trop gros, dépassement mémoire.

**Debug avec assert()** :
```cpp
// Dans Extract()
assert(entry.size < 1000000000);  // Max 1 GB
```

Si assertion échoue → fichier gigantesque dans l'archive.

**Solution** : Limiter taille maximale ou utiliser streaming au lieu de charger en mémoire.

---

### Outils de debugging

#### 1. Hex Editor
**HxD (Windows gratuit)** :
- Ouvrir game.asset
- Vérifier magic "ASET" à l'offset 0
- Vérifier structure binaire

#### 2. Logs temporaires
Ajouter des `std::cout` temporaires :
```cpp
std::cout << "DEBUG: Seeking to " << entry.offset << "\n";
std::cout << "DEBUG: Reading " << entry.size << " bytes\n";
```
Retirer après debug.

#### 3. Assertions
```cpp
assert(entry.size > 0);
assert(entry.offset >= sizeof(ArchiveHeader));
```

---

## 🎓 Conclusion

### Ce que tu as appris

1. **CRC32** : Comment calculer une empreinte pour détecter les corruptions
2. **Format binaire** : Comment structurer des données (header + data + index)
3. **Lecture/écriture par chunks** : Comment traiter de gros fichiers efficacement
4. **Offsets et seeking** : Comment naviguer dans un fichier binaire
5. **CLI parsing** : Comment créer une interface en ligne de commande

### Concepts avancés

Si tu veux aller plus loin :
- **Compression** : Intégrer zlib pour réduire la taille
- **Chiffrement** : AES pour protéger les données
- **Index en mémoire** : Cache pour accélérer les recherches
- **Multi-threading** : Archiver plusieurs fichiers en parallèle

---

**Version** : 2.0 (Pédagogique Complète)
**Date** : 28 octobre 2025
**Auteur** : Asset Engine Documentation
