#ifndef FREIGHTCALCULATOR_H
#define FREIGHTCALCULATOR_H

#include <QObject>
#include <QList>
#include <QHash>
#include <QAtomicInt>
#include "DataModel/OrderData.h"
#include "DataModel/PriceTable.h"

class FreightCalculator : public QObject {
    Q_OBJECT
public:
    explicit FreightCalculator(QObject *parent = nullptr);

    double calculateFreight(const OrderData &order, const CustomerRule &rule);

    void calculateBatch(QList<OrderData> &orders, const CustomerRule &rule, int threadCount = 8);

    void setDefaultPriceTable(const QList<PriceRule> &rules);

    QList<PriceRule> defaultPriceTable() const;

    void setGlobalRules(const GlobalRules &rules);
    GlobalRules globalRules() const;

    double quickCalculate(double weight, const QString &province, const CustomerRule &rule, const QDate &time = QDate());

    void buildRuleCache(const CustomerRule &rule);

signals:
    void progressUpdated(int percent);
    void calculationComplete(int totalCount, int errorCount);

private:
    QList<PriceRule> m_defaultPriceTable;
    GlobalRules m_globalRules;
    static constexpr int BATCH_SIZE = 10000;

    QHash<QString, PriceRule> m_cachedCustomRules;
    QHash<QString, PriceRule> m_cachedDefaultRules;
    QString m_lastCachedCustomer;
    bool m_defaultCacheBuilt = false;

    PriceRule findPriceRule(const QString &province, const CustomerRule &rule);

    static QString normalizeProvince(const QString &province);

    double calculateFreightDetail(const OrderData &order, const CustomerRule &rule, QString &outRuleDesc);
};

#endif // FREIGHTCALCULATOR_H
