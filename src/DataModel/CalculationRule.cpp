#include "CalculationRule.h"
#include <cmath>

CalculationRule::Mode CalculationRule::modeFromString(const QString &str) {
    if (str == QStringLiteral("实际重量")) return Mode::ActualWeight;
    if (str == QStringLiteral("拉均重"))   return Mode::AverageWeight;
    if (str == QStringLiteral("百克续"))   return Mode::HundredGram;
    if (str == QStringLiteral("全续"))     return Mode::FullAdditional;
    return Mode::ActualWeight;
}

QString CalculationRule::modeToString(CalculationRule::Mode mode) {
    switch (mode) {
    case Mode::ActualWeight:     return QStringLiteral("实际重量");
    case Mode::AverageWeight:    return QStringLiteral("拉均重");
    case Mode::HundredGram:      return QStringLiteral("百克续");
    case Mode::FullAdditional:   return QStringLiteral("全续");
    }
    return QStringLiteral("实际重量");
}

QStringList CalculationRule::allModes() {
    return {QStringLiteral("实际重量"), QStringLiteral("拉均重"),
            QStringLiteral("百克续"), QStringLiteral("全续")};
}

double CalculationRule::getEffectiveWeight(double actualWeight, double volumetricWeight, Mode mode) {
    double maxWeight = std::max(actualWeight, volumetricWeight);
    switch (mode) {
    case Mode::ActualWeight:     return maxWeight;
    case Mode::AverageWeight:    return maxWeight;
    case Mode::HundredGram:      return maxWeight;
    case Mode::FullAdditional:   return maxWeight;
    }
    return maxWeight;
}

bool CalculationRule::isWeightInRange(double weight, const WeightSegment &segment) {
    if (weight <= segment.minWeight) return false;
    if (segment.maxWeight < 0) return true;
    return weight <= segment.maxWeight;
}

double CalculationRule::calculateStandard(double weight, const WeightSegment &segment) {
    double firstWeightPrice = segment.firstWeightPrice;
    double additionalPrice = segment.additionalPrice;
    double firstWeight = segment.minWeight;

    if (segment.maxWeight < 0 && segment.minWeight >= 30) {
        firstWeight = 3.0;
    }

    double extraWeight = weight - firstWeight;
    if (extraWeight < 0) extraWeight = 0;
    return firstWeightPrice + extraWeight * additionalPrice;
}

double CalculationRule::calculateHundredGram(double weight, const WeightSegment &segment) {
    double minWeight = segment.minWeight;
    double firstWeightPrice = segment.firstWeightPrice;
    double additionalPrice = segment.additionalPrice;

    double extraWeight = weight - minWeight;
    int hundredGramUnits = static_cast<int>(std::ceil(extraWeight * 10));
    if (hundredGramUnits < 0) hundredGramUnits = 0;
    return firstWeightPrice + hundredGramUnits * (additionalPrice / 10.0);
}

double CalculationRule::calculateFullAdditional(double weight, const WeightSegment &segment) {
    double fullWeight = std::ceil(weight);
    return calculateStandard(fullWeight, segment);
}

double CalculationRule::calculateFullAdditional(double weight, const QList<WeightSegment> &segments) {
    if (segments.isEmpty()) return 0.0;

    double fullWeight = std::ceil(weight);

    const WeightSegment* matchedSeg = nullptr;
    double maxFirstWeightPrice = 0.0;

    for (const WeightSegment& seg : segments) {
        if (seg.firstWeightPrice > maxFirstWeightPrice) {
            maxFirstWeightPrice = seg.firstWeightPrice;
        }
        if (isWeightInRange(fullWeight, seg)) {
            matchedSeg = &seg;
            break;
        }
    }

    if (!matchedSeg) return 0.0;

    double result = calculateStandard(fullWeight, *matchedSeg);

    if (result < maxFirstWeightPrice) {
        result = maxFirstWeightPrice;
    }

    return result;
}

double CalculationRule::applyActivityPriceIncrease(double freight, const QList<ActivityRule> &rules, const QDate &date) {
    double result = freight;
    QDateTime time = QDateTime(date, QTime(0, 0));
    for (const ActivityRule &rule : rules) {
        if (rule.isInRange(time)) {
            if (rule.isFixedAmount) {
                result += rule.increaseAmount;
            } else {
                result *= (1.0 + rule.increaseRate);
            }
        }
    }
    return result;
}

double CalculationRule::applyTempPriceIncrease(double freight, const QList<TempPriceIncrease> &rules, const QDate &date) {
    double result = freight;
    QDateTime time = QDateTime(date, QTime(0, 0));
    for (const TempPriceIncrease &rule : rules) {
        if (rule.isInRange(time)) {
            if (rule.isFixedAmount) {
                result += rule.increaseAmount;
            } else {
                result *= (1.0 + rule.increaseRate);
            }
        }
    }
    return result;
}

double CalculationRule::applyProvincePriceIncrease(double freight, const QList<ProvincePriceIncrease> &rules, const QString &province) {
    double result = freight;
    QString normalized = province;
    static const QStringList suffixes = {
        QStringLiteral("省"), QStringLiteral("市"),
        QStringLiteral("自治区"), QStringLiteral("特别行政区")
    };
    for (const QString &sf : suffixes) {
        if (normalized.endsWith(sf)) {
            normalized.chop(sf.size());
            break;
        }
    }
    normalized = normalized.trimmed();

    for (const ProvincePriceIncrease &rule : rules) {
        if (!rule.isActive) continue;
        QString ruleProv = rule.province;
        for (const QString &sf : suffixes) {
            if (ruleProv.endsWith(sf)) {
                ruleProv.chop(sf.size());
                break;
            }
        }
        ruleProv = ruleProv.trimmed();
        if (ruleProv == normalized || rule.province == province) {
            result += rule.increaseAmount;
        }
    }
    return result;
}
