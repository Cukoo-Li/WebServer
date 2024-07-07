#include "httprequest.h"

const std::unordered_set<std::string> HttpRequest::kDefaultHtml_{
    "/index", "/register", "/login", "/welcome", "/video", "/picture",
};

const std::unordered_map<std::string, int> HttpRequest::kDefaultHtmlTag_{
    {"/register.html", 0},
    {"/login.html", 1},
};

HttpRequest::HttpRequest() {
    Init();
}

void HttpRequest::Init() {
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

std::string HttpRequest::GetPostRequestParm(const std::string& key) const {
    assert(key != "");
    if (post_request_parms_.count(key) == 1) {
        return post_request_parms_.at(key);
    }
    return "";
}

std::string HttpRequest::GetPostRequestParm(const char *key) const {
    assert(key);
    return GetPostRequestParm(std::string(key));
}

bool HttpRequest::IsKeepAlive() const {
    if (headers_.count("Connection") == 1) {
        return headers_.at("Connection") == "keep-alive" && version_ == "1.1";
    }
    return false;
}

// 解析请求报文（重点理解）
bool HttpRequest::Parse(Buffer& buff) {
    const char CRLF[] = "\r\n";
    if (buff.ReadableBytes() == 0) {
        return false;
    }
    while (buff.ReadableBytes() && state_ != ParseState::FINISH) {
        // 如果找不到 CRLF，line_end 就等于 buff.WriteBegin()
        const char* line_end = std::search(
            buff.ReadBegin(), buff.ConstWriteBegin(), CRLF, CRLF + 2);
        std::string line(buff.ReadBegin(), line_end);
        switch (state_) {
            case ParseState::START_LINE:
                if (!ParseStartLine(line)) {
                    return false;
                }
                ParseUrl();
                break;
            case ParseState::HEADERS:
                ParseHeaders(line);
                // 这个条件是否有问题
                if (buff.ReadableBytes() <= 2) {
                    state_ = ParseState::FINISH;
                }
                break;
            case ParseState::BODY:
                ParseBody(line);
                break;
            default:
                break;
        }
        // 不是完整的一行，不更新 buff.ReadBegin()，下次再处理
        if (line_end == buff.WriteBegin()) {
            break;
        }

        buff.RetrieveUntil(line_end + 2);
    }
    // LOG_DEBUG("[%s], [%s], [%s]", method_.c_str(), url_.c_str(),
    // version_.c_str());
    return true;
}

void HttpRequest::ParseUrl() {
    if (url_ == "/") {
        url_ = "/index.html";
    } else {
        for (auto& item : kDefaultHtml_) {
            if (item == url_) {
                url_ += ".html";
                break;
            }
        }
    }
}

// 重点理解
bool HttpRequest::ParseStartLine(const std::string& line) {
    std::regex pattern("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");
    std::smatch sub_match;
    if (std::regex_match(line, sub_match, pattern)) {
        method_ = sub_match[1];
        url_ = sub_match[2];
        version_ = sub_match[3];
        state_ = ParseState::HEADERS;
        return true;
    }
    // LOG_ERROR("RequestLine Error");
    return false;
}

// 重点理解
void HttpRequest::ParseHeaders(const std::string& line) {
    std::regex pattern("^([^:]*): ?(.*)$");
    std::smatch sub_match;
    if (std::regex_match(line, sub_match, pattern)) {
        headers_[sub_match[1]] = sub_match[2];
    } else {
        state_ = ParseState::BODY;
    }
}

void HttpRequest::ParseBody(const std::string& line) {
    body_ = line;
    ParsePost();
    state_ = ParseState::FINISH;
    // LOG_DEBUG("Body:%s, len:%d", line.c_str(), line.size());
}

// ???
int HttpRequest::ConverHex(char ch) {
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    return ch;
}

void HttpRequest::ParsePost() {
    if (method_ == "Post" &&
        headers_["Content-Type"] == "application/x-www-form-urlencoded") {
        ParseFromUrlencoded();
        if (kDefaultHtmlTag_.count(url_)) {
            int tag = kDefaultHtmlTag_.at(url_);
            // LOG_DEBUG("Tag:%d", tag);
            if (tag == 0 || tag == 1) {
                bool is_login = (tag == 1);
                if (UserVerify(post_request_parms_["username"], post_request_parms_["password"],
                               is_login)) {
                    url_ = "/welcome.html";
                } else {
                    url_ = "/error.html";
                }
            }
        }
    }
}

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
                num = ConverHex(body_[i + 1]) * 16 + ConverHex(body_[i + 1]);
                body_[i + 2] = num % 10 + '0';
                body_[i + 1] = num / 10 + '0';
                i += 2;
                break;
            case '&':
                value = body_.substr(j, i - j);
                j = i + 1;
                post_request_parms_[key] = value;
                // LOG_DEBUG("%s = %s", key.c_str(), value.c_str());
                break;
            default:
                break;
        }
    }
    assert(j <= i);
    if (post_request_parms_.count(key) == 0 && j < i) {
        value = body_.substr(j, i - j);
        post_request_parms_[key] = value;
    }
}

// 重点理解
bool HttpRequest::UserVerify(const std::string& name,
                             const std::string& pwd,
                             bool is_login) {
    if (name == "" || pwd == "") {
        return false;
    }
    // LOG_INFO("Verify name:%s pwd:%s", name.c_str(), pwd.c_str());
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
    // LOG_DEBUG("%s", order);

    if (mysql_query(sql, order) != 0) {
        mysql_free_result(res);
        return false;
    }

    res = mysql_store_result(sql);
    j = mysql_num_fields(res);
    fields = mysql_fetch_fields(res);

    // 为什么要用 while？不就一行记录吗？
    while (MYSQL_ROW row = mysql_fetch_row(res)) {
        // LOG_DEBUG("MYSQL ROW: %s %s", row[0], row[1]);
        std::string password(row[1]);
        
        // 登录 - 验证密码
        if (is_login) {
            if (pwd == password) {
                flag = true;
            } else {
                flag = false;
                // LOG_DEBUG("pwd error!");
            }
        } 
        // 注册 - 用户名已存在
        else {
            flag = false;
            // LOG_DEBUG("user used!");
        }
    }
    mysql_free_result(res);

    // 注册行为 且 用户名未被使用
    if (!is_login && flag == true) {
        // LOG_DEBUG("regirster!");
        memset(order, 0, sizeof(order));
        snprintf(order, 256,"INSERT INTO user(username, password) VALUES('%s','%s')", name.c_str(), pwd.c_str());
        // LOG_DEBUG( "%s", order);
        if (mysql_query(sql, order) != 0) {
            // LOG_DEBUG( "Insert error!");
            flag = false;
        }
        flag = true;
    }
    // LOG_DEBUG( "UserVerify success!!");
    return flag;
}