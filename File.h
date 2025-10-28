#pragma once
#include "Stream.h"

enum  Mode
{
    READ,
    WRITE
};

class File : public Stream
{
public:
    File() = default;
    ~File() override;
    
    // bool            Create(const std::string& filename) override {}
    // bool            Delete(const std::string& filename) override {}
    // bool            ConvertData(const std::vector<UINT8>& in, std::vector<UINT8>& out) override {}
    
    bool            Open(const std::string& filename, Mode mode);
    UINT64          Read(UINT8 *buffer, UINT64 size, UINT64 count = 1) override;
    UINT64          Write(const UINT8* buffer, UINT64 size, UINT64 count = 1) override;
    void            Close();
    INT64           Seek(INT32 offset, int origin = SEEK_SET) override;


    bool            OpenRead(const std::string& filename);
    bool            OpenWrite(const std::string& filename);
    bool            IsOpen() const { return mpFile != nullptr; }

private:
    //const char      *mFilename;
    UINT64          ComputeChunkSize(UINT64 remainingBytes) const;
 
};
