#include "ExcelExporter.h"
#include "ExcelEngine.h"
#include "xlsxdocument.h"

#include <QMap>

ExcelExporter::ExcelExporter(QObject *parent)
    : QObject(parent)
    , m_engine(new ExcelEngine(this))
{
    connect(m_engine, &ExcelEngine::progressUpdated, this, &ExcelExporter::progressUpdated);
    connect(m_engine, &ExcelEngine::errorOccurred, this, &ExcelExporter::errorOccurred);
}

bool ExcelExporter::exportToFile(const QString &filePath, const QList<OrderData> &orders)
{
    static const QStringList defaultHeaders = {
        "运单号", "业务时间", "体积重", "订单客户", "客户",
        "目的省份", "实际重量", "运费", "使用的规则", "错误信息"
    };

    return m_engine->writeExcel(filePath, orders, defaultHeaders);
}

bool ExcelExporter::exportSummary(const QString &filePath, const QList<OrderData> &orders)
{
    if (orders.isEmpty()) {
        emit errorOccurred(tr("无数据可导出"));
        return false;
    }

    struct SummaryData {
        int count = 0;
        double totalFreight = 0;
        double totalVolumetricWeight = 0;
        double totalActualWeight = 0;
        int validCount = 0;
        int invalidCount = 0;
    };

    // 按 (客户, 店铺) 分组统计
    struct SummaryKey {
        QString client;
        QString shop;
        bool operator<(const SummaryKey &other) const {
            if (client != other.client) return client < other.client;
            return shop < other.shop;
        }
    };

    QMap<SummaryKey, SummaryData> summaryMap;
    for (const OrderData &order : orders) {
        QString clientName = order.client.isEmpty() ? order.customer : order.client;
        QString shopName = order.customer;
        SummaryKey key{clientName, shopName};
        SummaryData &data = summaryMap[key];
        data.count++;
        data.totalFreight += order.freight;
        data.totalVolumetricWeight += order.volumetricWeight;
        data.totalActualWeight += order.actualWeight;
        if (order.isValid)
            data.validCount++;
        else
            data.invalidCount++;
    }

    // 汇总表表头
    QStringList summaryHeaders = {
        "客户", "店铺", "总条数", "有效条数", "无效条数",
        "总运费(元)", "平均运费(元)", "总体积重(kg)", "平均体积重(kg)",
        "总实际重量(kg)", "平均实际重量(kg)"
    };

    QXlsx::Document xlsx;

    // 写表头
    for (int c = 0; c < summaryHeaders.size(); ++c) {
        xlsx.write(1, c + 1, summaryHeaders[c]);
    }

    // 写数据行
    int row = 2;
    for (auto it = summaryMap.constBegin(); it != summaryMap.constEnd(); ++it) {
        const SummaryData &data = it.value();
        double avgFreight = data.count > 0 ? data.totalFreight / data.count : 0;
        double avgVolWeight = data.count > 0 ? data.totalVolumetricWeight / data.count : 0;
        double avgActWeight = data.count > 0 ? data.totalActualWeight / data.count : 0;

        xlsx.write(row, 1, it.key().client);
        xlsx.write(row, 2, it.key().shop);
        xlsx.write(row, 3, data.count);
        xlsx.write(row, 4, data.validCount);
        xlsx.write(row, 5, data.invalidCount);
        xlsx.write(row, 6, data.totalFreight);
        xlsx.write(row, 7, avgFreight);
        xlsx.write(row, 8, data.totalVolumetricWeight);
        xlsx.write(row, 9, avgVolWeight);
        xlsx.write(row, 10, data.totalActualWeight);
        xlsx.write(row, 11, avgActWeight);
        row++;
    }

    // 写总计行
    SummaryData totalData;
    for (const OrderData &order : orders) {
        totalData.count++;
        totalData.totalFreight += order.freight;
        totalData.totalVolumetricWeight += order.volumetricWeight;
        totalData.totalActualWeight += order.actualWeight;
        if (order.isValid)
            totalData.validCount++;
        else
            totalData.invalidCount++;
    }
    double totalAvgFreight = totalData.count > 0 ? totalData.totalFreight / totalData.count : 0;
    double totalAvgVolWeight = totalData.count > 0 ? totalData.totalVolumetricWeight / totalData.count : 0;
    double totalAvgActWeight = totalData.count > 0 ? totalData.totalActualWeight / totalData.count : 0;

    xlsx.write(row, 1, "总计");
    xlsx.write(row, 2, "");
    xlsx.write(row, 3, totalData.count);
    xlsx.write(row, 4, totalData.validCount);
    xlsx.write(row, 5, totalData.invalidCount);
    xlsx.write(row, 6, totalData.totalFreight);
    xlsx.write(row, 7, totalAvgFreight);
    xlsx.write(row, 8, totalData.totalVolumetricWeight);
    xlsx.write(row, 9, totalAvgVolWeight);
    xlsx.write(row, 10, totalData.totalActualWeight);
    xlsx.write(row, 11, totalAvgActWeight);

    if (!xlsx.saveAs(filePath)) {
        emit errorOccurred(tr("保存汇总文件失败: %1").arg(filePath));
        return false;
    }

    emit progressUpdated(100);
    return true;
}
