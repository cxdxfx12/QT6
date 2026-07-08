#include "ThreadPool.h"
#include <QThread>

ThreadPool& ThreadPool::instance()
{
    static ThreadPool s_instance;
    return s_instance;
}

ThreadPool::ThreadPool()
{
    m_pool.setMaxThreadCount(QThread::idealThreadCount());
}

ThreadPool::~ThreadPool()
{
    m_pool.waitForDone();
}

QThreadPool* ThreadPool::pool()
{
    return &m_pool;
}

int ThreadPool::maxThreadCount() const
{
    return m_pool.maxThreadCount();
}

void ThreadPool::setMaxThreadCount(int count)
{
    m_pool.setMaxThreadCount(count);
}

int ThreadPool::activeThreadCount() const
{
    return m_pool.activeThreadCount();
}
