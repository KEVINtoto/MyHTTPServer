# MyHTTPServer

该代码仓库基于 `C++11` 实现一个比较轻量级的 `HTTP` 服务器，主要特性和模块如下所示：
* 服务器部分使用的是单 `Reactor` 多线程网络模式，主线程通过一个 `epoll` 对象以 `ET` 触发模式来处理客户端的连接事件、读事件和写事件。客户端的请求由线程池里的工作线程来处理，各线程之间互斥地从请求队列中获取请求对象。这里主要参考《Linux 高性能服务器编程》里的实现。
* 在 `HTTP/1.1` 的基础上支持 `HTTPS` 请求，支持 `GET` 和 `POST` 请求方法，其中 `POST` 请求方法支持文本类型和二进制类型的数据。
* 使用有限状态机来解析请求报文，使用正则表达式解析 `URL` 和请求内容里的参数；使用“伪 CGI”函数来根据请求内容动态生成网页。
* 使用时间堆来实现客户端请求的「超时断连」机制，采用「懒删除」的方式在每次遍历完 `epoll` 事件后才进行超时事件的处理而没有设置定时器。
* 使用模板编程实现了一个跳跃表和一个简单的跳跃表迭代器。并基于此跳跃表实现了一个 `Key-Value` 内存型数据库，使用读写锁来互斥不同线程的读写操作。支持从文件将数据加载到内存和定时将数据持久化到磁盘中。
* 实现了一个简单的异步双缓冲区日志系统，当前端缓冲区达到设置的最大行数时会交由后端线程异步地将其内容写入到文件中。
* 实现了一个轻量的 `JSON` 解析器，通过解析配置文件里的参数来初始化服务器、数据库和日志系统。这里主要参考了 [https://github.com/miloyip/json-tutorial](https://github.com/miloyip/json-tutorial) 的实现。

## MyHTTPServer 框架图

![MyHTTPServer](https://user-images.githubusercontent.com/34743589/181698914-7e8658da-d215-4a5c-b923-600a5eafb603.png)

## 后续可进行的工作和改进
* 将单 Reactor 改为多 Reactor 模式，减少主线程的 I/O 压力。
* ......
