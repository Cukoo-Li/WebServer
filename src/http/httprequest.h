#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <errno.h>
#include <mysql/mysql.h>
#include <regex>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnguard.h"
#include "../pool/sqlconnpool.h"

class HttpRequest {
   public:
    enum class ParseState {
        START_LINE,
        HEADERS,
        BODY,
        FINISH
    };

    enum class HttpCode {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURSE,
        FORBIDDENT_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION,
    };

    HttpRequest();
    ~HttpRequest() = default;

    void Init();

    std::string url() const;
    std::string& url();
    std::string method() const;
    std::string version() const;
    std::string GetPostRequestParm(const std::string& key) const;
    std::string GetPostRequestParm(const char* key) const;
    bool IsKeepAlive() const;

    bool Parse(Buffer& buff);

   private:
    bool ParseStartLine(const std::string& line);
    void ParseHeaders(const std::string& line);
    void ParseBody(const std::string& line);

    void ParseUrl();
    void ParsePost();
    void ParseFromUrlencoded();  // ???

    static bool UserVerify(const std::string& name,
                           const std::string& pwd,
                           bool is_login);

    ParseState state_;
    std::string method_;
    std::string url_;
    std::string version_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> post_request_parms_;

    static const std::unordered_set<std::string> kDefaultHtml_;
    static const std::unordered_map<std::string, int> kDefaultHtmlTag_;
    static int ConverHex(char ch);  // ???
};

#endif