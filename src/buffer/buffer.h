#ifndef BUFFER_H
#define BUFFER_H

#include <assert.h>
#include <sys/uio.h>
#include <unistd.h>
#include <atomic>
#include <cstring>
#include <iostream>
#include <vector>

class Buffer {
   public:
    Buffer(int init_buff_size = 1024);
    ~Buffer() = default;

    size_t PrependableBytes() const;
    size_t ReadableBytes() const;
    size_t WritableBytes() const;

    const char* ReadBegin() const;
    const char* ConstWriteBegin() const;

    void Retrieve(size_t len);
    void RetrieveUntil(const char* end);
    void RetrieveAll();

    void Append(const std::string& str);
    void Append(const char* str, size_t len);
    void Append(const Buffer& buff);

    ssize_t ReadFd(int fd, int* save_errno);
    ssize_t WriteFd(int fd, int* save_errno);

   private:
    char* Begin();
    const char* Begin() const;
    char* WriteBegin();
    void EnsureWritable(size_t len);
    void MakeSpace(size_t len);
    void HasWritten(size_t len);

    std::vector<char> buffer_;
    std::atomic<size_t> read_pos_;
    std::atomic<size_t> write_pos_;
};

#endif
