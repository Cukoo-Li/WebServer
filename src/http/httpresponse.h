// Author: Cukoo
// Date: 2024-07-03

#ifndef HTTP_RESPONSE_H
#define HTTP_RESPONSE_H

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <spdlog/spdlog.h>

#include "../buffer/buffer.h"

class HttpResponse {
   public:
    HttpResponse();
    ~HttpResponse();

    void Init(const std::string& work_dir,
              std::string& file_path,
              bool is_keep_alive = false,
              int code = -1);
    void MakeResponse(Buffer& buff);
    void UnmapFile();

    void* file_addr() const;
    size_t file_size() const;
    std::string file_type() const;
    int code() const;

   private:
    void AddStartLine(Buffer& buff);
    void AddHeaders(Buffer& buff);
    void AddBody(Buffer& buff);
    void HandleErrorStatusCode();
    void ErrorContent(Buffer& buff, std::string message);

    int code_;
    bool is_keep_alive_;
    std::string file_path_;  // 文件路径
    std::string work_dir_;  // 工作目录
    void* file_addr_;  // 文件的内存地址
    struct stat file_stat_;     // 文件的状态

    static const std::unordered_map<std::string, std::string>
        kSuffixTypes_;  // 文件名后缀 -> 文件类型
    static const std::unordered_map<int, std::string>
        kStatusCodePhrases_;  // 状态码 -> 短语
    static const std::unordered_map<int, std::string>
        kErrorStatusCodeHtmlPaths_;  // 错误状态码 -> 错误页面路径
};

#endif
