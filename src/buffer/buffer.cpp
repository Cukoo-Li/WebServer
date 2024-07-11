#include "buffer.h"

Buffer::Buffer(int init_buff_size)
    : buffer_(init_buff_size), read_pos_(0), write_pos_(0) {}

size_t Buffer::ReadableBytes() const {
    return write_pos_ - read_pos_;
}

size_t Buffer::WritableBytes() const {
    return buffer_.size() - write_pos_;
}

size_t Buffer::PrependableBytes() const {
    return read_pos_;
}

const char* Buffer::ReadBegin() const {
    return Begin() + read_pos_;
}

void Buffer::Retrieve(size_t len) {
    // assert(len <= ReadableBytes());
    read_pos_ += len;
}

void Buffer::RetrieveUntil(const char* end) {
    // assert(ReadBegin() <= end);
    Retrieve(end - ReadBegin());
}

void Buffer::RetrieveAll() {
    memset(&buffer_[0], 0, buffer_.size());
    read_pos_ = 0;
    write_pos_ = 0;
}

const char* Buffer::ConstWriteBegin() const {
    return Begin() + write_pos_;
}

char* Buffer::WriteBegin() {
    return Begin() + write_pos_;
}

void Buffer::HasWritten(size_t len) {
    write_pos_ += len;
}

void Buffer::Append(const char* str, size_t len) {
    // assert(str);
    EnsureWritable(len);
    std::copy(str, str + len, WriteBegin());
    HasWritten(len);
}

void Buffer::Append(const std::string& str) {
    Append(str.c_str(), str.length());
}

void Buffer::Append(const Buffer& buff) {
    Append(buff.ReadBegin(), buff.ReadableBytes());
}

void Buffer::EnsureWritable(size_t len) {
    if (WritableBytes() < len) {
        MakeSpace(len);
    }
    // assert(WritableBytes() >= len);
}

// 从 fd 中读取数据，写入到 Buffer 中
ssize_t Buffer::ReadFd(int fd, int* save_errno) {
    // 为了保证有足够的空间存储从 fd 中读取到的数据
    // 设置一个辅助数组，分散读，最后再合并起来
    char buff[2048];  // 辅助数组大小应该根据实际需求调整
    iovec iov[2]{};
    const size_t writable_bytes = WritableBytes();
    iov[0].iov_base = Begin() + write_pos_;
    iov[0].iov_len = writable_bytes;
    iov[1].iov_base = buff;
    iov[1].iov_len = sizeof(buff);

    const ssize_t len = readv(fd, iov, 2);
    if (len < 0) {
        *save_errno = errno;
    } else if (len <= writable_bytes) {
        HasWritten(len);
    } else {
        write_pos_ = buffer_.size();
        Append(buff, len - writable_bytes);
    }
    return len;
}

// 从 Buffer 中读取数据，写入到 fd 中
ssize_t Buffer::WriteFd(int fd, int* save_errno) {
    ssize_t len = write(fd, ReadBegin(), ReadableBytes());
    if (len < 0) {
        *save_errno = errno;
    } else {
        read_pos_ += len;
    }
    return len;
}

char* Buffer::Begin() {
    return &buffer_[0];
}

const char* Buffer::Begin() const {
    return &buffer_[0];
}

void Buffer::MakeSpace(size_t len) {
    if (WritableBytes() + PrependableBytes() < len) {
        buffer_.resize(write_pos_ + len + 1);
    } else {
        size_t readable_bytes = ReadableBytes();
        std::copy(Begin() + read_pos_, Begin() + write_pos_, Begin());
        read_pos_ = 0;
        write_pos_ = read_pos_ + readable_bytes;
        // assert(readable_bytes == ReadableBytes());
    }
}
