#ifndef EXCELENGINE_H
#define EXCELENGINE_H

#include <QObject>
#include <QList>
#include <QMap>
#include <QVector>
#include "DataModel/OrderData.h"

namespace QXlsx { class Document; }

class ExcelEngine : public QObject {
    Q_OBJECT
public:
    explicit ExcelEngine(QObject *parent = nullptr);
    ~ExcelEngine();

    QList<OrderData> readExcel(const QString &filePath, const QMap<QString, int> &columnMapping);

    bool writeExcel(const QString &filePath, const QList<OrderData> &orders, const QStringList &headers);

    QStringList getHeaders(const QString &filePath);

    int countRowsFast(const QString &filePath);

signals:
    void progressUpdated(int percent);
    void errorOccurred(const QString &error);

private:
    static constexpr int BATCH_SIZE = 10000;
    static constexpr int MAX_THREADS = 8;

    OrderData parseRow(const QList<QVariant> &rowData, const QMap<QString, int> &columnMap);

    bool writeExcelFast(const QString &filePath, const QList<OrderData> &orders, const QStringList &headers);
};

#endif // EXCELENGINE_H
