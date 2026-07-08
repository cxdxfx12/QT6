#ifndef EXCELIMPORTER_H
#define EXCELIMPORTER_H

#include <QObject>
#include <QList>
#include <QMap>
#include "DataModel/OrderData.h"

class ExcelEngine;

class ExcelImporter : public QObject {
    Q_OBJECT
public:
    explicit ExcelImporter(QObject *parent = nullptr);

    // 导入Excel（自动列映射）
    QList<OrderData> importFromFile(const QString &filePath);

    // 导入Excel（自定义列映射）
    QList<OrderData> importFromFile(const QString &filePath, const QMap<QString, int> &columnMapping);

    QStringList getAvailableHeaders(const QString &filePath);

signals:
    void progressUpdated(int percent);
    void errorOccurred(const QString &error);

private:
    ExcelEngine *m_engine;
    QMap<QString, int> autoDetectColumns(const QStringList &headers);
};

#endif // EXCELIMPORTER_H
