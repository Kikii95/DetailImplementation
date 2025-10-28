#include "pch.h"
#include "Archive.h"
#include "SafeReader.h"
#include <sys/stat.h>
#include <direct.h>
#include <algorithm>
#include <random>
#include <chrono>

UINT64 Archive::GenerateFileID(const std::string& filename)
{
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

    UINT32 hash = SafeReader::CalculateCRC32((const UINT8*)filename.c_str(), filename.size());

    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<UINT64> dis(0, 0xFFFFFFFF);
    UINT64 random = dis(gen);

    return (timestamp << 32) | (hash ^ (random & 0xFFFFFFFF));
}

UINT32 Archive::CountActiveFiles(const std::vector<FileEntry>& entries)
{
    UINT32 count = 0;
    for (const auto& entry : entries)
    {
        if (entry.flags & FILE_ACTIVE)
            count++;
    }
    return count;
}

FileEntry* Archive::FindDeletedEntry(std::vector<FileEntry>& entries, UINT64 minSize)
{
    for (auto& entry : entries)
    {
        if ((entry.flags & FILE_DELETED) && entry.size >= minSize)
            return &entry;
    }
    return nullptr;
}

std::string Archive::GetBasename(const std::string& path)
{
    size_t lastSlash = path.find_last_of("/\\");
    return (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;
}

bool Archive::ReadArchiveHeader(File& archive, ArchiveHeader& header)
{
    assert(archive.IsOpen() && "Archive file must be open");

    UINT64 bytesRead = archive.Read((UINT8*)&header, sizeof(ArchiveHeader), 1);
    if (bytesRead != sizeof(ArchiveHeader))
        return false;

    if (header.magic[0] != 'A' || header.magic[1] != 'S' ||
        header.magic[2] != 'E' || header.magic[3] != 'T')
        return false;

    return true;
}

bool Archive::WriteArchiveHeader(File& archive, const ArchiveHeader& header)
{
    assert(archive.IsOpen() && "Archive file must be open");

    UINT64 written = archive.Write((const UINT8*)&header, sizeof(ArchiveHeader), 1);
    return (written == sizeof(ArchiveHeader));
}

std::vector<FileEntry> Archive::ReadIndex(File& archive, const ArchiveHeader& header)
{
    std::vector<FileEntry> entries;
    entries.reserve(header.fileCount);

    archive.Seek(header.indexOffset, SEEK_SET);

    for (UINT32 i = 0; i < header.fileCount; i++)
    {
        FileEntry entry;
        UINT64 bytesRead = archive.Read((UINT8*)&entry, sizeof(FileEntry), 1);
        if (bytesRead == sizeof(FileEntry))
            entries.push_back(entry);
    }

    return entries;
}

bool Archive::WriteIndex(File& archive, const std::vector<FileEntry>& entries, ArchiveHeader& header)
{
    UINT64 indexOffset = archive.Seek(0, SEEK_CUR);
    header.indexOffset = indexOffset;

    for (const auto& entry : entries)
    {
        UINT64 written = archive.Write((const UINT8*)&entry, sizeof(FileEntry), 1);
        if (written != sizeof(FileEntry))
            return false;
    }

    return true;
}

bool Archive::WriteFileWithHeader(File& archive, const std::string& filePath, UINT64& outID, UINT64& outSize)
{
    File inputFile;
    if (!inputFile.OpenRead(filePath))
        return false;

    inputFile.Seek(0, SEEK_END);
    UINT64 fileSize = inputFile.Seek(0, SEEK_CUR);
    inputFile.Seek(0, SEEK_SET);

    Blob fileData;
    fileData.Reserve(fileSize);

    UINT8 buffer[MAX_BUFFER_SIZE];
    UINT64 remaining = fileSize;

    while (remaining > 0)
    {
        UINT64 chunkSize = (remaining > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : remaining;
        UINT64 bytesRead = inputFile.Read(buffer, chunkSize, 1);
        if (bytesRead == 0)
            break;
        fileData.Append(buffer, bytesRead);
        remaining -= bytesRead;
    }

    inputFile.Close();

    Stream::Header streamHeader;
    streamHeader.magic[0] = 'S';
    streamHeader.magic[1] = 'A';
    streamHeader.magic[2] = 'F';
    streamHeader.magic[3] = 'E';
    streamHeader.version = 1;
    streamHeader.flags = Stream::NONE;
    streamHeader.dataSize = fileData.GetSize();
    streamHeader.originalSize = fileData.GetSize();
    streamHeader.checksum = SafeReader::CalculateCRC32(fileData.GetData(), fileData.GetSize());

    UINT64 headerWritten = archive.Write((const UINT8*)&streamHeader, sizeof(Stream::Header), 1);
    UINT64 dataWritten = archive.Write(fileData.GetData(), fileData.GetSize(), 1);

    if (headerWritten != sizeof(Stream::Header) || dataWritten != fileData.GetSize())
        return false;

    outID = GenerateFileID(filePath);
    outSize = sizeof(Stream::Header) + fileData.GetSize();

    return true;
}

bool Archive::Create(const std::string& archivePath, const std::vector<std::string>& filePaths)
{
    assert(!archivePath.empty() && "Archive path cannot be empty");
    assert(!filePaths.empty() && "File list cannot be empty");

    File archive;
    if (!archive.OpenWrite(archivePath))
        return false;

    ArchiveHeader header;
    header.magic[0] = 'A';
    header.magic[1] = 'S';
    header.magic[2] = 'E';
    header.magic[3] = 'T';
    header.version = 2;
    header.fileCount = static_cast<UINT32>(filePaths.size());
    header.dataOffset = sizeof(ArchiveHeader);
    header.indexOffset = 0;
    header.archiveCRC = 0;

    archive.Write((const UINT8*)&header, sizeof(ArchiveHeader), 1);

    std::vector<FileEntry> entries;
    entries.reserve(filePaths.size());

    UINT64 currentOffset = header.dataOffset;

    for (const auto& filePath : filePaths)
    {
        UINT64 fileID = 0;
        UINT64 fileSize = 0;

        archive.Seek(currentOffset, SEEK_SET);

        if (!WriteFileWithHeader(archive, filePath, fileID, fileSize))
            continue;

        FileEntry entry;
        memset(&entry, 0, sizeof(FileEntry));

        entry.id = fileID;
        std::string filename = GetBasename(filePath);
        strncpy_s(entry.filename, MAX_FILENAME_LENGTH, filename.c_str(), _TRUNCATE);
        entry.offset = currentOffset;
        entry.size = fileSize;
        entry.flags = FILE_ACTIVE;

        entries.push_back(entry);
        currentOffset += fileSize;
    }

    header.fileCount = static_cast<UINT32>(entries.size());

    if (!WriteIndex(archive, entries, header))
    {
        archive.Close();
        return false;
    }

    archive.Seek(0, SEEK_SET);
    WriteArchiveHeader(archive, header);

    archive.Close();
    return true;
}

bool Archive::List(const std::string& archivePath)
{
    File archive;
    if (!archive.OpenRead(archivePath))
        return false;

    ArchiveHeader header;
    if (!ReadArchiveHeader(archive, header))
    {
        archive.Close();
        return false;
    }

    std::vector<FileEntry> entries = ReadIndex(archive, header);

    std::cout << "==========================================\n";
    std::cout << "Archive: " << archivePath << "\n";
    std::cout << "Files: " << CountActiveFiles(entries) << " active";
    if (entries.size() > CountActiveFiles(entries))
        std::cout << " (" << entries.size() - CountActiveFiles(entries) << " deleted)";
    std::cout << "\n==========================================\n";

    UINT32 index = 1;
    for (const auto& entry : entries)
    {
        if (!(entry.flags & FILE_ACTIVE))
            continue;

        archive.Seek(entry.offset, SEEK_SET);

        Stream::Header streamHeader;
        UINT64 headerRead = archive.Read((UINT8*)&streamHeader, sizeof(Stream::Header), 1);

        if (headerRead == sizeof(Stream::Header))
        {
            std::cout << "[" << index << "] " << entry.filename
                      << " (ID: " << entry.id
                      << ", " << streamHeader.dataSize << " bytes"
                      << ", CRC32: 0x" << std::hex << std::uppercase << streamHeader.checksum << std::dec << ")\n";
        }
        index++;
    }

    std::cout << "==========================================\n";

    archive.Close();
    return true;
}

bool Archive::Extract(const std::string& archivePath, UINT64 fileID, const std::string& outputPath)
{
    File archive;
    if (!archive.OpenRead(archivePath))
        return false;

    ArchiveHeader header;
    if (!ReadArchiveHeader(archive, header))
    {
        archive.Close();
        return false;
    }

    std::vector<FileEntry> entries = ReadIndex(archive, header);

    auto it = std::find_if(entries.begin(), entries.end(),
        [fileID](const FileEntry& e) { return e.id == fileID && (e.flags & FILE_ACTIVE); });

    if (it == entries.end())
    {
        archive.Close();
        return false;
    }

    archive.Seek(it->offset, SEEK_SET);

    Stream::Header streamHeader;
    UINT64 headerRead = archive.Read((UINT8*)&streamHeader, sizeof(Stream::Header), 1);

    if (headerRead != sizeof(Stream::Header))
    {
        archive.Close();
        return false;
    }

    if (streamHeader.magic[0] != 'S' || streamHeader.magic[1] != 'A' ||
        streamHeader.magic[2] != 'F' || streamHeader.magic[3] != 'E')
    {
        archive.Close();
        return false;
    }

    Blob fileData;
    fileData.Reserve(streamHeader.dataSize);

    UINT8 buffer[MAX_BUFFER_SIZE];
    UINT64 remaining = streamHeader.dataSize;

    while (remaining > 0)
    {
        UINT64 chunkSize = (remaining > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : remaining;
        UINT64 bytesRead = archive.Read(buffer, chunkSize, 1);
        if (bytesRead == 0)
            break;
        fileData.Append(buffer, bytesRead);
        remaining -= bytesRead;
    }

    archive.Close();

    UINT32 calculatedCRC = SafeReader::CalculateCRC32(fileData.GetData(), fileData.GetSize());
    if (calculatedCRC != streamHeader.checksum)
        return false;

    File outputFile;
    if (!outputFile.OpenWrite(outputPath))
        return false;

    UINT64 written = outputFile.Write(fileData.GetData(), fileData.GetSize(), 1);
    outputFile.Close();

    return (written == fileData.GetSize());
}

bool Archive::ExtractByName(const std::string& archivePath, const std::string& filename, const std::string& outputPath)
{
    FileEntry entry;
    if (!FindByName(archivePath, filename, entry))
        return false;

    return Extract(archivePath, entry.id, outputPath);
}

bool Archive::ExtractAll(const std::string& archivePath, const std::string& outputDir)
{
    File archive;
    if (!archive.OpenRead(archivePath))
        return false;

    ArchiveHeader header;
    if (!ReadArchiveHeader(archive, header))
    {
        archive.Close();
        return false;
    }

    _mkdir(outputDir.c_str());

    std::vector<FileEntry> entries = ReadIndex(archive, header);

    for (const auto& entry : entries)
    {
        if (!(entry.flags & FILE_ACTIVE))
            continue;

        std::string outputPath = outputDir + "/" + entry.filename;

        archive.Seek(entry.offset, SEEK_SET);

        Stream::Header streamHeader;
        UINT64 headerRead = archive.Read((UINT8*)&streamHeader, sizeof(Stream::Header), 1);

        if (headerRead != sizeof(Stream::Header))
            continue;

        if (streamHeader.magic[0] != 'S' || streamHeader.magic[1] != 'A' ||
            streamHeader.magic[2] != 'F' || streamHeader.magic[3] != 'E')
            continue;

        Blob fileData;
        fileData.Reserve(streamHeader.dataSize);

        UINT8 buffer[MAX_BUFFER_SIZE];
        UINT64 remaining = streamHeader.dataSize;

        while (remaining > 0)
        {
            UINT64 chunkSize = (remaining > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : remaining;
            UINT64 bytesRead = archive.Read(buffer, chunkSize, 1);
            if (bytesRead == 0)
                break;
            fileData.Append(buffer, bytesRead);
            remaining -= bytesRead;
        }

        File outputFile;
        if (outputFile.OpenWrite(outputPath))
        {
            outputFile.Write(fileData.GetData(), fileData.GetSize(), 1);
            outputFile.Close();
        }
    }

    archive.Close();
    return true;
}

bool Archive::Validate(const std::string& archivePath)
{
    File archive;
    if (!archive.OpenRead(archivePath))
        return false;

    ArchiveHeader header;
    if (!ReadArchiveHeader(archive, header))
    {
        archive.Close();
        return false;
    }

    std::cout << "Validating archive: " << archivePath << "\n";
    std::cout << "Files to check: " << CountActiveFiles(ReadIndex(archive, header)) << "\n";

    std::vector<FileEntry> entries = ReadIndex(archive, header);

    bool allValid = true;

    for (const auto& entry : entries)
    {
        if (!(entry.flags & FILE_ACTIVE))
            continue;

        archive.Seek(entry.offset, SEEK_SET);

        Stream::Header streamHeader;
        UINT64 headerRead = archive.Read((UINT8*)&streamHeader, sizeof(Stream::Header), 1);

        if (headerRead != sizeof(Stream::Header))
        {
            std::cout << "[FAIL] " << entry.filename << " (invalid stream header)\n";
            allValid = false;
            continue;
        }

        if (streamHeader.magic[0] != 'S' || streamHeader.magic[1] != 'A' ||
            streamHeader.magic[2] != 'F' || streamHeader.magic[3] != 'E')
        {
            std::cout << "[FAIL] " << entry.filename << " (invalid magic)\n";
            allValid = false;
            continue;
        }

        Blob fileData;
        fileData.Reserve(streamHeader.dataSize);

        UINT8 buffer[MAX_BUFFER_SIZE];
        UINT64 remaining = streamHeader.dataSize;

        while (remaining > 0)
        {
            UINT64 chunkSize = (remaining > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : remaining;
            UINT64 bytesRead = archive.Read(buffer, chunkSize, 1);
            if (bytesRead == 0)
                break;
            fileData.Append(buffer, bytesRead);
            remaining -= bytesRead;
        }

        UINT32 calculatedCRC = SafeReader::CalculateCRC32(fileData.GetData(), fileData.GetSize());

        if (calculatedCRC == streamHeader.checksum)
        {
            std::cout << "[OK] " << entry.filename << "\n";
        }
        else
        {
            std::cout << "[FAIL] " << entry.filename << " (CRC mismatch)\n";
            allValid = false;
        }
    }

    archive.Close();

    std::cout << (allValid ? "Archive is valid.\n" : "Archive has errors.\n");

    return allValid;
}

bool Archive::AddFile(const std::string& archivePath, const std::string& filePath, UINT64& outFileID)
{
    File archive;
    if (!archive.Open(archivePath, WRITE))
        return false;

    ArchiveHeader header;
    if (!ReadArchiveHeader(archive, header))
    {
        archive.Close();
        return false;
    }

    std::vector<FileEntry> entries = ReadIndex(archive, header);

    File inputFile;
    if (!inputFile.OpenRead(filePath))
    {
        archive.Close();
        return false;
    }

    inputFile.Seek(0, SEEK_END);
    UINT64 fileSize = inputFile.Seek(0, SEEK_CUR);
    inputFile.Close();

    UINT64 neededSize = sizeof(Stream::Header) + fileSize;

    FileEntry* reusableEntry = FindDeletedEntry(entries, neededSize);

    UINT64 insertOffset;
    if (reusableEntry != nullptr)
    {
        insertOffset = reusableEntry->offset;
        archive.Seek(insertOffset, SEEK_SET);

        UINT64 fileID = 0;
        UINT64 writtenSize = 0;
        if (!WriteFileWithHeader(archive, filePath, fileID, writtenSize))
        {
            archive.Close();
            return false;
        }

        reusableEntry->id = fileID;
        std::string filename = GetBasename(filePath);
        strncpy_s(reusableEntry->filename, MAX_FILENAME_LENGTH, filename.c_str(), _TRUNCATE);
        reusableEntry->size = writtenSize;
        reusableEntry->flags = FILE_ACTIVE;
        outFileID = fileID;
    }
    else
    {
        insertOffset = header.indexOffset;
        archive.Seek(insertOffset, SEEK_SET);

        UINT64 fileID = 0;
        UINT64 writtenSize = 0;
        if (!WriteFileWithHeader(archive, filePath, fileID, writtenSize))
        {
            archive.Close();
            return false;
        }

        FileEntry newEntry;
        memset(&newEntry, 0, sizeof(FileEntry));
        newEntry.id = fileID;
        std::string filename = GetBasename(filePath);
        strncpy_s(newEntry.filename, MAX_FILENAME_LENGTH, filename.c_str(), _TRUNCATE);
        newEntry.offset = insertOffset;
        newEntry.size = writtenSize;
        newEntry.flags = FILE_ACTIVE;

        entries.push_back(newEntry);
        outFileID = fileID;
    }

    header.fileCount = static_cast<UINT32>(entries.size());

    if (!WriteIndex(archive, entries, header))
    {
        archive.Close();
        return false;
    }

    archive.Seek(0, SEEK_SET);
    WriteArchiveHeader(archive, header);

    archive.Close();
    return true;
}

bool Archive::RemoveFile(const std::string& archivePath, UINT64 fileID)
{
    File archive;
    if (!archive.Open(archivePath, WRITE))
        return false;

    ArchiveHeader header;
    if (!ReadArchiveHeader(archive, header))
    {
        archive.Close();
        return false;
    }

    std::vector<FileEntry> entries = ReadIndex(archive, header);

    auto it = std::find_if(entries.begin(), entries.end(),
        [fileID](const FileEntry& e) { return e.id == fileID; });

    if (it == entries.end())
    {
        archive.Close();
        return false;
    }

    it->flags = FILE_DELETED;

    if (!WriteIndex(archive, entries, header))
    {
        archive.Close();
        return false;
    }

    archive.Seek(0, SEEK_SET);
    WriteArchiveHeader(archive, header);

    archive.Close();
    return true;
}

bool Archive::RenameFile(const std::string& archivePath, UINT64 fileID, const std::string& newName)
{
    File archive;
    if (!archive.Open(archivePath, WRITE))
        return false;

    ArchiveHeader header;
    if (!ReadArchiveHeader(archive, header))
    {
        archive.Close();
        return false;
    }

    std::vector<FileEntry> entries = ReadIndex(archive, header);

    auto it = std::find_if(entries.begin(), entries.end(),
        [fileID](const FileEntry& e) { return e.id == fileID && (e.flags & FILE_ACTIVE); });

    if (it == entries.end())
    {
        archive.Close();
        return false;
    }

    strncpy_s(it->filename, MAX_FILENAME_LENGTH, newName.c_str(), _TRUNCATE);

    if (!WriteIndex(archive, entries, header))
    {
        archive.Close();
        return false;
    }

    archive.Close();
    return true;
}

bool Archive::Compact(const std::string& archivePath)
{
    File oldArchive;
    if (!oldArchive.OpenRead(archivePath))
        return false;

    ArchiveHeader header;
    if (!ReadArchiveHeader(oldArchive, header))
    {
        oldArchive.Close();
        return false;
    }

    std::vector<FileEntry> oldEntries = ReadIndex(oldArchive, header);

    std::string tempPath = archivePath + ".tmp";
    File newArchive;
    if (!newArchive.OpenWrite(tempPath))
    {
        oldArchive.Close();
        return false;
    }

    ArchiveHeader newHeader = header;
    newHeader.fileCount = CountActiveFiles(oldEntries);
    newArchive.Write((const UINT8*)&newHeader, sizeof(ArchiveHeader), 1);

    UINT64 currentOffset = sizeof(ArchiveHeader);
    std::vector<FileEntry> newEntries;

    for (const auto& entry : oldEntries)
    {
        if (!(entry.flags & FILE_ACTIVE))
            continue;

        oldArchive.Seek(entry.offset, SEEK_SET);

        Blob blob;
        blob.Reserve(entry.size);
        UINT8 buffer[MAX_BUFFER_SIZE];
        UINT64 remaining = entry.size;

        while (remaining > 0)
        {
            UINT64 chunkSize = (remaining > MAX_BUFFER_SIZE) ? MAX_BUFFER_SIZE : remaining;
            UINT64 bytesRead = oldArchive.Read(buffer, chunkSize, 1);
            if (bytesRead == 0)
                break;
            blob.Append(buffer, bytesRead);
            remaining -= bytesRead;
        }

        newArchive.Seek(currentOffset, SEEK_SET);
        newArchive.Write(blob.GetData(), blob.GetSize(), 1);

        FileEntry newEntry = entry;
        newEntry.offset = currentOffset;
        newEntries.push_back(newEntry);

        currentOffset += entry.size;
    }

    newHeader.fileCount = static_cast<UINT32>(newEntries.size());

    if (!WriteIndex(newArchive, newEntries, newHeader))
    {
        oldArchive.Close();
        newArchive.Close();
        remove(tempPath.c_str());
        return false;
    }

    newArchive.Seek(0, SEEK_SET);
    WriteArchiveHeader(newArchive, newHeader);

    oldArchive.Close();
    newArchive.Close();

    remove(archivePath.c_str());
    rename(tempPath.c_str(), archivePath.c_str());

    return true;
}

bool Archive::FindByID(const std::string& archivePath, UINT64 fileID, FileEntry& outEntry)
{
    File archive;
    if (!archive.OpenRead(archivePath))
        return false;

    ArchiveHeader header;
    if (!ReadArchiveHeader(archive, header))
    {
        archive.Close();
        return false;
    }

    std::vector<FileEntry> entries = ReadIndex(archive, header);

    auto it = std::find_if(entries.begin(), entries.end(),
        [fileID](const FileEntry& e) { return e.id == fileID && (e.flags & FILE_ACTIVE); });

    if (it == entries.end())
    {
        archive.Close();
        return false;
    }

    outEntry = *it;
    archive.Close();
    return true;
}

bool Archive::FindByName(const std::string& archivePath, const std::string& filename, FileEntry& outEntry)
{
    File archive;
    if (!archive.OpenRead(archivePath))
        return false;

    ArchiveHeader header;
    if (!ReadArchiveHeader(archive, header))
    {
        archive.Close();
        return false;
    }

    std::vector<FileEntry> entries = ReadIndex(archive, header);

    auto it = std::find_if(entries.begin(), entries.end(),
        [&filename](const FileEntry& e) {
            return (e.flags & FILE_ACTIVE) && strcmp(e.filename, filename.c_str()) == 0;
        });

    if (it == entries.end())
    {
        archive.Close();
        return false;
    }

    outEntry = *it;
    archive.Close();
    return true;
}
