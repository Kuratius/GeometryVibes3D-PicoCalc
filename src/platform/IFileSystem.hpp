#pragma once
#include <cstddef>
#include <cstdint>

namespace gv {

class IFile {
public:
    virtual ~IFile() = default;

    virtual bool read(void* dst, size_t bytes, size_t& outRead) = 0;
    virtual bool write(const void* src, size_t bytes, size_t& outWritten) = 0;
    virtual bool seek(size_t absOffset) = 0;
    virtual size_t tell() const = 0;

    // close() releases all resources for this file.
    // Implementations may self-delete; the pointer is invalid after close().
    virtual void close() = 0;
};

class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    // init() mounts or prepares the underlying storage.
    virtual bool init() = 0;

    // openRead() / openWrite() return distinct file objects.
    // Multiple files may be open concurrently.
    // The returned pointer remains valid until close() is called.
    virtual IFile* openRead(const char* path) = 0;                 // nullptr on fail
    virtual IFile* openWrite(const char* path, bool truncate) = 0; // nullptr on fail

    virtual bool exists(const char* path) const = 0;
};

} // namespace gv