#include "httpresponse.h"

const std::unordered_map<std::string, std::string> HttpResponse::kSuffixTypes_ =
    {
        {".html", "text/html"},
        {".xml", "text/xml"},
        {".xhtml", "application/xhtml+xml"},
        {".txt", "text/plain"},
        {".rtf", "application/rtf"},
        {".pdf", "application/pdf"},
        {".word", "application/nsword"},
        {".png", "image/png"},
        {".gif", "image/gif"},
        {".jpg", "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".au", "audio/basic"},
        {".mpeg", "video/mpeg"},
        {".mpg", "video/mpeg"},
        {".avi", "video/x-msvideo"},
        {".gz", "application/x-gzip"},
        {".tar", "application/x-tar"},
        {".css", "text/css "},
        {".js", "text/javascript "},
};

const std::unordered_map<int, std::string> HttpResponse::kStatusCodePhrases_ = {
    {200, "OK"},           // 请求成功
    {400, "Bad Request"},  // 客户发来的请求报文格式不合法
    {403, "Forbidden"},    // 客户对所请求资源没有访问权限
    {404, "Not Found"},    // 客户所请求的资源不存在
};

const std::unordered_map<int, std::string>
    HttpResponse::kErrorStatusCodeHtmlPaths_ = {
        {400, "/400.html"},
        {403, "/403.html"},
        {404, "/404.html"},
};

HttpResponse::HttpResponse() {
    code_ = -1;
    is_keep_alive_ = false;
    file_path_ = work_dir_ = "";
    file_addr_ = nullptr;
    file_stat_ = {};
}

HttpResponse::~HttpResponse() {
    UnmapFile();
}

void HttpResponse::Init(const std::string& work_dir,
                        std::string& file_path,
                        bool is_keep_alive,
                        int code) {
    assert(work_dir != "");
    UnmapFile();
    code_ = code;
    is_keep_alive_ = is_keep_alive;
    file_path_ = file_path;
    work_dir_ = work_dir;
    file_addr_ = nullptr;
    file_stat_ = {};
}

void HttpResponse::MakeResponse(Buffer& buff) {
    // 考察所请求的资源文件的状态
    if (stat((work_dir_ + file_path_).c_str(),&file_stat_) < 0 ||
        S_ISDIR(file_stat_.st_mode)) {
        code_ = 404;  // 不存在，或者是一个目录
    } else if (!(file_stat_.st_mode & S_IROTH)) {
        code_ = 403;  // 不可读
    } else if (code_ == -1) {
        code_ = 200;
    }
    HandleErrorStatusCode();
    AddStartLine(buff);
    AddHeaders(buff);
    AddBody(buff);
}

void* HttpResponse::file_addr() const {
    return file_addr_;
}

size_t HttpResponse::file_size() const {
    return file_stat_.st_size;
}

std::string HttpResponse::file_type() const {
    // 根据文件名后缀推断文件类型
    auto idx = file_path_.find_last_of('.');
    if (idx != std::string::npos) {
        std::string suffix = file_path_.substr(idx);
        if (kSuffixTypes_.count(suffix)) {
            return kSuffixTypes_.at(suffix);
        }
    }
    return "text/plain";
}

void HttpResponse::HandleErrorStatusCode() {
    if (kErrorStatusCodeHtmlPaths_.count(code_)) {
        file_path_ = kErrorStatusCodeHtmlPaths_.at(code_);
        stat((work_dir_ + file_path_).c_str(), &file_stat_);
    }
}

void HttpResponse::AddStartLine(Buffer& buff) {
    std::string phrase;
    if (kStatusCodePhrases_.count(code_)) {
        phrase = kStatusCodePhrases_.at(code_);
    } else {
        code_ = 400;
        phrase = kStatusCodePhrases_.at(code_);
    }
    buff.Append("HTTP/1.1 " + std::to_string(code_) + " " + phrase + "\r\n");
}

void HttpResponse::AddHeaders(Buffer& buff) {
    buff.Append("Connection: ");
    if (is_keep_alive_) {
        buff.Append("keep-alive\r\n");
        buff.Append("keep-alive: max=6, timeout=120\r\n");
    } else {
        buff.Append("close\r\n");
    }
    buff.Append("Content-type: " + file_type() + "\r\n");
}

// 实际上并没有往 buff 中写入 body
// 而是将文件映射到内存中，并将其地址记录到成员变量 file_addr_ 中
// 同时，往 buff 中写入 Content-length 和 一个空行
void HttpResponse::AddBody(Buffer& buff) {
    int fd = open((work_dir_ + file_path_).c_str(), O_RDONLY);
    assert(fd >= 0);
    file_addr_ = mmap(0, file_size(), PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    buff.Append("Content-length: " + std::to_string(file_size()) + "\r\n\r\n");
}

void HttpResponse::UnmapFile() {
    if (!file_addr_) {
        return;
    }
    munmap(file_addr_, file_size());
    file_addr_ = nullptr;
}
