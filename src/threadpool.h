
#pragma once

#include <pthread.h>
#include <exception>
#include <list>
#include <cstdio>
#include "locker.h"

// 线程池类，模板类
template <typename T>
class ThreadPool
{
public:
    ThreadPool(int thread_number = 8, int max_requests = 10000);
    ~ThreadPool();
    bool append(T *request);

private:
    // 线程的数量
    int m_thread_number;
    // 线程池数组，大小为 m_thread_number
    pthread_t *m_threads;
    // 请求队列最大的等待数量
    int m_max_requests;
    // 请求队列
    std::list<T *> m_work_queue;
    // 互斥锁
    Locker m_queue_locker;
    // 信号量用来判断是否有任务需要处理
    Sem m_queue_stat;
    // 是否结束线程
    bool m_stop;
    // 线程处理函数
    static void *worker(void *arg);
    void run();
};

template <typename T>
ThreadPool<T>::ThreadPool(int thread_number, int max_requests)
    : m_thread_number(thread_number),
      m_max_requests(max_requests),
      m_stop(false),
      m_threads(NULL)
{
    if ((thread_number <= 0) || (max_requests <= 0))
    {
        throw std::exception();
    }
    m_threads = new pthread_t[m_thread_number];
    if (!m_threads)
    {
        throw std::exception();
    }
    // 创建 thread_number 个线程，并将它们设置为脱离
    for (int i = 0; i < thread_number; i++)
    {
        printf("Creating the %dth thread.\n", i);
        // 将当前的线程池对象传入到线程的 worker 函数中
        if (pthread_create(m_threads + i, NULL, worker, this) != 0)
        {
            delete[] m_threads;
            throw std::exception();
        }
        if (pthread_detach(m_threads[i]))
        {
            delete[] m_threads;
            throw std::exception();
        }
    }
}

template <typename T>
ThreadPool<T>::~ThreadPool()
{
    m_stop = true;
    delete[] m_threads;
}

template <typename T>
bool ThreadPool<T>::append(T *request)
{
    m_queue_locker.lock(); // 对请求队列加锁
    if (m_work_queue.size() > m_max_requests)
    {                            // 当前请求队列已满
        m_queue_locker.unlock(); // 解锁
        return false;            // 当前请求加入失败
    }
    m_work_queue.push_back(request); // 加入队列
    m_queue_locker.unlock();         // 解锁
    m_queue_stat.post();             // 更新信号量
    return true;
}

template <typename T>
void *ThreadPool<T>::worker(void *arg)
{
    // 线程处理函数
    ThreadPool *pool = (ThreadPool *)arg;
    pool->run();
    return pool;
}

template <typename T>
void ThreadPool<T>::run()
{
    while (!m_stop)
    {
        m_queue_stat.wait();   // 如果有请求则进行处理
        m_queue_locker.lock(); // 加锁
        if (m_work_queue.empty())
        {                            // 当前请求队列为空
            m_queue_locker.unlock(); // 解锁
            continue;
        }
        T *request = m_work_queue.front(); // 获取请求
        m_work_queue.pop_front();
        m_queue_locker.unlock();
        if (!request)
        {
            continue;
        }
        request->process(); // 处理获取到的请求
    }
}
