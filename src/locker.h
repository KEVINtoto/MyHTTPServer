
#pragma once

#include <pthread.h>
#include <exception>
#include <semaphore.h>

// 互斥锁类
class Locker
{
public:
    // 初始化一个互斥量
    Locker()
    {
        if (pthread_mutex_init(&m_mutex, NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~Locker()
    {
        pthread_mutex_destroy(&m_mutex);
    }
    // 对互斥量进行加锁
    bool lock()
    {
        return pthread_mutex_lock(&m_mutex) == 0;
    }
    // 对互斥量进行解锁
    bool unlock()
    {
        return pthread_mutex_unlock(&m_mutex) == 0;
    }
    // 获取互斥量
    pthread_mutex_t *get()
    {
        return &m_mutex;
    }

private:
    pthread_mutex_t m_mutex;
};

// 条件变量类
class Cond
{
public:
    // 初始化条件变量
    Cond()
    {
        if (pthread_cond_init(&m_cond, NULL) != 0)
        {
            throw std::exception();
        }
    }
    ~Cond()
    {
        pthread_cond_destroy(&m_cond);
    }
    // 等待条件变量发出信号或广播。在这之前假定互斥量是锁定的。
    bool wait(pthread_mutex_t *mutex)
    {
        return pthread_cond_wait(&m_cond, mutex) == 0;
    }
    bool timewait(pthread_mutex_t *mutex, struct timespec t)
    {
        return pthread_cond_timedwait(&m_cond, mutex, &t) == 0;
    }
    // 唤醒一个等待条件变量 m_cond 的线程
    bool signal()
    {
        return pthread_cond_signal(&m_cond) == 0;
    }
    // 唤醒所有等待条件变量 m_cond 的线程
    bool broadcast()
    {
        return pthread_cond_broadcast(&m_cond) == 0;
    }

private:
    pthread_cond_t m_cond;
};

// 信号量类
class Sem
{
public:
    Sem()
    {
        if (sem_init(&m_sem, 0, 0) != 0)
        {
            throw std::exception();
        }
    }
    Sem(int num)
    {
        // pshared：0 表示线程间共享，其它表示多少个进程间共享
        // value：信号量的初始值
        if (sem_init(&m_sem, 0, num) != 0)
        {
            throw std::exception();
        }
    }
    ~Sem()
    {
        sem_destroy(&m_sem);
    }
    // 以原子操作的方式给信号量的值减 1（P 操作），如果信号量的值为 0 函数将会等待，直到有线程增加了该信号量的值使其不再为 0
    bool wait()
    {
        return sem_wait(&m_sem) == 0;
    }
    // 是以原子操作的方式给信号量的值加 1（V 操作），并发出信号唤醒等待线程 sem_wait
    bool post()
    {
        return sem_post(&m_sem) == 0;
    }

private:
    sem_t m_sem;
};

class RWLocker
{
public:
    RWLocker() : stat(0) {}

    void readLock()
    {
        locker.lock();
        while (stat < 0)
        { // stat 小于 0，表示当前有写操作在进行。
            cond.wait(locker.get());
        }
        ++stat;
        locker.unlock();
    }

    void readUnlock()
    {
        locker.lock();
        if (--stat == 0)
        { // 当前无锁，写操作可以进行
            cond.signal();
        }
        locker.unlock();
    }

    void writeLock()
    {
        locker.lock();
        while (stat != 0)
        { // 只要不是无锁状态写操作就得等待
            cond.wait(locker.get());
        }
        stat = -1;
        locker.unlock();
    }

    void writeUnlock()
    {
        locker.lock();
        stat = 0;
        cond.broadcast();
        locker.unlock();
    }

private:
    Locker locker;
    Cond cond;
    // 状态标记，0 表示没上锁， > 0 表示有上了几次读锁， -1 表示上了写锁
    int stat;
};
