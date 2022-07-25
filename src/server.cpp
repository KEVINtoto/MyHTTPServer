
#include "server.h"
#include "log.h"

extern void addfd(int epollfd, int fd, bool one_shot);
extern void modfd(int epollfd, int fd, int ev);
extern void removefd(int epollfd, int fd);

// 添加信号捕捉
void addsig(int sig, void(handler)(int))
{
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

Server::Server(int _port, int max_fd_, int max_events_, int thread_number_, int max_request_, int timeout_)
    : port(_port),
      pool(new ThreadPool<HTTPConnection>(thread_number_, max_request_)),
      clients(max_fd_),
      events(max_events_),
      stop(true),
      conn_timeout(timeout_)
{
}

void Server::init(const std::string cert_path, const std::string cert_passwd, const std::string prikey_path)
{
    // 初始化 SSL 环境
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, NULL);
    ctx = SSL_CTX_new(TLS_method());
    if (ctx == NULL)
    {
        // cout << "create ctx wrong." << endl;
        LOG_ERROR << "create ctx wrong." << Log::endl;
        return;
    }
    SSL_CTX_set_mode(ctx, SSL_MODE_AUTO_RETRY);
    SSL_CTX_set_options(ctx,
                        SSL_OP_ALL | SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 |
                            SSL_OP_NO_COMPRESSION | SSL_OP_NO_TLSv1_3 |
                            SSL_OP_NO_SESSION_RESUMPTION_ON_RENEGOTIATION);
    // 加载证书
    if (SSL_CTX_use_certificate_chain_file(ctx, cert_path.c_str()) != 1)
    {
        // cout << "cert load wrong." << endl;
        LOG_ERROR << "cert load wrong." << Log::endl;
        return;
    }
    SSL_CTX_set_default_passwd_cb_userdata(ctx, (void *)cert_passwd.c_str());
    if (SSL_CTX_use_PrivateKey_file(ctx, prikey_path.c_str(), SSL_FILETYPE_PEM) != 1)
    {
        // cout << "key load wrong." << endl;
        LOG_ERROR << "key load wrong." << Log::endl;
        return;
    }
    stop = false;
}

void Server::start()
{
    addsig(SIGPIPE, SIG_IGN);
    // 创建监听套接字
    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1)
    {
        LOG_ERROR << "socket" << Log::endl;
        stop = true;
        return;
    }
    // 设置端口复用
    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
    // 端口绑定
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr *)&address, sizeof(address)) == -1)
    {
        LOG_ERROR << "bind" << Log::endl;
        stop = true;
        return;
    }
    // 监听
    if (listen(listen_fd, 5) == -1)
    {
        LOG_ERROR << "listen" << Log::endl;
        stop = true;
        return;
    }
    // 创建 epoll 对象
    epoll_fd = epoll_create(6);
    // 将监听的文件描述符添加到 epoll 对象中
    addfd(epoll_fd, listen_fd, false);
    HTTPConnection::m_epoll_fd = epoll_fd;
}

void Server::loop()
{
    while (!stop)
    {
        int num = epoll_wait(epoll_fd, &*events.begin(), events.size(), conn_timeout);
        if (num < 0 && errno != EINTR)
        {
            // cout << "epoll failure." << endl;
            LOG_ERROR << "epoll failure." << Log::endl;
            stop = true;
            break;
        }
        // 循环遍历事件数组
        for (int i = 0; i < num; ++i)
        {
            int sock_fd = events[i].data.fd;
            if (sock_fd == listen_fd)
            { // 有客户端连接进来
                struct sockaddr_in client_address;
                socklen_t client_addrlen = sizeof(client_address);
                int connect_fd = accept(listen_fd, (struct sockaddr *)&client_address, &client_addrlen);
                if (connect_fd < 0)
                {
                    LOG_ERROR << "An error occurred, the errno is: " << errno << Log::endl;
                    continue;
                }
                if (HTTPConnection::m_user_count >= clients.size())
                {
                    // 目前已达到最大连接数
                    // 给客户端回复信息："服务器忙"
                    std::string tmp = "Internal server busy.";
                    send(connect_fd, tmp.c_str(), tmp.size(), 0);
                    close(connect_fd);
                    continue;
                }
                // 将新的客户端连接数据初始化，放入到数组中
                SSL *new_ssl = SSL_new(ctx);
                if (new_ssl == NULL)
                {
                    LOG_ERROR << "ssl new wrong." << Log::endl;
                    continue;
                }
                SSL_set_fd(new_ssl, connect_fd);
                SSL_accept(new_ssl);
                clients[connect_fd].init(connect_fd, client_address, new_ssl);
                timer_heap.addTimer(&clients[connect_fd], conn_timeout);
                LOG_INFO << "new client: " << connect_fd << Log::endl;
            }
            else if (events[i].events & (EPOLLHUP | EPOLLRDHUP | EPOLLERR))
            {
                // 客户端异常断开或者发生了错误事件
                if(events[i].events & EPOLLHUP) {
                    LOG_DEBUG << "EPOLLHUP" << Log::endl;
                }
                if(events[i].events & EPOLLRDHUP) {
                    LOG_DEBUG << "EPOLLRDHUP" << Log::endl;
                }
                if(events[i].events & EPOLLERR) {
                    LOG_DEBUG << "EPOLLERR" << Log::endl;
                }
                clients[sock_fd].close_conn();
            }
            else if (events[i].events & EPOLLIN)
            {
                // printf("come request.\n");
                if (clients[sock_fd].read())
                { 
                    pool->append(&clients[sock_fd]);
                    clients[sock_fd].updateTimer(conn_timeout);
                }
                else
                {
                    clients[sock_fd].close_conn();
                }
            }
            else if (events[i].events & EPOLLOUT)
            {
                if (!clients[sock_fd].write())
                { 
                    clients[sock_fd].close_conn();
                }
                clients[sock_fd].updateTimer(conn_timeout);
                // printf("send file.\n");
            }
        }
        timer_heap.handleExpireEvent();
    }
    stop = true;
    LOG_INFO << "The server stops running." << Log::endl;
}

Server::~Server()
{
    close(epoll_fd);
    close(listen_fd);
    SSL_CTX_free(ctx);
}