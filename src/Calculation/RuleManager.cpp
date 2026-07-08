#include "RuleManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>

QString RuleManager::defaultFilePath()
{
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) {
        return QStringLiteral("rules.json");
    }
    QDir().mkpath(appDataDir);
    return appDataDir + QDir::separator() + QStringLiteral("rules.json");
}

RuleManager::RuleManager(QObject *parent)
    : QObject(parent)
{
    initRegionMappings();
}

// ===================== 客户规则CRUD =====================

void RuleManager::addCustomerRule(const CustomerRule &rule)
{
    m_rules.insert(rule.customerName, rule);
    emit rulesChanged();
}

void RuleManager::updateCustomerRule(const QString &name, const CustomerRule &rule)
{
    m_rules.insert(name, rule);
    emit rulesChanged();
}

void RuleManager::removeCustomerRule(const QString &name)
{
    m_rules.remove(name);
    emit rulesChanged();
}

CustomerRule RuleManager::getRule(const QString &name) const
{
    return m_rules.value(name);
}

QList<CustomerRule> RuleManager::allRules() const
{
    return m_rules.values();
}

QStringList RuleManager::ruleNames() const
{
    return m_rules.keys();
}

QMap<QString, CustomerRule> RuleManager::rulesMap() const
{
    return m_rules;
}

void RuleManager::saveRule(const CustomerRule &rule)
{
    m_rules.insert(rule.customerName, rule);
    emit rulesChanged();
}

// ===================== 默认报价表 =====================

void RuleManager::setDefaultPriceTable(const QList<PriceRule> &rules)
{
    m_defaultPriceTable = rules;
}

QList<PriceRule> RuleManager::defaultPriceTable() const
{
    return m_defaultPriceTable;
}

// ===================== 全局规则 =====================

GlobalRules RuleManager::globalRules() const
{
    return m_globalRules;
}

void RuleManager::setGlobalRules(const GlobalRules &rules)
{
    m_globalRules = rules;
    emit rulesChanged();
}

QList<ActivityRule> RuleManager::activityRules() const
{
    return m_globalRules.activityRules;
}

void RuleManager::setActivityRules(const QList<ActivityRule> &rules)
{
    m_globalRules.activityRules = rules;
    emit rulesChanged();
}

void RuleManager::addActivityRule(const ActivityRule &rule)
{
    m_globalRules.activityRules.append(rule);
    emit rulesChanged();
}

void RuleManager::removeActivityRule(int index)
{
    if (index >= 0 && index < m_globalRules.activityRules.size()) {
        m_globalRules.activityRules.removeAt(index);
        emit rulesChanged();
    }
}

QList<TempPriceIncrease> RuleManager::tempPriceIncreases() const
{
    return m_globalRules.tempPriceIncreases;
}

void RuleManager::setTempPriceIncreases(const QList<TempPriceIncrease> &rules)
{
    m_globalRules.tempPriceIncreases = rules;
    emit rulesChanged();
}

void RuleManager::addTempPriceIncrease(const TempPriceIncrease &rule)
{
    m_globalRules.tempPriceIncreases.append(rule);
    emit rulesChanged();
}

void RuleManager::removeTempPriceIncrease(int index)
{
    if (index >= 0 && index < m_globalRules.tempPriceIncreases.size()) {
        m_globalRules.tempPriceIncreases.removeAt(index);
        emit rulesChanged();
    }
}

QList<ProvincePriceIncrease> RuleManager::provincePriceIncreases() const
{
    return m_globalRules.provincePriceIncreases;
}

void RuleManager::setProvincePriceIncreases(const QList<ProvincePriceIncrease> &rules)
{
    m_globalRules.provincePriceIncreases = rules;
    emit rulesChanged();
}

void RuleManager::addProvincePriceIncrease(const ProvincePriceIncrease &rule)
{
    m_globalRules.provincePriceIncreases.append(rule);
    emit rulesChanged();
}

void RuleManager::removeProvincePriceIncrease(int index)
{
    if (index >= 0 && index < m_globalRules.provincePriceIncreases.size()) {
        m_globalRules.provincePriceIncreases.removeAt(index);
        emit rulesChanged();
    }
}

double RuleManager::noWeightDefaultPrice() const
{
    return m_globalRules.noWeightDefaultPrice;
}

void RuleManager::setNoWeightDefaultPrice(double price)
{
    m_globalRules.noWeightDefaultPrice = price;
    emit rulesChanged();
}

// ===================== 初始化默认报价表 =====================

void RuleManager::initDefaultPriceTable()
{
    m_defaultPriceTable.clear();

    // 辅助lambda：为一区的每个省份创建0-3kg基础段
    auto addBaseSegments = [](PriceRule &rule) {
        rule.segments.append({0, 0.5, 2.26, 0, false, false});
        rule.segments.append({0.5, 1, 2.46, 0, false, false});
        rule.segments.append({1, 2, 3.56, 0, false, false});
        rule.segments.append({2, 3, 4.76, 0, false, false});
    };

    // ---- 一区：江苏/浙江/安徽/上海 ----
    for (const QString &prov : QStringList{QStringLiteral("江苏"), QStringLiteral("浙江"),
                                            QStringLiteral("安徽"), QStringLiteral("上海")}) {
        PriceRule rule;
        rule.region = QStringLiteral("一区");
        rule.province = prov;
        rule.segments.append({0, 0.5, 2.26, 0, false, false});
        rule.segments.append({0.5, 1, 2.46, 0, false, false});
        rule.segments.append({1, 2, 3.56, 0, false, false});
        rule.segments.append({2, 3, 4.76, 0, false, false});
        rule.segments.append({3, 30, 3.76, 0.8, false, false});
        rule.segments.append({30, -1, 3.86, 0.8, false, false});
        m_defaultPriceTable.append(rule);
    }

    // ---- 二区：山东/广东/福建/北京/河南/湖北/湖南/江西/天津/河北 ----
    for (const QString &prov : QStringList{QStringLiteral("山东"), QStringLiteral("广东"), QStringLiteral("福建"),
                                            QStringLiteral("北京"), QStringLiteral("河南"), QStringLiteral("湖北"),
                                            QStringLiteral("湖南"), QStringLiteral("江西"), QStringLiteral("天津"),
                                            QStringLiteral("河北")}) {
        PriceRule rule;
        rule.region = QStringLiteral("二区");
        rule.province = prov;
        rule.segments.append({0, 0.5, 2.26, 0, false, false});
        rule.segments.append({0.5, 1, 2.46, 0, false, false});
        rule.segments.append({1, 2, 3.56, 0, false, false});
        rule.segments.append({2, 3, 4.76, 0, false, false});
        rule.segments.append({3, 30, 3.76, 1.1, false, false});
        rule.segments.append({30, -1, 4.06, 1.3, false, false});
        m_defaultPriceTable.append(rule);
    }

    // ---- 三区：山西/广西/四川/重庆/陕西/贵州/辽宁/吉林/黑龙江/云南 ----
    for (const QString &prov : QStringList{QStringLiteral("山西"), QStringLiteral("广西"), QStringLiteral("四川"),
                                            QStringLiteral("重庆"), QStringLiteral("陕西"), QStringLiteral("贵州"),
                                            QStringLiteral("辽宁"), QStringLiteral("吉林"), QStringLiteral("黑龙江"),
                                            QStringLiteral("云南")}) {
        PriceRule rule;
        rule.region = QStringLiteral("三区");
        rule.province = prov;
        rule.segments.append({0, 0.5, 2.26, 0, false, false});
        rule.segments.append({0.5, 1, 2.46, 0, false, false});
        rule.segments.append({1, 2, 3.56, 0, false, false});
        rule.segments.append({2, 3, 4.76, 0, false, false});
        rule.segments.append({3, 30, 3.76, 1.5, false, false});
        rule.segments.append({30, -1, 4.06, 1.6, false, false});
        m_defaultPriceTable.append(rule);
    }

    // ---- 四区：海南/甘肃/青海/内蒙古/宁夏 ----
    for (const QString &prov : QStringList{QStringLiteral("海南"), QStringLiteral("甘肃"), QStringLiteral("青海"),
                                            QStringLiteral("内蒙古"), QStringLiteral("宁夏")}) {
        PriceRule rule;
        rule.region = QStringLiteral("四区");
        rule.province = prov;
        rule.segments.append({0, 0.5, 2.56, 0, false, false});
        rule.segments.append({0.5, 1, 3.56, 0, false, false});
        rule.segments.append({1, 2, 4.06, 0, false, false});
        rule.segments.append({2, 3, 5.06, 0, false, false});
        rule.segments.append({3, 30, 3.76, 2.5, false, false});
        rule.segments.append({30, -1, 4.06, 4.3, false, false});
        m_defaultPriceTable.append(rule);
    }

    // ---- 五区：新疆、西藏 ----
    {
        PriceRule xinjiang;
        xinjiang.region = QStringLiteral("五区");
        xinjiang.province = QStringLiteral("新疆");
        xinjiang.segments.append({0, 0.5, 10, 0, false, false});
        xinjiang.segments.append({0.5, 1, 13, 0, false, false});
        xinjiang.segments.append({1, 2, 20, 0, false, false});
        xinjiang.segments.append({2, 3, 25, 0, false, false});
        xinjiang.segments.append({3, 30, 15, 15, false, false});
        xinjiang.segments.append({30, -1, 15, 15, false, false});
        m_defaultPriceTable.append(xinjiang);
    }
    {
        PriceRule xizang;
        xizang.region = QStringLiteral("五区");
        xizang.province = QStringLiteral("西藏");
        xizang.segments.append({0, 0.5, 13, 0, false, false});
        xizang.segments.append({0.5, 1, 15, 0, false, false});
        xizang.segments.append({1, 2, 25, 0, false, false});
        xizang.segments.append({2, 3, 30, 0, false, false});
        xizang.segments.append({3, 30, 15, 15, false, false});
        xizang.segments.append({30, -1, 15, 15, false, false});
        m_defaultPriceTable.append(xizang);
    }
}

// ===================== 区域映射初始化 =====================

void RuleManager::initRegionMappings()
{
    m_regionMappings.clear();

    m_regionMappings.append({QStringLiteral("一区"), {QStringLiteral("江苏"), QStringLiteral("浙江"),
                                                       QStringLiteral("安徽"), QStringLiteral("上海")}});
    m_regionMappings.append({QStringLiteral("二区"), {QStringLiteral("山东"), QStringLiteral("广东"), QStringLiteral("福建"),
                                                       QStringLiteral("北京"), QStringLiteral("河南"), QStringLiteral("湖北"),
                                                       QStringLiteral("湖南"), QStringLiteral("江西"), QStringLiteral("天津"),
                                                       QStringLiteral("河北")}});
    m_regionMappings.append({QStringLiteral("三区"), {QStringLiteral("山西"), QStringLiteral("广西"), QStringLiteral("四川"),
                                                       QStringLiteral("重庆"), QStringLiteral("陕西"), QStringLiteral("贵州"),
                                                       QStringLiteral("辽宁"), QStringLiteral("吉林"), QStringLiteral("黑龙江"),
                                                       QStringLiteral("云南")}});
    m_regionMappings.append({QStringLiteral("四区"), {QStringLiteral("海南"), QStringLiteral("甘肃"), QStringLiteral("青海"),
                                                       QStringLiteral("内蒙古"), QStringLiteral("宁夏")}});
    m_regionMappings.append({QStringLiteral("五区"), {QStringLiteral("新疆"), QStringLiteral("西藏")}});
}

// ===================== 持久化 =====================

namespace {

QJsonObject weightSegmentToJson(const WeightSegment &seg)
{
    QJsonObject obj;
    obj[QStringLiteral("minWeight")] = seg.minWeight;
    obj[QStringLiteral("maxWeight")] = seg.maxWeight;
    obj[QStringLiteral("firstWeightPrice")] = seg.firstWeightPrice;
    obj[QStringLiteral("additionalPrice")] = seg.additionalPrice;
    obj[QStringLiteral("isFullAdditional")] = seg.isFullAdditional;
    obj[QStringLiteral("isHundredGram")] = seg.isHundredGram;
    return obj;
}

WeightSegment weightSegmentFromJson(const QJsonObject &obj)
{
    WeightSegment seg;
    seg.minWeight = obj[QStringLiteral("minWeight")].toDouble(0);
    seg.maxWeight = obj[QStringLiteral("maxWeight")].toDouble(-1);
    seg.firstWeightPrice = obj[QStringLiteral("firstWeightPrice")].toDouble(0);
    seg.additionalPrice = obj[QStringLiteral("additionalPrice")].toDouble(0);
    seg.isFullAdditional = obj[QStringLiteral("isFullAdditional")].toBool(false);
    seg.isHundredGram = obj[QStringLiteral("isHundredGram")].toBool(false);
    return seg;
}

QJsonObject priceRuleToJson(const PriceRule &rule)
{
    QJsonObject obj;
    obj[QStringLiteral("region")] = rule.region;
    obj[QStringLiteral("province")] = rule.province;
    QJsonArray segArr;
    for (const WeightSegment &seg : rule.segments) {
        segArr.append(weightSegmentToJson(seg));
    }
    obj[QStringLiteral("segments")] = segArr;
    return obj;
}

PriceRule priceRuleFromJson(const QJsonObject &obj)
{
    PriceRule rule;
    rule.region = obj[QStringLiteral("region")].toString();
    rule.province = obj[QStringLiteral("province")].toString();
    QJsonArray segArr = obj[QStringLiteral("segments")].toArray();
    for (const QJsonValue &val : segArr) {
        rule.segments.append(weightSegmentFromJson(val.toObject()));
    }
    return rule;
}

QJsonObject activityRuleToJson(const ActivityRule &rule)
{
    QJsonObject obj;
    obj[QStringLiteral("name")] = rule.name;
    obj[QStringLiteral("startTime")] = rule.startTime.toString(Qt::ISODate);
    obj[QStringLiteral("endTime")] = rule.endTime.toString(Qt::ISODate);
    obj[QStringLiteral("increaseRate")] = rule.increaseRate;
    obj[QStringLiteral("increaseAmount")] = rule.increaseAmount;
    obj[QStringLiteral("isFixedAmount")] = rule.isFixedAmount;
    obj[QStringLiteral("isActive")] = rule.isActive;
    return obj;
}

ActivityRule activityRuleFromJson(const QJsonObject &obj)
{
    ActivityRule rule;
    rule.name = obj[QStringLiteral("name")].toString();
    rule.startTime = QDateTime::fromString(obj[QStringLiteral("startTime")].toString(), Qt::ISODate);
    rule.endTime = QDateTime::fromString(obj[QStringLiteral("endTime")].toString(), Qt::ISODate);
    rule.isActive = obj[QStringLiteral("isActive")].toBool(true);

    if (obj.contains(QStringLiteral("increaseRate"))) {
        rule.increaseRate = obj[QStringLiteral("increaseRate")].toDouble(0.0);
        rule.increaseAmount = obj[QStringLiteral("increaseAmount")].toDouble(0.0);
        rule.isFixedAmount = obj[QStringLiteral("isFixedAmount")].toBool(false);
    } else if (obj.contains(QStringLiteral("discountRate"))) {
        double discountRate = obj[QStringLiteral("discountRate")].toDouble(1.0);
        if (discountRate > 0 && discountRate != 1.0) {
            rule.increaseRate = discountRate - 1.0;
        } else {
            rule.increaseRate = 0.0;
        }
        rule.increaseAmount = 0.0;
        rule.isFixedAmount = false;
        if (obj[QStringLiteral("isFixedDiscount")].toBool(false)) {
            rule.increaseAmount = obj[QStringLiteral("fixedDiscount")].toDouble(0.0);
            rule.isFixedAmount = true;
            rule.increaseRate = 0.0;
        }
    }
    return rule;
}

QJsonObject tempPriceIncreaseToJson(const TempPriceIncrease &rule)
{
    QJsonObject obj;
    obj[QStringLiteral("name")] = rule.name;
    obj[QStringLiteral("startTime")] = rule.startTime.toString(Qt::ISODate);
    obj[QStringLiteral("endTime")] = rule.endTime.toString(Qt::ISODate);
    obj[QStringLiteral("increaseAmount")] = rule.increaseAmount;
    obj[QStringLiteral("increaseRate")] = rule.increaseRate;
    obj[QStringLiteral("isFixedAmount")] = rule.isFixedAmount;
    obj[QStringLiteral("isActive")] = rule.isActive;
    return obj;
}

TempPriceIncrease tempPriceIncreaseFromJson(const QJsonObject &obj)
{
    TempPriceIncrease rule;
    rule.name = obj[QStringLiteral("name")].toString();
    rule.startTime = QDateTime::fromString(obj[QStringLiteral("startTime")].toString(), Qt::ISODate);
    rule.endTime = QDateTime::fromString(obj[QStringLiteral("endTime")].toString(), Qt::ISODate);
    rule.increaseAmount = obj[QStringLiteral("increaseAmount")].toDouble(0);
    rule.increaseRate = obj[QStringLiteral("increaseRate")].toDouble(0);
    rule.isFixedAmount = obj[QStringLiteral("isFixedAmount")].toBool(true);
    rule.isActive = obj[QStringLiteral("isActive")].toBool(true);
    return rule;
}

QJsonObject provincePriceIncreaseToJson(const ProvincePriceIncrease &rule)
{
    QJsonObject obj;
    obj[QStringLiteral("province")] = rule.province;
    obj[QStringLiteral("increaseAmount")] = rule.increaseAmount;
    obj[QStringLiteral("isActive")] = rule.isActive;
    return obj;
}

ProvincePriceIncrease provincePriceIncreaseFromJson(const QJsonObject &obj)
{
    ProvincePriceIncrease rule;
    rule.province = obj[QStringLiteral("province")].toString();
    rule.increaseAmount = obj[QStringLiteral("increaseAmount")].toDouble(0);
    rule.isActive = obj[QStringLiteral("isActive")].toBool(true);
    return rule;
}

QJsonObject customerRuleToJson(const CustomerRule &rule)
{
    QJsonObject obj;
    obj[QStringLiteral("customerName")] = rule.customerName;
    obj[QStringLiteral("mappingHeader")] = rule.mappingHeader;
    obj[QStringLiteral("calculationMode")] = rule.calculationMode;
    obj[QStringLiteral("useDefaultPrice")] = rule.useDefaultPrice;

    QJsonArray customArr;
    for (const PriceRule &pr : rule.customPriceRules) {
        customArr.append(priceRuleToJson(pr));
    }
    obj[QStringLiteral("customPriceRules")] = customArr;

    return obj;
}

CustomerRule customerRuleFromJson(const QJsonObject &obj)
{
    CustomerRule rule;
    rule.customerName = obj[QStringLiteral("customerName")].toString();
    rule.mappingHeader = obj[QStringLiteral("mappingHeader")].toString();
    rule.calculationMode = obj[QStringLiteral("calculationMode")].toString();
    rule.useDefaultPrice = obj[QStringLiteral("useDefaultPrice")].toBool(true);

    QJsonArray customArr = obj[QStringLiteral("customPriceRules")].toArray();
    for (const QJsonValue &val : customArr) {
        rule.customPriceRules.append(priceRuleFromJson(val.toObject()));
    }

    return rule;
}

QJsonObject globalRulesToJson(const GlobalRules &rules)
{
    QJsonObject obj;
    obj[QStringLiteral("noWeightDefaultPrice")] = rules.noWeightDefaultPrice;

    QJsonArray activityArr;
    for (const ActivityRule &ar : rules.activityRules) {
        activityArr.append(activityRuleToJson(ar));
    }
    obj[QStringLiteral("activityRules")] = activityArr;

    QJsonArray tempArr;
    for (const TempPriceIncrease &tr : rules.tempPriceIncreases) {
        tempArr.append(tempPriceIncreaseToJson(tr));
    }
    obj[QStringLiteral("tempPriceIncreases")] = tempArr;

    QJsonArray provArr;
    for (const ProvincePriceIncrease &pr : rules.provincePriceIncreases) {
        provArr.append(provincePriceIncreaseToJson(pr));
    }
    obj[QStringLiteral("provincePriceIncreases")] = provArr;

    // 拉均重配置
    obj[QStringLiteral("avgWeightBase")] = rules.avgWeightBase;
    obj[QStringLiteral("avgWeightUpperLimit")] = rules.avgWeightUpperLimit;
    obj[QStringLiteral("avgWeightIncrement")] = rules.avgWeightIncrement;
    obj[QStringLiteral("avgWeightSurcharge")] = rules.avgWeightSurcharge;

    return obj;
}

GlobalRules globalRulesFromJson(const QJsonObject &obj)
{
    GlobalRules rules;
    rules.noWeightDefaultPrice = obj[QStringLiteral("noWeightDefaultPrice")].toDouble(0);

    QJsonArray activityArr = obj[QStringLiteral("activityRules")].toArray();
    for (const QJsonValue &val : activityArr) {
        rules.activityRules.append(activityRuleFromJson(val.toObject()));
    }

    QJsonArray tempArr = obj[QStringLiteral("tempPriceIncreases")].toArray();
    for (const QJsonValue &val : tempArr) {
        rules.tempPriceIncreases.append(tempPriceIncreaseFromJson(val.toObject()));
    }

    QJsonArray provArr = obj[QStringLiteral("provincePriceIncreases")].toArray();
    for (const QJsonValue &val : provArr) {
        rules.provincePriceIncreases.append(provincePriceIncreaseFromJson(val.toObject()));
    }

    // 拉均重配置
    rules.avgWeightBase = obj[QStringLiteral("avgWeightBase")].toDouble(0.5);
    rules.avgWeightUpperLimit = obj[QStringLiteral("avgWeightUpperLimit")].toDouble(1.0);
    rules.avgWeightIncrement = obj[QStringLiteral("avgWeightIncrement")].toDouble(0.1);
    rules.avgWeightSurcharge = obj[QStringLiteral("avgWeightSurcharge")].toDouble(0.1);

    return rules;
}

} // anonymous namespace

bool RuleManager::saveToFile(const QString &filePath)
{
    QJsonObject root;

    // 保存默认报价表
    QJsonArray priceArr;
    for (const PriceRule &pr : m_defaultPriceTable) {
        priceArr.append(priceRuleToJson(pr));
    }
    root[QStringLiteral("defaultPriceTable")] = priceArr;

    // 保存全局规则
    root[QStringLiteral("globalRules")] = globalRulesToJson(m_globalRules);

    // 保存客户规则
    QJsonArray rulesArr;
    for (const CustomerRule &rule : m_rules) {
        rulesArr.append(customerRuleToJson(rule));
    }
    root[QStringLiteral("customerRules")] = rulesArr;

    // 保存区域映射
    QJsonArray regionArr;
    for (const RegionMapping &rm : m_regionMappings) {
        QJsonObject obj;
        obj[QStringLiteral("region")] = rm.region;
        obj[QStringLiteral("provinces")] = QJsonArray::fromStringList(rm.provinces);
        regionArr.append(obj);
    }
    root[QStringLiteral("regionMappings")] = regionArr;

    QJsonDocument doc(root);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    return true;
}

bool RuleManager::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();

    if (error.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    QJsonObject root = doc.object();

    // 加载默认报价表
    m_defaultPriceTable.clear();
    QJsonArray priceArr = root[QStringLiteral("defaultPriceTable")].toArray();
    for (const QJsonValue &val : priceArr) {
        m_defaultPriceTable.append(priceRuleFromJson(val.toObject()));
    }

    // 加载全局规则
    m_globalRules = globalRulesFromJson(root[QStringLiteral("globalRules")].toObject());

    // 加载客户规则
    m_rules.clear();
    QJsonArray rulesArr = root[QStringLiteral("customerRules")].toArray();
    for (const QJsonValue &val : rulesArr) {
        CustomerRule rule = customerRuleFromJson(val.toObject());
        m_rules.insert(rule.customerName, rule);
    }

    // 加载区域映射
    m_regionMappings.clear();
    QJsonArray regionArr = root[QStringLiteral("regionMappings")].toArray();
    for (const QJsonValue &val : regionArr) {
        QJsonObject obj = val.toObject();
        RegionMapping rm;
        rm.region = obj[QStringLiteral("region")].toString();
        QJsonArray provArr = obj[QStringLiteral("provinces")].toArray();
        for (const QJsonValue &pv : provArr) {
            rm.provinces.append(pv.toString());
        }
        m_regionMappings.append(rm);
    }

    emit rulesChanged();
    return true;
}
