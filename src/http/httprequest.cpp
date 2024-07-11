#include "httprequest.h"

// 保存默认页面名的静态变量，所有对以下 url 的请求都会加上 .html 后缀
const std::unordered_set<std::string> HttpRequest::kDefaultHtml_{
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

// 区分登录和注册的标志
const std::unordered_map<std::string, int> HttpRequest::kDefaultHtmlTag_{
    {"/register.html", 0},
    {"/login.html", 1},
};

// 构造时请求信息默认初始化为空
HttpRequest::HttpRequest() {
    Init();
}

// 将各请求信息重新初始化为空
void HttpRequest::Init() {
    state_ = ParseState::START_LINE;
    method_ = url_ = version_ = body_ = "";
    headers_.clear();
    post_request_parms_.clear();
}

std::string HttpRequest::url() const {
    return url_;
}

std::string& HttpRequest::url() {
    return url_;
}

std::string HttpRequest::method() const {
    return method_;
}

std::string HttpRequest::version() const {
    return version_;
}

HttpRequest::ParseState HttpRequest::state() const {
    return state_;
}

bool HttpRequest::IsKeepAlive() const {
    if (headers_.count("Connection") == 1) {
        return headers_.at("Connection") == "keep-alive" && version_ == "1.1";
    }
    return false;
}

// 按关键字获取指定 Post 请求参数
std::string HttpRequest::GetPostRequestParm(const std::string& key) const {
    assert(key != "");
    if (post_request_parms_.count(key) == 1) {
        return post_request_parms_.at(key);
    }
    return "";
}

// 解析请求报文（重点理解）
HttpRequest::HttpCode HttpRequest::Parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    // 如果读缓冲区中没有内容可以被读取，返回 NO_REQUEST，需要重置 EPOLLIN
    // 等下次再读
    if (buff.ReadableBytes() == 0) {
        return HttpCode::NO_REQUEST;
    }

    // 状态机解析请求报文
    while (buff.ReadableBytes() && state_ != ParseState::FINISH) {
        // 先尝试从读缓冲区中提取出一行
        const char* line_end = std::search(
            buff.ReadBegin(), buff.ConstWriteBegin(), CRLF, CRLF + 2);
        // 如果找不到 CRLF，返回尾后指针，此时 line 并不是一个有效的行，返回
        // NO_REQUEST，需要重置 EPOLLIN 等下次再读
        if (line_end == buff.ConstWriteBegin() && state_ != ParseState::BODY) {
            return HttpCode::NO_REQUEST;
        }
        std::string line(buff.ReadBegin(), line_end);
        // 根据当前状态决定解析方式
        switch (state_) {
            // 解析请求行
            case ParseState::START_LINE:
                if (!ParseStartLine(line)) {
                    return HttpCode::BAD_REQUEST;
                }
                ParseUrl();  // 将默认 url 补充完整
                break;
            // 解析请求头
            case ParseState::HEADERS:
                if (!ParseHeaders(line)) {
                    return HttpCode::BAD_REQUEST;
                }
                // ParseHeaders 在遇到空行时会将 state_ 置为 BODY
                // 如果是 GET 请求，就解析完毕了
                if (state_ == ParseState::BODY && method_ == "GET") {
                    state_ = ParseState::FINISH;
                    buff.RetrieveAll();
                    return HttpCode::GET_REQUEST;
                }
                break;
            // 解析请求体
            case ParseState::BODY:
                if (!ParseBody(line)) {
                    return HttpCode::NO_REQUEST;
                }
                buff.RetrieveAll();
                return HttpCode::GET_REQUEST;
                break;
            default:
                return HttpCode::INTERNAL_ERROR;
        }
        buff.RetrieveUntil(line_end + 2);
    }
    spdlog::debug("[{}], [{}], [{}]", method_, url_, version_);
    return HttpCode::NO_REQUEST;
}

// 将默认 url 补充完整
void HttpRequest::ParseUrl() {
    if (url_ == "/") {
        url_ = "/index.html";
        return;
    }
    if (kDefaultHtml_.count(url_)) {
        url_ += ".html";
        return;
    }
}

// 解析请求行
bool HttpRequest::ParseStartLine(const std::string& line) {
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch sub_match;
    if (std::regex_match(line, sub_match, pattern)) {
        method_ = sub_match[1];
        url_ = sub_match[2];
        version_ = sub_match[3];
        state_ = ParseState::HEADERS;  // 切换状态
        return true;
    }
    spdlog::error("StartLine Error! {}", line);
    return false;
}

// 解析请求头
bool HttpRequest::ParseHeaders(const std::string& line) {
    std::regex pattern("^([^:]*): ?(.*)$");
    std::smatch sub_match;
    if (std::regex_match(line, sub_match, pattern)) {
        headers_[sub_match[1]] = sub_match[2];
    } else if (line == "") {
        state_ = ParseState::BODY;
    } else {
        spdlog::error("Headers Error! {}", line);
        return false;
    }
    return true;
}

// 解析请求体
bool HttpRequest::ParseBody(const std::string& line) {
    if (line.size() < stoi(headers_.at("Content-Length"))) {
        return false;
    }
    body_ = line;
    ParsePost();
    state_ = ParseState::FINISH;
    spdlog::debug("Body:{}, len:{}", line, line.size());
    return true;
}

// 解析 Post 请求
void HttpRequest::ParsePost() {
    if (method_ == "POST" &&
        headers_["Content-Type"] == "application/x-www-form-urlencoded") {
        // 将请求体解析成 Post 请求参数
        ParseFromUrlencoded();
        // 处理登录注册请求
        if (kDefaultHtmlTag_.count(url_)) {
            int tag = kDefaultHtmlTag_.at(url_);
            spdlog::debug("Tag:{}", tag);
            if (tag == 0 || tag == 1) {
                bool is_login = (tag == 1);
                if (UserVerify(post_request_parms_["username"],
                               post_request_parms_["password"], is_login)) {
                    url_ = "/welcome.html";
                } else {
                    url_ = "/error.html";
                }
            }
        }
    }
}

// 将 16 进制数转为 10 进制数
int HttpRequest::ConvertHexToDecimal(char ch) {
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return ch;
}

// 从 ContentType = application/x-www-form-urlencoded 的请求体中解析请求参数
void HttpRequest::ParseFromUrlencoded() {
    if (body_.empty()) {
        return;
    }

    std::string key, value;
    int num = 0;
    int n = body_.size();
    int i = 0, j = 0;

    for (; i < n; ++i) {
        char ch = body_[i];
        switch (ch) {
            case '=':
                key = body_.substr(j, i - j);
                j = i + 1;
                break;
            case '+':
                body_[i] = ' ';
                break;
            case '%':
                num = ConvertHexToDecimal(body_[i + 1]) * 16 +
                      ConvertHexToDecimal(body_[i + 2]);
                body_[i + 2] = num % 10 + '0';
                body_[i + 1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                value = body_.substr(j, i - j);
                j = i + 1;
                post_request_parms_[key] = value;
                spdlog::debug("{} = {}", key, value);
                break;
            default:
                break;
        }
    }
    // 获取最后一个请求参数
    if (post_request_parms_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_request_parms_[key] = value;
    }
}

// 用户注册登录验证*****
bool HttpRequest::UserVerify(const std::string& name,
                             const std::string& pwd,
                             bool is_login) {
    if (name == "" || pwd == "") {
        return false;
    }
    spdlog::debug("Verify name: {},  pwd: {}", name, pwd);
    MYSQL* sql;
    SqlConnGuard give_me_a_name(&sql, SqlConnPool::Instance());
    assert(sql);

    bool flag = false;
    unsigned int j = 0;
    char order[256]{};
    MYSQL_FIELD* fields{};
    MYSQL_RES* res{};

    if (!is_login) {
        flag = true;
    }
    // 查询用户及密码
    snprintf(order, 256,
             "SELECT username, password FROM user WHERE username='%s' LIMIT 1",
             name.c_str());
    spdlog::debug("sql: {}", order);

    if (mysql_query(sql, order) != 0) {
        mysql_free_result(res);
        return false;
    }

    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    // 为什么要用 while？不就一行记录吗？
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        spdlog::debug("MYSQL ROW: {}, {}", row[0], row[1]);
        std::string password(row[1]);

        // 登录 - 验证密码
        if (is_login) {
            if (pwd == password) {
                flag = true;
            } else {
                flag = false;
                spdlog::debug("pwd error!");
            }
        }
        // 注册 - 用户名已存在
        else {
            flag = false;
            spdlog::debug("user used!");
        }
    }
    mysql_free_result(res);

    // 注册行为 且 用户名未被使用
    if (!is_login && flag == true) {
        spdlog::debug("register!");
        memset(order, 0, sizeof(order));
        snprintf(order, 256,
                 "INSERT INTO user(username, password) VALUES('%s','%s')",
                 name.c_str(), pwd.c_str());
        spdlog::debug("sql: {}", order);
        if (mysql_query(sql, order) != 0) {
            spdlog::debug("Insert error!");
            flag = false;
        }
        flag = true;
    }
    spdlog::debug("UserVerify success!!");
    return flag;
}
