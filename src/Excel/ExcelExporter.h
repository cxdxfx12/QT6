#ifndef EXCELEXPORTER_H
#define EXCELEXPORTER_H

#include <QObject>
#include <QList>
#include "DataModel/OrderData.h"

class ExcelEngine;

class ExcelExporter : public QObject {
    Q_OBJECT
public:
    explicit ExcelExporter(QObject *parent = nullptr);

    bool exportToFile(const QString &filePath, const QList<OrderData> &orders);
    bool exportSummary(const QString &filePath, const QList<OrderData> &orders);

signals:
    void progressUpdated(int percent);
    void errorOccurred(const QString &error);

private:
    ExcelEngine *m_engine;
};

#endif // EXCELEXPORTER_H
