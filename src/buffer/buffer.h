#ifndef BUFFER_H
#define BUFFER_H

#include <cstring>
#include <iostream>
#include <unistd.h>
#include <sys/uio.h>
#include <vector>
#include <atomic>
#include <assert.h>

class Buffer {
    public:
    Buffer(int init_buff_size = 1024);
    ~Buffer() = default;

    size_t WritableBytes() const;
    size_t ReadableBytes() const;
    size_t PrependableBytes() const;    // ???

    const char* Peek() const;   // ???
    void EnsureWritable(size_t len);    // ???
    void HasWritten(size_t len);    // ???

    void Retrieve(size_t len);      // ???
    void RetrieveUntil(const char* end);    // ???

    void RetrieveAll();         // ???
    std::string RetrieveAlltoStr();     // ???

    const char* BeginWriteConst() const;    // ???
    char* BeginWrite();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const void* data, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* save_errno);
    ssize_t WriteFd(int fd, int* save_errno);

    private:
    char* BeginPtr();
    const char* BeginPtr() const;
    void MakeSpace(size_t len);

    std::vector<char> buffer_;
    std::atomic<size_t> read_pos_;
    std::atomic<size_t> write_pos_;
};

#endif