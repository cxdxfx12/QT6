#include "ConfigManager.h"

ConfigManager::ConfigManager(QObject *parent)
    : QObject(parent)
    , m_settings("ExpressFreight", "ExpressFreightCalculator")
{
}

int ConfigManager::defaultThreadCount() const
{
    return m_settings.value("defaultThreadCount", 8).toInt();
}

void ConfigManager::setDefaultThreadCount(int count)
{
    m_settings.setValue("defaultThreadCount", count);
}

int ConfigManager::batchSize() const
{
    return m_settings.value("batchSize", 10000).toInt();
}

void ConfigManager::setBatchSize(int size)
{
    m_settings.setValue("batchSize", size);
}

QString ConfigManager::lastImportPath() const
{
    return m_settings.value("lastImportPath", "").toString();
}

void ConfigManager::setLastImportPath(const QString &path)
{
    m_settings.setValue("lastImportPath", path);
}

QString ConfigManager::lastExportPath() const
{
    return m_settings.value("lastExportPath", "").toString();
}

void ConfigManager::setLastExportPath(const QString &path)
{
    m_settings.setValue("lastExportPath", path);
}

QString ConfigManager::rulesFilePath() const
{
    return m_settings.value("rulesFilePath", "rules.json").toString();
}

void ConfigManager::setRulesFilePath(const QString &path)
{
    m_settings.setValue("rulesFilePath", path);
}

void ConfigManager::sync()
{
    m_settings.sync();
}
