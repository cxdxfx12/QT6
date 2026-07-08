#ifndef ORDERDATA_H
#define ORDERDATA_H

#include <QDate>
#include <QString>

struct OrderData {
    QDate businessTime;           // 业务时间（只保留年月日）
    QString waybillNo;            // 运单号
    double volumetricWeight = 0;  // 体积重 (kg)
    QString customer;             // 订单客户
    QString client;               // 结算对象/客户
    QString destinationProvince;  // 目的省份
    double actualWeight = 0;      // 实际重量 (kg)
    double freight = 0;           // 计算出的运费 (结果)

    // 辅助字段
    QString usedRule;             // 使用的规则（匹配到的价格规则名 + 叠加规则）
    QString errorMessage;         // 错误信息
    bool isValid = false;         // 数据有效性

    // 日期辅助
    int year() const { return businessTime.year(); }
    int month() const { return businessTime.month(); }
    int day() const { return businessTime.day(); }
};

#endif // ORDERDATA_H
