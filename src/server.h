
#pragma once

#include <iostream>
#include <sys/socket.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <memory>
#include <vector>
#include <openssl/ssl.h>
#include <sys/epoll.h>
#include "httpconnection.h"
#include "threadpool.h"

class Server
{
public:
    Server(int _port, int, int, int, int, int);
    ~Server();
    void init(const std::string, const std::string, const std::string);
    void start();
    void loop();

private:
    int port;                                         // 端口号
    std::unique_ptr<ThreadPool<HTTPConnection>> pool; // 线程池
    std::vector<HTTPConnection> clients;              // 用于保存所有的客户端信息
    /* SSL_CTX 数据结构主要用于 SSL 握手前的环境准备，设置 CA 文件和目录、
    设置 SSL 握手中的证书文件和私钥、设置协议版本以及其他一些 SSL 握手时的选项。 */
    SSL_CTX *ctx;
    int listen_fd; // 监听的 socket 文件描述符
    int epoll_fd;  // epoll 对象的文件描述符
    std::vector<epoll_event> events;
    bool stop;
    TimerHeap timer_heap;
    time_t conn_timeout;
};
