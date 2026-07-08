#ifndef PRICETABLE_H
#define PRICETABLE_H

#include <QString>
#include <QList>
#include <QMap>
#include <QDateTime>

// 重量段定义
struct WeightSegment {
    double minWeight = 0;           // 最小重量 (kg)
    double maxWeight = -1;          // 最大重量 (kg), -1 表示无上限
    double firstWeightPrice = 0;    // 首重价格 (元)
    double additionalPrice = 0;     // 续重价格 (元/kg)
    bool isFullAdditional = false;  // 是否全续模式
    bool isHundredGram = false;     // 是否百克续
};

// 价格规则（按区域/省份）
struct PriceRule {
    QString region;                 // 区域名称 (一区、二区...)
    QString province;               // 省份
    QList<WeightSegment> segments;  // 重量段列表
};

// 活动加价规则
struct ActivityRule {
    QString name;                   // 活动名称
    QDateTime startTime;            // 活动开始时间
    QDateTime endTime;              // 活动结束时间
    double increaseRate = 0.0;      // 加价比例 (0.1 = 加10%, 0 = 不加价)
    double increaseAmount = 0.0;    // 固定加价金额 (元)
    bool isFixedAmount = false;     // true=固定金额, false=比例
    bool isActive = true;           // 是否启用

    bool isInRange(const QDateTime &time) const {
        return isActive && time >= startTime && time <= endTime;
    }
};

// 临时加价规则
struct TempPriceIncrease {
    QString name;                   // 规则名称
    QDateTime startTime;            // 加价开始时间
    QDateTime endTime;              // 加价结束时间
    double increaseAmount = 0;      // 固定加价金额 (元)
    double increaseRate = 0;        // 加价比例 (0.1 = 加10%)
    bool isFixedAmount = true;      // true=固定金额, false=比例
    bool isActive = true;           // 是否启用

    bool isInRange(const QDateTime &time) const {
        return isActive && time >= startTime && time <= endTime;
    }
};

// 按省份加价规则
struct ProvincePriceIncrease {
    QString province;               // 省份
    double increaseAmount = 0;      // 加价金额 (元)
    bool isActive = true;           // 是否启用
};

// 全局规则（对所有客户生效）
struct GlobalRules {
    double noWeightDefaultPrice = 0;                    // 无重量默认价格 (元)
    QList<ActivityRule> activityRules;                  // 活动规则列表
    QList<TempPriceIncrease> tempPriceIncreases;        // 临时加价规则列表
    QList<ProvincePriceIncrease> provincePriceIncreases;// 按省份加价规则

    // 拉均重模式加价配置
    double avgWeightBase = 0.5;          // 基准重量(kg)，平均重量超过此值开始加价
    double avgWeightUpperLimit = 1.0;    // 上限重量(kg)，超过上限按上限计算
    double avgWeightIncrement = 0.1;     // 递增步长(kg)，每超过此值加一次价
    double avgWeightSurcharge = 0.1;     // 每步长加价金额(元)

    // 运行时计算的每包裹加价（非持久化，由批量计算前设置）
    double runtimeAvgSurcharge = 0.0;
};

// 客户规则（每客户单独配置）
struct CustomerRule {
    QString customerName;           // 客户名称
    QString mappingHeader;          // 映射的表头名称
    QString calculationMode;        // 计算模式: "实际重量"|"体积重"|"拉均重"
    bool useDefaultPrice = true;    // 是否使用默认报价
    QList<PriceRule> customPriceRules;  // 自定义报价规则
};

// 区域-省份映射（默认报价表用）
struct RegionMapping {
    QString region;
    QStringList provinces;
};

#endif // PRICETABLE_H
