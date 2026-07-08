#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <QObject>
#include <QThreadPool>

class ThreadPool : public QObject {
    Q_OBJECT
public:
    static ThreadPool& instance();

    QThreadPool* pool();
    int maxThreadCount() const;
    void setMaxThreadCount(int count);
    int activeThreadCount() const;

private:
    ThreadPool();
    ~ThreadPool();
    QThreadPool m_pool;
};

#endif // THREADPOOL_H
