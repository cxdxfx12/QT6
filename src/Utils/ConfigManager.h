#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QSettings>

class ConfigManager : public QObject {
    Q_OBJECT
public:
    explicit ConfigManager(QObject *parent = nullptr);

    int defaultThreadCount() const;
    void setDefaultThreadCount(int count);

    int batchSize() const;
    void setBatchSize(int size);

    QString lastImportPath() const;
    void setLastImportPath(const QString &path);

    QString lastExportPath() const;
    void setLastExportPath(const QString &path);

    QString rulesFilePath() const;
    void setRulesFilePath(const QString &path);

    void sync();

private:
    QSettings m_settings;
};

#endif // CONFIGMANAGER_H
