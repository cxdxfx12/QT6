#include "Logger.h"

Logger& Logger::instance()
{
    static Logger logger;
    return logger;
}

Logger::Logger()
{
}

Logger::~Logger()
{
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

void Logger::setLogFile(const QString &filePath)
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
    m_filePath = filePath;
    m_file.setFileName(filePath);
    if (m_file.open(QIODevice::Append | QIODevice::Text)) {
        m_stream.setDevice(&m_file);
    }
}

void Logger::log(Level level, const QString &message)
{
    QMutexLocker locker(&m_mutex);
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString formatted = QString("[%1] [%2] %3").arg(timestamp, levelToString(level), message);
    m_stream << formatted << "\n";
    m_stream.flush();
}

void Logger::debug(const QString &msg)
{
    log(DEBUG, msg);
}

void Logger::info(const QString &msg)
{
    log(INFO, msg);
}

void Logger::warning(const QString &msg)
{
    log(WARNING, msg);
}

void Logger::error(const QString &msg)
{
    log(ERROR, msg);
}

QString Logger::levelToString(Level level)
{
    switch (level) {
    case DEBUG:   return "DEBUG";
    case INFO:    return "INFO";
    case WARNING: return "WARN";
    case ERROR:   return "ERROR";
    default:      return "UNKNOWN";
    }
}
