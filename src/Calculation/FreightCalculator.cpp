#include "FreightCalculator.h"
#include "ThreadPool.h"
#include "DataModel/CalculationRule.h"
#include <QtConcurrent>
#include <QFuture>
#include <QFutureWatcher>
#include <cmath>
#include <thread>
#include <vector>

FreightCalculator::FreightCalculator(QObject *parent)
    : QObject(parent)
{
}

QString FreightCalculator::normalizeProvince(const QString &province)
{
    QString s = province;
    static const QStringList suffixes = {
        QStringLiteral("省"), QStringLiteral("市"),
        QStringLiteral("自治区"), QStringLiteral("特别行政区")
    };
    for (const QString &sf : suffixes) {
        if (s.endsWith(sf)) {
            s.chop(sf.size());
            break;
        }
    }
    return s.trimmed();
}

// 两字前缀模糊匹配：前两字相同则视为匹配
// 注意：调用前已通过第一轮精确匹配和第二轮contains匹配，此函数仅做前缀匹配
static bool fuzzyProvinceMatch(const QString &a, const QString &b)
{
    if (a.size() >= 2 && b.size() >= 2 && a.left(2) == b.left(2)) return true;
    return false;
}

void FreightCalculator::buildRuleCache(const CustomerRule &rule)
{
    if (!m_defaultCacheBuilt) {
        m_cachedDefaultRules.clear();
        for (const PriceRule &pr : m_defaultPriceTable) {
            QString key = normalizeProvince(pr.province);
            if (!key.isEmpty())
                m_cachedDefaultRules.insert(key, pr);
        }
        m_defaultCacheBuilt = true;
    }

    if (m_lastCachedCustomer != rule.customerName) {
        m_cachedCustomRules.clear();
        for (const PriceRule &pr : rule.customPriceRules) {
            QString key = normalizeProvince(pr.province);
            if (!key.isEmpty())
                m_cachedCustomRules.insert(key, pr);
        }
        m_lastCachedCustomer = rule.customerName;
    }
}

double FreightCalculator::calculateFreight(const OrderData &order, const CustomerRule &rule)
{
    QString unused;
    return calculateFreightDetail(order, rule, unused);
}

double FreightCalculator::calculateFreightDetail(const OrderData &order, const CustomerRule &rule, QString &outRuleDesc)
{
    outRuleDesc.clear();

    CalculationRule::Mode mode = CalculationRule::modeFromString(rule.calculationMode);
    double effectiveWeight = CalculationRule::getEffectiveWeight(order.actualWeight, order.volumetricWeight, mode);

    if (effectiveWeight <= 0 && m_globalRules.noWeightDefaultPrice > 0) {
        double result = m_globalRules.noWeightDefaultPrice;
        result = CalculationRule::applyActivityPriceIncrease(result, m_globalRules.activityRules, order.businessTime);
        result = CalculationRule::applyTempPriceIncrease(result, m_globalRules.tempPriceIncreases, order.businessTime);
        result = CalculationRule::applyProvincePriceIncrease(result, m_globalRules.provincePriceIncreases, order.destinationProvince);
        result = std::round(result * 100.0) / 100.0;
        outRuleDesc = QStringLiteral("无重量默认价");
        return result;
    }

    if (effectiveWeight <= 0) {
        return 0.0;
    }

    PriceRule priceRule = findPriceRule(order.destinationProvince, rule);

    CalculationRule::Mode calcMode = CalculationRule::modeFromString(rule.calculationMode);

    // 判断是否为客户自定义规则
    bool isCustomRule = false;
    for (const PriceRule &pr : rule.customPriceRules) {
        if (pr.province == priceRule.province && pr.region == priceRule.region) {
            isCustomRule = true;
            break;
        }
    }

    double freight = 0.0;
    bool matched = false;
    for (const WeightSegment &seg : priceRule.segments) {
        if (CalculationRule::isWeightInRange(effectiveWeight, seg)) {
            if (calcMode == CalculationRule::Mode::FullAdditional) {
                freight = CalculationRule::calculateFullAdditional(effectiveWeight, priceRule.segments);
            } else if (calcMode == CalculationRule::Mode::HundredGram) {
                freight = CalculationRule::calculateHundredGram(effectiveWeight, seg);
            } else if (seg.isFullAdditional) {
                freight = CalculationRule::calculateFullAdditional(effectiveWeight, priceRule.segments);
            } else if (seg.isHundredGram) {
                freight = CalculationRule::calculateHundredGram(effectiveWeight, seg);
            } else {
                freight = CalculationRule::calculateStandard(effectiveWeight, seg);
            }
            matched = true;
            break;
        }
    }

    if (!matched) {
        return 0.0;
    }

    // 构建规则描述
    QString ruleDesc;
    if (isCustomRule && !rule.customerName.isEmpty()) {
        ruleDesc = rule.customerName + QStringLiteral("-");
    } else {
        ruleDesc = QStringLiteral("默认报价-");
    }
    ruleDesc += priceRule.province;
    if (!priceRule.region.isEmpty())
        ruleDesc += QStringLiteral("(%1)").arg(priceRule.region);

    // 活动加价
    QDateTime bizTime = QDateTime(order.businessTime, QTime(0, 0));
    for (const ActivityRule &ar : m_globalRules.activityRules) {
        if (ar.isInRange(bizTime)) {
            ruleDesc += QStringLiteral("+%1").arg(ar.name.isEmpty() ? QStringLiteral("活动加价") : ar.name);
        }
    }
    freight = CalculationRule::applyActivityPriceIncrease(freight, m_globalRules.activityRules, order.businessTime);
    freight = CalculationRule::applyTempPriceIncrease(freight, m_globalRules.tempPriceIncreases, order.businessTime);
    freight = CalculationRule::applyProvincePriceIncrease(freight, m_globalRules.provincePriceIncreases, order.destinationProvince);

    // 临时加价
    for (const TempPriceIncrease &tpi : m_globalRules.tempPriceIncreases) {
        if (tpi.isInRange(bizTime)) {
            ruleDesc += QStringLiteral("+%1").arg(tpi.name.isEmpty() ? QStringLiteral("临时加价") : tpi.name);
        }
    }

    // 省份加价
    QString normOrderProv = order.destinationProvince;
    static const QStringList suffixes2 = {
        QStringLiteral("省"), QStringLiteral("市"),
        QStringLiteral("自治区"), QStringLiteral("特别行政区")
    };
    for (const QString &sf : suffixes2) {
        if (normOrderProv.endsWith(sf)) {
            normOrderProv.chop(sf.size());
            break;
        }
    }
    normOrderProv = normOrderProv.trimmed();

    for (const ProvincePriceIncrease &ppi : m_globalRules.provincePriceIncreases) {
        if (!ppi.isActive) continue;
        QString ruleProv = ppi.province;
        for (const QString &sf : suffixes2) {
            if (ruleProv.endsWith(sf)) {
                ruleProv.chop(sf.size());
                break;
            }
        }
        ruleProv = ruleProv.trimmed();
        if (ruleProv == normOrderProv || ppi.province == order.destinationProvince) {
            ruleDesc += QStringLiteral("+省份加价");
            // 不 break：与活动加价/临时加价描述方式一致
        }
    }

    // 拉均重加价
    if (m_globalRules.runtimeAvgSurcharge > 0 && calcMode == CalculationRule::Mode::AverageWeight) {
        freight += m_globalRules.runtimeAvgSurcharge;
        ruleDesc += QStringLiteral("+拉均重加价%1").arg(m_globalRules.runtimeAvgSurcharge, 0, 'f', 2);
    }

    freight = std::round(freight * 100.0) / 100.0;

    outRuleDesc = ruleDesc;
    return freight;
}

void FreightCalculator::calculateBatch(QList<OrderData> &orders, const CustomerRule &rule, int threadCount)
{
    if (orders.isEmpty())
        return;

    int total = orders.size();
    QAtomicInt progress(0);
    QAtomicInt errorCount(0);

    buildRuleCache(rule);

    int effectiveThreads = qMax(1, qMin(threadCount, QThread::idealThreadCount()));
    int chunkSize = qMax(1000, total / effectiveThreads);
    int batches = (total + chunkSize - 1) / chunkSize;

    QVector<QFuture<void>> futures;
    futures.reserve(batches);

    OrderData *dataPtr = orders.data();

    for (int i = 0; i < batches; ++i) {
        int start = i * chunkSize;
        int end = qMin(start + chunkSize, total);

        futures.append(QtConcurrent::run([&, start, end]() {
            for (int idx = start; idx < end; ++idx) {
                OrderData &order = dataPtr[idx];
                QString ruleDesc;
                double freight = calculateFreightDetail(order, rule, ruleDesc);
                order.freight = freight;
                order.usedRule = ruleDesc;
                if (freight <= 0 && order.actualWeight > 0) {
                    order.isValid = false;
                    order.errorMessage = QStringLiteral("计算结果为0，可能未匹配到价格规则");
                    errorCount.fetchAndAddRelaxed(1);
                } else {
                    order.isValid = true;
                }

                int done = progress.fetchAndAddRelaxed(1) + 1;
                if (done % 2000 == 0 || done == total) {
                    int pct = static_cast<int>(done * 100.0 / total);
                    emit progressUpdated(qMin(99, pct));
                }
            }
        }));
    }

    for (auto &f : futures)
        f.waitForFinished();

    emit progressUpdated(100);
    emit calculationComplete(total, errorCount.loadAcquire());
}

void FreightCalculator::setDefaultPriceTable(const QList<PriceRule> &rules)
{
    m_defaultPriceTable = rules;
    m_defaultCacheBuilt = false;
}

QList<PriceRule> FreightCalculator::defaultPriceTable() const
{
    return m_defaultPriceTable;
}

void FreightCalculator::setGlobalRules(const GlobalRules &rules)
{
    m_globalRules = rules;
}

GlobalRules FreightCalculator::globalRules() const
{
    return m_globalRules;
}

double FreightCalculator::quickCalculate(double weight, const QString &province, const CustomerRule &rule, const QDate &time)
{
    OrderData tempOrder;
    tempOrder.actualWeight = weight;
    tempOrder.volumetricWeight = weight;
    tempOrder.destinationProvince = province;
    tempOrder.businessTime = time.isValid() ? time : QDate::currentDate();
    tempOrder.isValid = true;

    return calculateFreight(tempOrder, rule);
}

PriceRule FreightCalculator::findPriceRule(const QString &province, const CustomerRule &rule)
{
    QString normalized = normalizeProvince(province);

    if (m_lastCachedCustomer == rule.customerName && !m_cachedCustomRules.isEmpty()) {
        auto it = m_cachedCustomRules.constFind(normalized);
        if (it != m_cachedCustomRules.constEnd())
            return it.value();
    }

    if (!rule.useDefaultPrice || !rule.customPriceRules.isEmpty()) {
        // 第一轮: 精确匹配
        for (const PriceRule &pr : rule.customPriceRules) {
            QString ruleProvince = normalizeProvince(pr.province);
            if (ruleProvince == normalized) {
                return pr;
            }
        }
        // 第二轮: contains模糊匹配
        for (const PriceRule &pr : rule.customPriceRules) {
            QString ruleProvince = normalizeProvince(pr.province);
            if (normalized.contains(ruleProvince) || ruleProvince.contains(normalized)) {
                return pr;
            }
        }
        // 第三轮: 两字前缀模糊匹配
        for (const PriceRule &pr : rule.customPriceRules) {
            QString ruleProvince = normalizeProvince(pr.province);
            if (fuzzyProvinceMatch(normalized, ruleProvince)) {
                return pr;
            }
        }
    }

    if (m_defaultCacheBuilt && !m_cachedDefaultRules.isEmpty()) {
        auto it = m_cachedDefaultRules.constFind(normalized);
        if (it != m_cachedDefaultRules.constEnd())
            return it.value();
    }

    // 第一轮: 精确匹配
    for (const PriceRule &pr : m_defaultPriceTable) {
        QString ruleProvince = normalizeProvince(pr.province);
        if (ruleProvince == normalized) {
            return pr;
        }
    }
    // 第二轮: contains模糊匹配
    for (const PriceRule &pr : m_defaultPriceTable) {
        QString ruleProvince = normalizeProvince(pr.province);
        if (normalized.contains(ruleProvince) || ruleProvince.contains(normalized)) {
            return pr;
        }
    }
    // 第三轮: 两字前缀模糊匹配
    for (const PriceRule &pr : m_defaultPriceTable) {
        QString ruleProvince = normalizeProvince(pr.province);
        if (fuzzyProvinceMatch(normalized, ruleProvince)) {
            return pr;
        }
    }

    if (!m_defaultPriceTable.isEmpty()) {
        return m_defaultPriceTable.first();
    }

    return PriceRule();
}
