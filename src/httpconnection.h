
#pragma once

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <unordered_map>
#include <string.h>
#include <string>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <regex>
#include <stdarg.h>
#include <errno.h>
#include <sys/uio.h>
#include <openssl/ssl.h>
#include "locker.h"
#include "database.h"
#include "timer.h"

class TimerNode;

// HTTP 请求方法
enum METHOD
{
    GET = 0,
    POST,
    HEAD,
    PUT,
    DELETE,
    TRACE,
    OPTIONS,
    CONNECT
};

/* 解析客户端请求时，主状态机的状态：
 * PARSE_STATE_REQUESTLINE : 当前正在解析请求行
 * PARSE_STATE_HEADER      : 当前正在解析请求头部
 * PARSE_STATE_CONTENT     : 当前正在解析请求体 */
enum PARSE_STATE
{
    PARSE_STATE_REQUESTLINE = 0,
    PARSE_STATE_HEADER,
    PARSE_STATE_CONTENT,
    PARSE_DONE
};

/* 从状态机的三种可能状态，即行的读取状态，分别表示：
 * 1. 读取到一个完整的行；
 * 2. 行出错；
 * 3. 行数据尚且不完整 */
enum LINE_STATUS
{
    LINE_OK = 0,
    LINE_BAD,
    LINE_OPEN
};

/* 服务器处理 HTTP 请求的可能结果，报文解析的结果：
 * NO_REQUEST:          请求不完整，需要继续读取客户端数据
 * GET_REQUEST:         表示获得一个完成的客户请求
 * BAD_REQUEST:         表示客户请求语法错误
 * NO_RESOURCE:         表示服务器没有资源
 * FORBIDDEN_REQUEST:   表示客户端对资源没有足够的访问权限
 * FILE_REQUEST:        文件请求，获取文件成功
 * INTERNAL_ERROR:      表示服务器内部错误
 * DATABASE_REQUEST:    数据库访问请求 */
enum PARSE_RESULT
{
    NO_REQUEST,
    GET_REQUEST,
    BAD_REQUEST,
    NO_RESOURCE,
    FORBIDDEN_REQUEST,
    FILE_REQUEST,
    INTERNAL_ERROR
};

enum HTTP_VERSION
{
    HTTP_1_0,
    HTTP_1_1,
    HTTP_2_0,
    HTTP_3_0
};

enum ACTION
{
    LOGIN,
    REGISTER,
    QUIT,
    CANCEL,
    UPDATE,
    UPLOAD
};

enum INFO_TYPE
{
    PASSWD = 0,
    PORTRAIT,
    SIGNATURE
};

class HTTPConnection
{
public:
    static int m_epoll_fd;                     // 所有 socket 上的事件都被注册到同一个 epoll 对象中
    static int m_user_count;                   // 统计用户的数量
    static const int READ_BUFFER_SIZE = 4096;  // 读缓冲区的大小
    static const int WRITE_BUFFER_SIZE = 4096; // 写缓冲区的大小

    HTTPConnection() {}
    ~HTTPConnection() {}

    void init(int sock_fd, const sockaddr_in &addr, SSL *ssl); // 初始化新的连接
    void process();                                            // 处理请求
    void close_conn();                                         // 关闭连接
    bool read();                                               // 非阻塞地读
    bool write();                                              // 非阻塞地写
    void setTimer(std::shared_ptr<TimerNode>);
    void updateTimer(int timeout);

private:
    int m_sock_fd;             // 该 HTTP 连接的 socket
    SSL *m_ssl;                // SSL
    sockaddr_in m_address;     // 通信对方的 socket 地址
    Database::key_type m_user; // 当前连接的用户
    std::shared_ptr<TimerNode> m_timer;

    std::string m_read_buf; // 读缓冲区
    int m_pos;              // 目前正在读的位置
    int m_line;             // 目前正在读的位置的行首位置
    int m_read_size;        // 读缓冲区的大小
    PARSE_STATE m_parse_state;
    LINE_STATUS m_line_status;

    std::string m_file_path; // 客户请求的目标文件的完整路径，其内容等于 doc_root + m_url, doc_root 是网站根目录
    METHOD m_method;         // 请求方法
    std::string m_url;       // 请求目标文件的文件名
    ACTION m_action;
    std::string m_protocol; // 协议
    HTTP_VERSION m_version; // 协议版本
    // std::string m_host;                       // 主机名
    int m_content_length; // HTTP 请求的消息总长度
    bool m_linger;        // HTTP 请求是否保持连接
    std::unordered_map<std::string, std::string> m_parameters;
    std::unordered_map<std::string, std::string> m_headers;

    std::string m_write_buf; // 写缓冲区
    std::string m_file_buf;
    struct stat m_file_stat; // 目标文件的状态。可以用来判断文件是否存在、是否为目录、是否可读，并获取文件大小等相关信息

    void init(); // 初始化除了连接以外的信息

    PARSE_RESULT parseRequest();
    LINE_STATUS parseRequestLine();
    bool readMethod(int end);
    bool readURL(int end);
    bool readProtocol(int end);
    LINE_STATUS parseHeaders();
    bool readHeader(int end);
    LINE_STATUS parseContent();
    LINE_STATUS isBlank();
    LINE_STATUS isSpecialSymbol();
    LINE_STATUS readString(std::string &);
    void parseURL();
    void parseParameters(std::string);
    PARSE_RESULT doRequest();
    bool doAction();
    bool doLogin();
    bool doRegister();
    bool doQuit();
    bool doCancel();
    bool doUpdate();
    bool doUpload();
    void readFile();

    bool generateResponse(PARSE_RESULT result); // 生成 HTTP 响应
    void writeString(std::string str);
    void addStatusLine(std::string status, std::string title);
    void addHeaders(int content_length);
    void addContentLength(int content_length);
    void addContentType();
    void addLinger();
    void addContent(std::string content);
};
