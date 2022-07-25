

#include "httpconnection.h"
#include "log.h"

// 定义 HTTP 响应的一些状态信息
const std::string ok_200_title = "OK";
const std::string error_400_title = "Bad Request";
const std::string error_400_form = "Your request has bad syntax or is inherently impossible to satisfy.\n";
const std::string error_403_title = "Forbidden";
const std::string error_403_form = "You don't have permission to get file from this server.\n";
const std::string error_404_title = "Not Found";
const std::string error_404_form = "The requested file was not found on this server.\n";
const std::string error_500_title = "Internal Error";
const std::string error_500_form = "There was an unusual problem serving the requested file.\n";

// 网站的根目录
const std::string doc_root = "resources";

int HTTPConnection::m_epoll_fd = -1;
int HTTPConnection::m_user_count = 0;

// 设置文件描述符 fd 非阻塞
void setnonblockint(int fd)
{
    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}

// 添加文件描述符到 epoll 中
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLRDHUP | EPOLLET; // ET 触发
    if (one_shot)
    {
        event.events |= EPOLLONESHOT; // 防止同一个通信被不同的线程处理
    }
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    // 设置文件描述符非阻塞
    setnonblockint(fd);
}

// 从 epoll 中移除监听的文件描述符
void removefd(int epollfd, int fd, SSL *ssl)
{
    SSL_free(ssl);
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
}

// 修改文件描述符，重置 socket 上的 EPOLLONESHOT 事件，
// 以确保下一次可读时，EPOLLIN 事件能被触发
void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLONESHOT | EPOLLET | EPOLLRDHUP;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 返回带错误消息的默认界面
std::string index_cgi(std::string str)
{
    std::string ret = "<!DOCTYPE html>"
                      "<html lang=\"en\">"
                      "<head>"
                      "<meta charset=\"UTF-8\">"
                      "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">"
                      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
                      "<title>Home page</title>"
                      "<script language=\"javascript\">"
                      "function inputLength() {"
                      "var username = document.login_form.username.value;"
                      "var passwd = document.login_form.passwd.value;"
                      "if (username.length == 0 || passwd.length == 0) {"
                      "alert(\"Please enter your user name and password!\");"
                      "return false;"
                      "}"
                      "return true;"
                      "}"
                      "</script>"
                      "</head>"
                      "<body>"
                      "<h1>Welcome to toto's HTTP server~~~</h1>"
                      "<form method=\"post\" action=\"login.action\" name=\"login_form\" onsubmit=\"return inputLength()\">"
                      "<fieldset>"
                      "<legend>&nbsp;Login/Register: &nbsp;</legend> <br>"
                      "Username: <input type=\"text\" name=\"username\" id=\"username\" style=\"width: 200;height:20px;\">"
                      "<br><br>"
                      "Password: <input type=\"password\" name=\"passwd\" id=\"passwd\" style=\"width: 200;height:20px;\">"
                      "<br><br>"
                      "<input type=\"radio\" checked=\"checked\" name=\"type\" value=\"login\">login"
                      "<input type=\"radio\" name=\"type\" value=\"register\">register"
                      "<br><br>"
                      "<input type=\"submit\" style=\"width: 80px;\" />"
                      "&nbsp; <font color=\"red\">";
    ret += str + "</font>"
                 "</fieldset>"
                 "</form>"
                 "</body>"
                 "</html>";
    return ret;
}

// 返回显示用户界面的文件
std::string userinfo_cgi(Database::key_type user, Database::values_array vs)
{
    std::string ret = "<!DOCTYPE html>\n"
                      "<html lang=\"en\">\n"
                      "<head>\n"
                      "<meta charset=\"UTF-8\">\n"
                      "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\n"
                      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
                      "<title>User page</title>\n"
                      "</head>\n"
                      "<body>\n"
                      "<h1>Welcome to toto\'s HTTP server~~~</h1>\n"
                      "<fieldset>\n"
                      "<legend>&nbsp; Personal page: &nbsp;</legend><img src=\"";
    ret += vs[PORTRAIT] + "\" alt=\"Default portrait\"\n"
                          "style=\"float:right;border-radius:100%\" width=\"100\" height=\"100\"> <br>\n"
                          "Username: " +
           user + " <br><br>\n"
                  "Signature: " +
           vs[SIGNATURE] + " <br><br>\n"
                           "<form method=\"post\" action=\"quit.action\"\n"
                           "onsubmit=\"return confirm(\'Are you sure want to quit the current account?\');\">\n"
                           "<input type=\"submit\" value=\"quit account\" style=\"width: 150px;\" />\n"
                           "</form>\n"
                           "&nbsp;\n"
                           "<form method=\"post\" action=\"cancel.action\"\n"
                           "onsubmit=\"return confirm(\'Are you sure want to cancel the current account?\');\">\n"
                           "<input type=\"submit\" value=\"cancel account\" style=\"width: 150px;\" />\n"
                           "</form>\n"
                           "&nbsp;\n"
                           "<form method=\"post\" action=\"update.action\">\n"
                           "<input type=\"submit\" value=\"update info\" style=\"width: 150px;\" />\n"
                           "</form>\n"
                           "</fieldset>\n"
                           "</body>\n"
                           "</html>";
    return ret;
}

// 返回上传信息界面的文件
std::string upload_cgi(Database::key_type user, Database::values_array vs)
{
    std::string ret = "<!DOCTYPE html>\n"
                      "<html lang=\"en\">"
                      "<head>\n"
                      "<meta charset=\"UTF-8\">\n"
                      "<meta http-equiv=\"X-UA-Compatible\" content=\"IE=edge\">\n"
                      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
                      "<script type=\"text/javascript\">\n"
                      "function previewFile() {\n"
                      "var preview = document.querySelector(\'img\');\n"
                      "var file = document.querySelector(\'input[type=file]\').files[0];\n"
                      "var reader = new FileReader();\n"
                      "reader.onloadend = function () {\n"
                      "preview.src = reader.result;\n"
                      "}\n"
                      "if (file) {\n"
                      "reader.readAsDataURL(file);\n"
                      "} else {\n"
                      "preview.src = \"\";\n"
                      "}\n"
                      "}\n"
                      "</script>\n"
                      "<title>Upload page</title>\n"
                      "</head>\n"
                      "<body>\n"
                      "<h1>Welcome to toto\'s HTTP server~~~</h1>\n"
                      "<form method=\"post\" action=\"upload.action\" enctype=\"multipart/form-data\">\n"
                      "<fieldset>\n"
                      "<legend>&nbsp; Modify personal information: &nbsp;</legend> <img src=\"";
    ret += vs[PORTRAIT] + "\" height=\"100\""
                          "width=\"100\" style=\"float:right;border-radius:100%\" alt=\"Image preview...\" /> <br>\n"
                          "Username: ";
    ret += user + " <br><br>\n"
                  "Portrait: <input type=\"file\" name=\"portrait\" accpect=\"image/*\" onchange=\"previewFile()\" /><br><br>\n"
                  "Password: <input type=\"password\" name=\"passwd\" value=\"";
    ret += vs[PASSWD] + "\" style=\"width:200;height:25px;\"><br><br>\n"
                        "Signature: <input type=\"text\" name=\"signature\" value=\"";
    ret += vs[SIGNATURE] + "\" maxlength=\"100\""
                           "style=\"width:500px;height:25px;\"><br><br>\n"
                           "<input type=\"submit\" style=\"width: 100px;\" />\n"
                           "</fieldset>\n"
                           "</form>\n"
                           "</body>\n"
                           "</html>";
    return ret;
}

// 初始化新的连接
void HTTPConnection::init(int sock_fd, const sockaddr_in &addr, SSL *ssl)
{
    m_sock_fd = sock_fd;
    m_address = addr;
    m_ssl = ssl;
    m_user.clear();
    // 端口复用
    // int reuse = 1;
    // setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // 添加到 epoll 对象中
    addfd(m_epoll_fd, sock_fd, true);
    m_user_count++;
    init();
}

void HTTPConnection::init()
{
    m_parse_state = PARSE_STATE_REQUESTLINE; // 初始化为解析请求首行
    m_linger = false;                        // 默认不保持连接
    m_method = GET;                          // 默认请求方法为 GET
    m_read_buf.clear();
    m_pos = 0;
    m_line = 0;
    m_read_size = 0;
    m_line_status = LINE_OK;
    m_file_path.clear();
    m_url.clear();
    m_protocol.clear();
    m_version = HTTP_1_1;
    m_parameters.clear();
    m_headers.clear();
    m_write_buf.clear();
    m_file_buf.clear();
    m_content_length = 0;
    m_linger = false;
    m_timer.reset();
}

// 关闭连接
void HTTPConnection::close_conn()
{
    LOG_DEBUG << "close http conn." << Log::endl;
    if (m_sock_fd != -1)
    {
        removefd(m_epoll_fd, m_sock_fd, m_ssl);
        m_sock_fd = -1;
        m_ssl = NULL;
        m_user.clear();
        m_user_count--; // 连接的客户端数量减一
        if(m_timer.get()) {
            m_timer->setDeleted();
            m_timer.reset();
        }
    }
}

// 循环读取客户端数据，直到无数据可读或客户端断开连接
bool HTTPConnection::read()
{
    int read_bytes = 0; // 读取到的字节数
    while (true)
    {
        char buf[READ_BUFFER_SIZE];
        read_bytes = SSL_read(m_ssl, buf, READ_BUFFER_SIZE);
        // read_bytes = recv(m_sockfd, m_read_buf + m_read_index, READ_BUFFER_SIZE - m_read_index, 0);
        if (read_bytes == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            { // 没有数据
                break;
            }
            else if (errno == EINTR)
            {
                continue;
            }
            return false;
        }
        else if (read_bytes == 0)
        { // 客户端关闭连接
            return false;
        }
        m_read_buf += std::string(buf, buf + read_bytes);
    }
    m_read_size = m_read_buf.size();
    // printf("\n%s", m_read_buf.c_str());
    return true;
}

PARSE_RESULT HTTPConnection::parseRequest()
{
    m_line_status = LINE_OK;
    m_pos = m_line;
    while (m_parse_state != PARSE_DONE)
    {
        switch (m_parse_state)
        {
        case PARSE_STATE_REQUESTLINE:
            m_line_status = parseRequestLine();
            if (m_line_status == LINE_OK)
            {
                m_parse_state = PARSE_STATE_HEADER;
            }
            else if (m_line_status == LINE_BAD)
            {
                return BAD_REQUEST;
            }
            else if (m_line_status == LINE_OPEN)
            {
                return NO_REQUEST;
            }
            break;
        case PARSE_STATE_HEADER:
            m_line_status = parseHeaders();
            if (m_line_status == LINE_OK)
            {
                m_parse_state = PARSE_STATE_CONTENT;
            }
            else if (m_line_status == LINE_BAD)
            {
                return BAD_REQUEST;
            }
            else if (m_line_status == LINE_OPEN)
            {
                return NO_REQUEST;
            }
            break;
        case PARSE_STATE_CONTENT:
            if (m_method == GET)
            {
                parseURL();
            }
            else if (m_method == POST)
            {
                m_line_status = parseContent();
                if (m_line_status == LINE_OPEN)
                {
                    return NO_REQUEST;
                }
                else if (m_line_status == LINE_BAD)
                {
                    return BAD_REQUEST;
                }
            }
            else
            {
                return INTERNAL_ERROR;
            }
            m_pos = m_read_size;
            m_parse_state = PARSE_DONE;
            break;
        default:
            return INTERNAL_ERROR;
        }
    }
    return doRequest();
}

LINE_STATUS HTTPConnection::parseRequestLine()
{
    // "GET /index.html HTTP/1.1\r\n"
    auto enter_pos = m_read_buf.find("\r\n", m_pos);
    if (enter_pos == m_read_buf.npos)
    {
        return LINE_OPEN;
    }
    auto space_pos = m_read_buf.find(' ', m_pos); // first space
    if (space_pos == m_read_buf.npos)
    {
        return LINE_BAD;
    }
    if (!readMethod(space_pos))
    {
        return LINE_BAD;
    }
    m_pos = space_pos + 1;
    space_pos = m_read_buf.find(' ', m_pos); // second space
    if (space_pos == m_read_buf.npos)
    {
        return LINE_BAD;
    }
    if (!readURL(space_pos))
    {
        return LINE_BAD;
    }
    // printf("%s\n", m_url.c_str());
    m_pos = space_pos + 1;
    if (!readProtocol(enter_pos))
    {
        return LINE_BAD;
    }
    // printf("%s\n", m_protocol.c_str());
    m_pos = enter_pos + 2;
    m_line = m_pos;
    return LINE_OK;
}

LINE_STATUS HTTPConnection::parseHeaders()
{
    while (true)
    {
        auto enter_pos = m_read_buf.find("\r\n", m_pos);
        if (enter_pos == m_read_buf.npos)
        {
            return LINE_OPEN;
        }
        else if (int(enter_pos) == m_pos)
        {
            break;
        }
        if (!readHeader(enter_pos))
        {
            return LINE_BAD;
        }
        m_pos = enter_pos + 2;
        m_line = m_pos;
    }
    m_pos += 2;
    m_line = m_pos;
    return LINE_OK;
}

// POST 请求分情况解析内容
//      login.action  : 登录操作
//      quit.action   : 退出操作
//      cancel.action : 注销操作
//      update.action : 更新操作
//      upload.action : 上传操作
LINE_STATUS HTTPConnection::parseContent()
{
    if (m_read_size - m_pos < atoi(m_headers["Content-Length"].c_str()))
    {
        return LINE_OPEN;
    }
    // 从 url 中获取对应的 action
    auto slash_pos = m_url.find_last_of('/');
    if (slash_pos == m_url.npos)
    {
        return LINE_BAD;
    }
    auto action = m_url.substr(slash_pos + 1);
    if (action == "login.action")
    {
        parseParameters(m_read_buf.substr(m_pos, m_read_size - m_pos));
        if (m_parameters.count("type") == 0)
        {
            return LINE_BAD;
        }
        m_action = m_parameters["type"] == "login" ? LOGIN : REGISTER;
    }
    else if (action == "quit.action")
    {
        m_action = QUIT;
    }
    else if (action == "cancel.action")
    {
        m_action = CANCEL;
    }
    else if (action == "update.action")
    {
        m_action = UPDATE;
    }
    else if (action == "upload.action")
    {
        m_action = UPLOAD;
        // 解析多部分的内容
        if (m_headers.count("Content-Type") == 0)
        {
            return LINE_BAD;
        }
        auto content_type = m_headers["Content-Type"];
        auto equal_pos = content_type.find('=');
        auto boundary = "--" + content_type.substr(equal_pos + 1);
        auto boundary_end = boundary + "--";
        auto enter_pos = m_read_buf.find("\r\n", m_pos);
        while (enter_pos != m_read_buf.npos && m_read_buf.substr(m_pos, enter_pos - m_pos) == boundary)
        {
            m_pos = enter_pos + 2;
            auto name_pos = m_read_buf.find("name=\"", m_pos) + 6;
            auto quote_pos = m_read_buf.find('\"', name_pos);
            auto name = m_read_buf.substr(name_pos, quote_pos - name_pos);
            if (name == "portrait")
            {
                auto filename_pos = m_read_buf.find("filename=\"", quote_pos) + 10;
                auto quote_pos = m_read_buf.find('\"', filename_pos);
                auto filename = m_read_buf.substr(filename_pos, quote_pos - filename_pos);
                m_parameters["filename"] = filename;
            }
            enter_pos = m_read_buf.find("\r\n\r\n", quote_pos);
            m_pos = enter_pos + 4;
            enter_pos = m_read_buf.find("\r\n" + boundary, m_pos);
            auto value = m_read_buf.substr(m_pos, enter_pos - m_pos);
            m_parameters[name] = value;
            m_pos = enter_pos + 2;
            enter_pos = m_read_buf.find("\r\n", m_pos);
        }
    }
    else
    {
        return LINE_BAD;
    }
    return LINE_OK;
}

// str 以 "key1=value1&key2=value2" 的形式传入
void HTTPConnection::parseParameters(std::string str)
{
    // printf("%s\n", str.c_str());
    std::vector<std::string> name;
    std::vector<std::string> value;
    // match_result 中所含的首个 sub_match （下标 0 ）始终表示 regex 所做的目标序列内的完整匹配，
    // 而后继的 sub_match 表示按顺序对应分隔正则表达式中子表达式的左括号的子表达式匹配
    std::smatch result;
    // (.*?) 表示是一个尽可能少地匹配字符的模式
    // (.*?)=(.*?)&，该表示式可以理解为完整匹配：xxx=xxx&
    std::regex findStartParameter("(.*?)=(.*?)&");
    std::regex_search(str, result, findStartParameter);
    // printf("first name %s\n", result.str(1).c_str());
    name.push_back(result.str(1));
    std::regex findParameterName("&(.*?)=");
    std::sregex_iterator iter(str.begin(), str.end(), findParameterName);
    std::sregex_iterator end;
    for (iter; iter != end; iter++)
    {
        // printf("name:%s\n", (*iter).str(1).c_str());
        name.push_back((*iter).str(1));
    }
    // 查找 value
    std::regex findParameterValue("=(.*?)&");
    std::sregex_iterator iter1(str.begin(), str.end(), findParameterValue);
    for (iter1; iter1 != end; iter1++)
    {
        // printf("value:%s\n", (*iter1).str(1).c_str());
        value.push_back((*iter1).str(1));
    }
    // 最后一个 value，通过反转 URL 后使用 ^(.*?)= 行首匹配
    std::string reverse_str = str;
    std::reverse(reverse_str.begin(), reverse_str.end());
    std::regex findParameterLastValue("^(.*?)=");
    regex_search(reverse_str, result, findParameterLastValue);
    // printf("last value %s\n", result.str(1).c_str());
    std::string lastValue = result.str(1);
    std::reverse(lastValue.begin(), lastValue.end());
    value.push_back(lastValue);
    for (int i = 0; i < name.size(); i++)
    {
        m_parameters[name[i]] = value[i];
    }
    // for(auto parameter : m_parameters) {
    //     printf("name %s  value %s\n", parameter.first.c_str(), parameter.second.c_str());
    // }
}

void HTTPConnection::parseURL()
{
    auto i = m_url.find('?');
    if (i != m_url.npos)
    {
        parseParameters(m_url.substr(i + 1));
    }
    std::string tmp = m_url.substr(0, i);
    if (tmp == "/")
    {
        tmp += "index.html";
    }
    m_file_path = doc_root + tmp;
    // printf("%s\n", m_file_path.c_str());
}

bool HTTPConnection::readMethod(int end)
{
    std::string tmp = m_read_buf.substr(m_pos, end - m_pos);
    if (tmp == "GET")
    {
        m_method = GET;
    }
    else if (tmp == "POST")
    {
        m_method = POST;
    }
    else if (tmp == "HEAD")
    {
        m_method = HEAD;
    }
    else if (tmp == "PUT")
    {
        m_method = PUT;
    }
    else if (tmp == "DELETE")
    {
        m_method = DELETE;
    }
    else if (tmp == "TRACE")
    {
        m_method = TRACE;
    }
    else if (tmp == "OPTIONS")
    {
        m_method = OPTIONS;
    }
    else if (tmp == "CONNECT")
    {
        m_method = CONNECT;
    }
    else
    {
        return false;
    }
    // printf("%s\n", tmp.c_str());
    return true;
}

bool HTTPConnection::readProtocol(int end)
{
    m_protocol = m_read_buf.substr(m_pos, end - m_pos);
    auto i = m_protocol.find('/', 0);
    if (i == m_protocol.npos)
    {
        return false;
    }
    std::string tmp = m_protocol.substr(i + 1);
    if (tmp == "1.0")
    {
        m_version = HTTP_1_0;
    }
    else if (tmp == "1.1")
    {
        m_version = HTTP_1_1;
    }
    else if (tmp == "2.0")
    {
        m_version = HTTP_2_0;
    }
    else if (tmp == "3.0")
    {
        m_version = HTTP_3_0;
    }
    else
    {
        return false;
    }
    return true;
}

bool HTTPConnection::readURL(int end)
{
    m_url = m_read_buf.substr(m_pos, end - m_pos);
    return true;
}

bool HTTPConnection::readHeader(int end)
{
    std::string name, value;
    auto tmp_pos = m_read_buf.find(": ", m_pos);
    if (tmp_pos == m_read_buf.npos)
    {
        return false;
    }
    name = m_read_buf.substr(m_pos, tmp_pos - m_pos);
    value = m_read_buf.substr(tmp_pos + 2, end - tmp_pos - 2);
    m_headers.emplace(name, value);
    if (name == "Content-Length")
    {
        m_content_length = atoi(value.c_str());
    }
    if (name == "Connection" && value == "keep-alive")
    {
        m_linger = true;
    }
    // printf("%s : %s\n", name.c_str(), value.c_str());
    return true;
}

// 当得到一个完整、正确的 HTTP 请求时，我们就分析目标文件的属性，
// 判断目标文件是否存在、对所有用户是否可读，是否是目录
PARSE_RESULT HTTPConnection::doRequest()
{
    switch (m_method)
    {
    case GET:
        // 获取 m_file_path 文件的相关状态信息，-1 失败，0 成功
        // printf("%s\n", m_file_path.c_str());
        if (stat(m_file_path.c_str(), &m_file_stat) < 0)
        {
            return NO_RESOURCE;
        }
        // 判断访问权限
        if (!(m_file_stat.st_mode & S_IROTH))
        { // S_IROTH: Read by others.
            return FORBIDDEN_REQUEST;
        }
        // 判断是否为目录
        if (S_ISDIR(m_file_stat.st_mode))
        {
            return BAD_REQUEST;
        }
        readFile();
        return FILE_REQUEST;
    case POST:
        if (m_action == QUIT || m_action == CANCEL || m_action == UPDATE || m_action == UPLOAD)
        {
            if (m_user.empty())
            {
                return BAD_REQUEST;
            }
        }
        if (!doAction())
        {
            return BAD_REQUEST;
        }
        return FILE_REQUEST;
    default:
        break;
    }
    return INTERNAL_ERROR;
}

void HTTPConnection::readFile()
{
    FILE *file = fopen(m_file_path.c_str(), "r");
    m_file_buf.resize(m_file_stat.st_size);
    fread(const_cast<char *>(m_file_buf.data()), m_file_stat.st_size, 1, file);
    fclose(file);
}

bool HTTPConnection::doAction()
{
    switch (m_action)
    {
    case LOGIN:
        return doLogin();
    case REGISTER:
        return doRegister();
    case QUIT:
        return doQuit();
    case CANCEL:
        return doCancel();
    case UPDATE:
        return doUpdate();
    case UPLOAD:
        return doUpload();
    default:
        break;
    }
    return false;
}

bool HTTPConnection::doLogin()
{
    auto db_conn = Database::getDBConnection();
    Database::values_array vs;
    bool flag = db_conn->find(m_parameters["username"], vs);
    Database::releaseDBConnection();
    if (!flag)
    {
        // printf("用户不存在！\n");
        m_file_buf = std::move(index_cgi("Login failed: the user does not exist!"));
    }
    else if (vs[PASSWD] != m_parameters["passwd"])
    {
        // printf("密码错误！\n");
        m_file_buf = std::move(index_cgi("Login failed: password error!"));
    }
    else
    {
        m_user = m_parameters["username"];
        m_file_buf = std::move(userinfo_cgi(m_user, vs));
    }
    return true;
}

bool HTTPConnection::doRegister()
{
    auto db_conn = Database::getDBConnection();
    Database::values_array vs(3);
    vs[PASSWD] = m_parameters["passwd"];
    vs[PORTRAIT] = "images/default.jpeg";
    vs[SIGNATURE] = "The man was too lazy to say anything.";
    bool flag = db_conn->add(m_parameters["username"], vs);
    Database::releaseDBConnection();
    if (!flag)
    {
        // printf("用户已存在！\n");
        m_file_buf = std::move(index_cgi("Fail to register: the user already exists!"));
    }
    else
    {
        m_user = m_parameters["username"];
        m_file_buf = std::move(userinfo_cgi(m_user, vs));
    }
    return true;
}

bool HTTPConnection::doQuit()
{
    m_user.clear();
    m_file_path = doc_root + "/index.html";
    stat(m_file_path.c_str(), &m_file_stat);
    readFile();
    return true;
}

bool HTTPConnection::doCancel()
{
    if(m_user.empty()) {
        m_file_buf = std::move(index_cgi("The session timedout, please login again"));
        return true;
    }
    auto db_conn = Database::getDBConnection();
    bool flag = db_conn->del(m_user);
    Database::releaseDBConnection();
    if (!flag)
    {
        // printf("不存在该用户！\n");
        return false;
    }
    return doQuit();
}

bool HTTPConnection::doUpdate()
{
    if(m_user.empty()) {
        m_file_buf = std::move(index_cgi("The session timedout, please login again"));
        return true;
    }
    auto db_conn = Database::getDBConnection();
    Database::values_array vs;
    bool flag = db_conn->find(m_user, vs);
    Database::releaseDBConnection();
    if (!flag)
    {
        // printf("用户不存在！\n");
        return false;
    }
    m_file_buf = std::move(upload_cgi(m_user, vs));
    return true;
}

bool HTTPConnection::doUpload()
{
    auto db_conn = Database::getDBConnection();
    Database::values_array vs;
    bool flag = db_conn->find(m_user, vs);
    vs[PASSWD] = m_parameters["passwd"];
    if (m_parameters["filename"] != "")
    {
        vs[PORTRAIT] = "images/" + m_user + "_portrait" + m_parameters["filename"].substr(m_parameters["filename"].find_last_of('.'));
        std::ofstream fout((doc_root + "/" + vs[PORTRAIT]).c_str(), std::ios::out | std::ios::binary);
        fout.write(m_parameters["portrait"].c_str(), m_parameters["portrait"].size());
        fout.close();
    }
    vs[SIGNATURE] = m_parameters["signature"];
    db_conn->mod(m_user, vs);
    Database::releaseDBConnection();
    if (!flag)
    {
        // printf("用户不存在！\n");
        return false;
    }
    m_file_buf = std::move(userinfo_cgi(m_user, vs));
    return true;
}

// 写 HTTP 响应
bool HTTPConnection::write()
{
    // printf("\n%s", m_write_buf.c_str());
    const char *ptr = m_write_buf.c_str();
    int bytes_to_send = m_write_buf.size();
    int bytes_have_send = 0;
    int tmp = 0;
    while (bytes_to_send > 0)
    {
        tmp = SSL_write(m_ssl, ptr, bytes_to_send);
        if (tmp < 0)
        {
            if (errno == EINTR)
            {
                tmp = 0;
                continue;
            }
            // 如果 TCP 写缓冲没有空间，则等待下一轮 EPOLLOUT 事件，虽然在此期间
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性
            if (errno == EAGAIN)
            {
                modfd(m_epoll_fd, m_sock_fd, EPOLLOUT);
                break;
            }
            return false;
        }
        bytes_have_send += tmp;
        bytes_to_send -= tmp;
        ptr += tmp;
    }
    if (bytes_to_send == 0)
    {
        // 将要发送的字节为 0，这一次响应结束，重置该连接
        modfd(m_epoll_fd, m_sock_fd, EPOLLIN);
        init();
    }
    else
    {
        m_write_buf = m_write_buf.substr(bytes_have_send);
    }
    return true;
}

void HTTPConnection::writeString(std::string str)
{
    m_write_buf += str;
}

void HTTPConnection::addStatusLine(std::string status, std::string title)
{
    writeString(m_protocol + " " + status + " " + title + "\r\n");
}

void HTTPConnection::addHeaders(int content_length)
{
    addContentLength(content_length);
    addContentType();
    addLinger();
    writeString("\r\n");
}

void HTTPConnection::addContentLength(int content_length)
{
    writeString("Content-Length: " + std::to_string(content_length) + "\r\n");
}

void HTTPConnection::addContentType()
{
    writeString(std::string("Content-Type: ") + "text/html" + "\r\n");
}

void HTTPConnection::addLinger()
{
    std::string tmp("Connection: ");
    tmp += m_linger ? "keep-alive" : "close";
    tmp += "\r\n";
    writeString(tmp);
}

void HTTPConnection::addContent(std::string content)
{
    writeString(content);
}

bool HTTPConnection::generateResponse(PARSE_RESULT result)
{
    switch (result)
    {
    case INTERNAL_ERROR:
        addStatusLine("500", error_500_title);
        addHeaders(error_500_form.size());
        addContent(error_500_form);
        break;
    case BAD_REQUEST:
        addStatusLine("400", error_400_title);
        addHeaders(error_400_form.size());
        addContent(error_400_form);
        break;
    case NO_RESOURCE:
        addStatusLine("404", error_404_title);
        addHeaders(error_404_form.size());
        addContent(error_404_form);
        break;
    case FORBIDDEN_REQUEST:
        addStatusLine("403", error_403_title);
        addHeaders(error_403_form.size());
        addContent(error_403_form);
        break;
    case FILE_REQUEST:
        addStatusLine("200", ok_200_title);
        addHeaders(m_file_buf.size());
        addContent(m_file_buf);
        break;
    default:
        return false;
    }
    return true;
}

// 由线程池中的工作线程调用，这是处理 HTTP 请求的入口函数
void HTTPConnection::process()
{
    // 解析 HTTP 请求
    // printf("parse request.\n");
    PARSE_RESULT parse_result = parseRequest();
    if (parse_result == NO_REQUEST)
    {
        modfd(m_epoll_fd, m_sock_fd, EPOLLIN);
        return;
    }
    // 生成响应
    bool write_ret = generateResponse(parse_result);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epoll_fd, m_sock_fd, EPOLLOUT);
}

void HTTPConnection::setTimer(std::shared_ptr<TimerNode> timer_)
{
    // printf("http connection set timer.\n");
    LOG_DEBUG << "set timer" << Log::endl;
    m_timer = timer_;
}

void HTTPConnection::updateTimer(int timeout)
{
    if(m_timer.get()) {
        LOG_DEBUG << "update timer" << Log::endl;
        m_timer->update(timeout);
    }
}