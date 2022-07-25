
#pragma once

#include <pthread.h>
#include <memory>
#include <string>
#include <string.h>
#include <fstream>
#include <ctime>
#include "locker.h"

struct Endl
{
};

class Log
{
public:
    static Endl endl;
    void init(std::string file_path, int max_lines_); // 创建后端线程和打开文件
    static Log *getInstance();

    void append_title(int, std::string, int); // 写每一行的开头标题
    Log &operator<<(bool);
    Log &operator<<(short);
    Log &operator<<(unsigned short);
    Log &operator<<(int);
    Log &operator<<(unsigned int);
    Log &operator<<(long);
    Log &operator<<(unsigned long);
    Log &operator<<(long long);
    Log &operator<<(unsigned long long);
    Log &operator<<(float);
    Log &operator<<(double);
    Log &operator<<(long double);
    Log &operator<<(char);
    Log &operator<<(const char *);
    Log &operator<<(const unsigned char *);
    Log &operator<<(const std::string &);
    Log &operator<<(const Endl &endl);

private:
    Log();
    ~Log();
    void flush(); // 唤醒后端线程往文件里写
    static void *log_thread_run(void *);
    void log_async_write();

private:
    static int MAX_LINES;

    std::string log_front_buf, log_end_buf;
    std::ofstream log_writer;

    int line_count_;

    std::unique_ptr<pthread_t> log_thread;
    bool log_thread_stop;

    Locker line_locker;
    Locker log_thread_locker;
    Sem log_thread_sem;
};

#define LOG_BASE(level)                                              \
    do                                                               \
    {                                                                \
        Log::getInstance()->append_title(level, __FILE__, __LINE__); \
    } while (0);                                                     \
    *Log::getInstance()

#define LOG_DEBUG LOG_BASE(0)
#define LOG_INFO LOG_BASE(1)
#define LOG_WARN LOG_BASE(2)
#define LOG_ERROR LOG_BASE(3)