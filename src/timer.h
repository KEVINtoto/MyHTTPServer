
#pragma once

#include <time.h>
#include <vector>
#include <queue>
#include <memory>
#include "httpconnection.h"

class HTTPConnection;

class TimerNode
{
public:
    TimerNode(HTTPConnection *conn, int timeout);
    ~TimerNode();
    bool isExpired();
    bool isDeleted();
    void setDeleted();
    void update(int timeout);
    time_t getExpireTime();

private:
    bool deleted;
    time_t expire;
    HTTPConnection *http_conn;
};

struct TimerCmp
{
    bool operator()(std::shared_ptr<TimerNode> &a, std::shared_ptr<TimerNode> &b) const
    {
        return a->getExpireTime() > b->getExpireTime();
    }
};

class TimerHeap
{
public:
    TimerHeap() {}
    ~TimerHeap() {}
    void addTimer(HTTPConnection *, int timeout);
    void handleExpireEvent();

private:
    typedef std::shared_ptr<TimerNode> timer_node_ptr;
    std::priority_queue<timer_node_ptr, std::vector<timer_node_ptr>, TimerCmp> timer_queue;
};
