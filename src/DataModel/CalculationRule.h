#ifndef CALCULATIONRULE_H
#define CALCULATIONRULE_H

#include <QString>
#include <QList>
#include "PriceTable.h"

// 计算规则辅助工具类
class CalculationRule {
public:
    // 计算模式枚举
    enum class Mode {
        ActualWeight,    // 实际重量
        AverageWeight,   // 拉均重
        HundredGram,     // 百克续
        FullAdditional   // 全续
    };

    static Mode modeFromString(const QString &str);
    static QString modeToString(Mode mode);
    static QStringList allModes();

    // 获取有效重量
    static double getEffectiveWeight(double actualWeight, double volumetricWeight, Mode mode);

    // 判断重量段是否匹配
    static bool isWeightInRange(double weight, const WeightSegment &segment);

    // 标准计算
    static double calculateStandard(double weight, const WeightSegment &segment);

    // 百克续计算
    static double calculateHundredGram(double weight, const WeightSegment &segment);

    // 全续计算（单个重量段版本，保留向后兼容）
    static double calculateFullAdditional(double weight, const WeightSegment &segment);

    // 全续计算（完整重量段列表版本，确保价格单调递增）
    // 最低收费取匹配段及之前所有段的最大首重价
    static double calculateFullAdditional(double weight, const QList<WeightSegment> &segments);

    // 应用活动加价规则
    static double applyActivityPriceIncrease(double freight, const QList<ActivityRule> &rules, const QDate &date);

    // 应用临时加价规则
    static double applyTempPriceIncrease(double freight, const QList<TempPriceIncrease> &rules, const QDate &date);

    // 应用省份加价规则
    static double applyProvincePriceIncrease(double freight, const QList<ProvincePriceIncrease> &rules, const QString &province);
};

#endif // CALCULATIONRULE_H
