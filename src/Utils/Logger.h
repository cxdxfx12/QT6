#ifndef LOGGER_H
#define LOGGER_H

#include <QString>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <QDateTime>

class Logger {
public:
    enum Level { DEBUG, INFO, WARNING, ERROR };

    static Logger& instance();
    void setLogFile(const QString &filePath);
    void log(Level level, const QString &message);
    void debug(const QString &msg);
    void info(const QString &msg);
    void warning(const QString &msg);
    void error(const QString &msg);

private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    QFile m_file;
    QTextStream m_stream;
    QMutex m_mutex;
    QString m_filePath;

    QString levelToString(Level level);
};

#endif // LOGGER_H
