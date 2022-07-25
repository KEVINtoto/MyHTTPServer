
#include "log.h"

Endl Log::endl;

int Log::MAX_LINES = 100;

Log::Log() : log_thread(nullptr)
{
    line_count_ = 0;
    log_thread_stop = false;
}

Log::~Log()
{
    while (!log_front_buf.empty())
    {
        flush();
    }
    log_thread_stop = true;
    log_thread_sem.post();
    pthread_join(*log_thread.get(), NULL);
    log_writer.close();
}

Log *Log::getInstance()
{
    static Log logger;
    return &logger;
}

void Log::init(std::string log_file_path_, int max_line_)
{
    MAX_LINES = max_line_;
    // 创建后台线程
    log_thread.reset(new pthread_t);
    pthread_create(log_thread.get(), NULL, log_thread_run, this);
    // 打开文件
    log_writer.open(log_file_path_, std::ios::app);
}

void Log::flush()
{
    log_thread_locker.lock();
    swap(log_front_buf, log_end_buf);
    line_count_ = 0;
    log_thread_locker.unlock();
    log_thread_sem.post();
}

void *Log::log_thread_run(void *arg)
{
    auto obj_ptr = (Log *)arg;
    obj_ptr->log_async_write();
    return obj_ptr;
}

void Log::log_async_write()
{
    while (!log_thread_stop)
    {
        log_thread_sem.wait();
        log_thread_locker.lock();
        if (log_end_buf.empty())
        {
            log_thread_locker.unlock();
            continue;
        }
        // 开始向文件里面写
        log_writer.write(log_end_buf.c_str(), log_end_buf.size());
        log_writer.flush();
        log_end_buf.clear();
        log_thread_locker.unlock();
    }
}

void Log::append_title(int level, std::string __file, int __line)
{
    line_locker.lock();
    switch (level)
    {
    case 0:
        *this << "[DEBUG ";
        break;
    case 1:
        *this << "[INFO ";
        break;
    case 2:
        *this << "[WARN ";
        break;
    case 3:
        *this << "[ERROR ";
        break;
    default:
        return;
    }
    time_t cur_sec;
    time(&cur_sec);
    tm *nowtime = localtime(&cur_sec);
    char time_str[20];
    sprintf(time_str, "%.4d-%.2d-%.2d %.2d:%.2d:%.2d ",
             1900 + nowtime->tm_year, 1 + nowtime->tm_mon, nowtime->tm_mday,
             nowtime->tm_hour, nowtime->tm_min, nowtime->tm_sec);
    *this << time_str << __file << ":" << __line << "] ";
}

Log &Log::operator<<(bool b)
{
    log_front_buf.append(b ? "1" : "0", 1);
    return *this;
}
Log &Log::operator<<(short s)
{
    *this << static_cast<int>(s);
    return *this;
}
Log &Log::operator<<(unsigned short us)
{
    *this << static_cast<unsigned int>(us);
    return *this;
}
Log &Log::operator<<(int i)
{
    *this << std::to_string(i);
    return *this;
}
Log &Log::operator<<(unsigned int ui)
{
    *this << std::to_string(ui);
    return *this;
}
Log &Log::operator<<(long l)
{
    *this << std::to_string(l);
    return *this;
}
Log &Log::operator<<(unsigned long ul)
{
    *this << std::to_string(ul);
    return *this;
}
Log &Log::operator<<(long long ll)
{
    *this << std::to_string(ll);
    return *this;
}
Log &Log::operator<<(unsigned long long ull)
{
    *this << std::to_string(ull);
    return *this;
}
Log &Log::operator<<(float f)
{
    *this << static_cast<double>(f);
    return *this;
}
Log &Log::operator<<(double d)
{
    *this << std::to_string(d);
    return *this;
}
Log &Log::operator<<(long double ld)
{
    *this << std::to_string(ld);
    return *this;
}
Log &Log::operator<<(char c)
{
    log_front_buf.append(&c, 1);
    return *this;
}
Log &Log::operator<<(const char *str)
{
    if (str)
        log_front_buf.append(str, strlen(str));
    else
        log_front_buf.append("(null)", 6);
    return *this;
}
Log &Log::operator<<(const unsigned char *str)
{
    return operator<<(reinterpret_cast<const char *>(str));
}
Log &Log::operator<<(const std::string &str)
{
    log_front_buf.append(str);
    return *this;
}
Log &Log::operator<<(const Endl &endl)
{
    *this << '\n';
    ++line_count_;
    if (line_count_ == MAX_LINES)
    {
        flush();
    }
    line_locker.unlock();
}