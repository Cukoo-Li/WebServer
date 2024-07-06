#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>

#include "../buffer/buffer.h"
#include "../log/log.h"

class HttpResponse {
   public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& src_dir,
              std::string& path,
              bool is_keep_alive = false,
              int code = -1);
    void MakeResponse(Buffer& buff);
    void UnmapFile();
    void ErrorContent(Buffer& buff, std::string message);

    uint8_t* file_addr() const;
    size_t file_size() const;
    int code() const;

   private:
    void AddStateLine(Buffer& buff);
    void AddHeader(Buffer& buff);
    void AddContent(Buffer& buff);

    void ErrorHtml();  // ???

    int code_;
    bool is_keep_alive_;
    std::string path_;  // ???
    std::string src_dir_;
    uint8_t* file_addr_;  // 文件的内存地址
    // struct stat file_stat_;
    // file_size file_type 作为属性，在初始化函数里初始化吧。
    size_t file_size_;
    std::string file_type_;

    static const std::unordered_map<std::string, std::string>
        kSuffixType_;                                                // ???
    static const std::unordered_map<int, std::string> kCodeStatus_;  // ???
    static const std::unordered_map<int, std::string> kCodePath_;    // ???
};

#endif