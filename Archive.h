#ifndef ARCHIVE_H__
#define ARCHIVE_H__

#include <string>
#include <vector>
#include <ctime>

#define MAX_FILENAME_LENGTH 256

enum FileFlags : UINT8
{
    FILE_ACTIVE     = 0x01,
    FILE_DELETED    = 0x02,
    FILE_COMPRESSED = 0x04,
    FILE_ENCRYPTED  = 0x08
};

struct ArchiveHeader
{
    char     magic[4];
    UINT32   version;
    UINT32   fileCount;
    UINT64   indexOffset;
    UINT64   dataOffset;
    UINT32   archiveCRC;
};

struct FileEntry
{
    UINT64   id;
    char     filename[MAX_FILENAME_LENGTH];
    UINT64   offset;
    UINT64   size;
    UINT8    flags;
    UINT8    padding[7];
};

class Archive
{
public:
    static bool Create(const std::string& archivePath, const std::vector<std::string>& filePaths);
    static bool List(const std::string& archivePath);
    static bool Extract(const std::string& archivePath, UINT64 fileID, const std::string& outputPath);
    static bool ExtractByName(const std::string& archivePath, const std::string& filename, const std::string& outputPath);
    static bool ExtractAll(const std::string& archivePath, const std::string& outputDir);
    static bool Validate(const std::string& archivePath);

    static bool AddFile(const std::string& archivePath, const std::string& filePath, UINT64& outFileID);
    static bool RemoveFile(const std::string& archivePath, UINT64 fileID);
    static bool RenameFile(const std::string& archivePath, UINT64 fileID, const std::string& newName);
    static bool Compact(const std::string& archivePath);

    static bool FindByID(const std::string& archivePath, UINT64 fileID, FileEntry& outEntry);
    static bool FindByName(const std::string& archivePath, const std::string& filename, FileEntry& outEntry);

private:
    static UINT64 GenerateFileID(const std::string& filename);
    static UINT32 CountActiveFiles(const std::vector<FileEntry>& entries);
    static FileEntry* FindDeletedEntry(std::vector<FileEntry>& entries, UINT64 minSize);
    static std::vector<FileEntry> ReadIndex(File& archive, const ArchiveHeader& header);
    static bool WriteIndex(File& archive, const std::vector<FileEntry>& entries, ArchiveHeader& header);
    static bool WriteFileWithHeader(File& archive, const std::string& filePath, UINT64& outID, UINT64& outSize);
    static bool ReadArchiveHeader(File& archive, ArchiveHeader& header);
    static bool WriteArchiveHeader(File& archive, const ArchiveHeader& header);
    static std::string GetBasename(const std::string& path);
};

#endif // !ARCHIVE_H__
