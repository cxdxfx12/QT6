#ifndef RULEMANAGER_H
#define RULEMANAGER_H

#include <QObject>
#include <QList>
#include <QMap>
#include "DataModel/PriceTable.h"

class RuleManager : public QObject {
    Q_OBJECT
public:
    explicit RuleManager(QObject *parent = nullptr);

    // 客户规则CRUD
    void addCustomerRule(const CustomerRule &rule);
    void updateCustomerRule(const QString &name, const CustomerRule &rule);
    void removeCustomerRule(const QString &name);
    CustomerRule getRule(const QString &name) const;
    QList<CustomerRule> allRules() const;
    QStringList ruleNames() const;
    QMap<QString, CustomerRule> rulesMap() const;
    void saveRule(const CustomerRule &rule);

    // 默认报价表
    void setDefaultPriceTable(const QList<PriceRule> &rules);
    QList<PriceRule> defaultPriceTable() const;

    // 全局规则
    GlobalRules globalRules() const;
    void setGlobalRules(const GlobalRules &rules);

    // 全局活动规则
    QList<ActivityRule> activityRules() const;
    void setActivityRules(const QList<ActivityRule> &rules);
    void addActivityRule(const ActivityRule &rule);
    void removeActivityRule(int index);

    // 全局临时加价规则
    QList<TempPriceIncrease> tempPriceIncreases() const;
    void setTempPriceIncreases(const QList<TempPriceIncrease> &rules);
    void addTempPriceIncrease(const TempPriceIncrease &rule);
    void removeTempPriceIncrease(int index);

    // 全局省份加价规则
    QList<ProvincePriceIncrease> provincePriceIncreases() const;
    void setProvincePriceIncreases(const QList<ProvincePriceIncrease> &rules);
    void addProvincePriceIncrease(const ProvincePriceIncrease &rule);
    void removeProvincePriceIncrease(int index);

    // 无重量默认价格
    double noWeightDefaultPrice() const;
    void setNoWeightDefaultPrice(double price);

    // 初始化默认报价表数据
    void initDefaultPriceTable();

    // 持久化
    bool saveToFile(const QString &filePath);
    bool loadFromFile(const QString &filePath);

    // 获取默认规则文件路径（应用数据目录下的 rules.json）
    static QString defaultFilePath();

signals:
    void rulesChanged();

private:
    QMap<QString, CustomerRule> m_rules;
    QList<PriceRule> m_defaultPriceTable;
    GlobalRules m_globalRules;
    QList<RegionMapping> m_regionMappings;

    void initRegionMappings();
};

#endif // RULEMANAGER_H
