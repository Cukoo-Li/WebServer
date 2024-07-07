#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <unordered_set>
#include <unordered_map>
#include <string>
#include <regex>
#include <errno.h>
#include <mysql/mysql.h>

#include "../buffer/buffer.h"
#include "../log/log.h"
#include "../pool/sqlconnpool.h"
#include "../pool/sqlconnguard.h"

class HttpRequest {
   public:
    enum class ParseState { REQUEST_LINE, REQUEST_HEADERS, REQUEST_BODY, FINISH };

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

    std::string path() const;
    std::string& path();
    std::string method() const;
    std::string version() const;
    std::string GetPostRequestParm(const std::string& key) const;
    std::string GetPostRequestParm(const char* key) const;
    bool IsKeepAlive() const;
    
    bool Parse(Buffer& buff);

   private:
    bool ParseRequestLine(const std::string& line);
    void ParseRequestHeader(const std::string& line);
    void ParseRequestBody(const std::string& line);

    void ParsePath();
    void ParsePost();
    void ParseFromUrlencoded();  // ???

    static bool UserVerify(const std::string& name,
                           const std::string& pwd,
                           bool is_login);

    ParseState state_;
    std::string method_;
    std::string path_;
    std::string version_;
    std::string body_;
    std::unordered_map<std::string, std::string> header_;       // ???
    std::unordered_map<std::string, std::string> post_;         // ???

    static const std::unordered_set<std::string> kDefaultHtml_;
    static const std::unordered_map<std::string, int> kDefaultHtmlTag_;
    static int ConverHex(char ch);  // ???
};

#endif