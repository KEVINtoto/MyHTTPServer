
#include "timer.h"

TimerNode::TimerNode(HTTPConnection *conn, int timeout)
{
    deleted = false;
    expire = time(NULL) + timeout;
    // printf("%ld %ld\n", cur_time, expire);
    http_conn = conn;
}

TimerNode::~TimerNode()
{
    if (http_conn) {
        http_conn->close_conn();
        // printf("~TimerNode()\n");
    }
}

bool TimerNode::isExpired()
{
    time_t now = time(NULL);
    // printf("%ld %ld\n", now, expire);
    if (expire <= now)
    {
        deleted = true;
        return true;
    }
    else
    {
        return false;
    }
}

bool TimerNode::isDeleted()
{
    return deleted;
}

void TimerNode::setDeleted()
{
    deleted = true;
    http_conn = nullptr;
}

void TimerNode::update(int timeout)
{
    expire = time(NULL) + timeout;
}

time_t TimerNode::getExpireTime()
{
    return expire;
}

void TimerHeap::addTimer(HTTPConnection *conn, int timeout)
{
    // printf("timer heap add timer.\n");
    std::shared_ptr<TimerNode> new_node(new TimerNode(conn, timeout));
    timer_queue.push(new_node);
    conn->setTimer(new_node);
}

void TimerHeap::handleExpireEvent()
{
    while (!timer_queue.empty())
    {
        timer_node_ptr top_node = timer_queue.top();
        if (top_node->isDeleted())
        {
            // printf("deleted.\n");
            timer_queue.pop();
        }
        else if (top_node->isExpired())
        {
            // printf("expire.\n");
            timer_queue.pop();
        }
        else
        {
            break;
        }
    }
}