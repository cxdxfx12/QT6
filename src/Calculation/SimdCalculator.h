#ifndef SIMDCALCULATOR_H
#define SIMDCALCULATOR_H

#include <QList>
#include <QHash>
#include <QString>
#include <functional>
#include "DataModel/OrderData.h"
#include "DataModel/PriceTable.h"

/*
 * SIMD加速运费计算器
 *
 * 技术特性：
 * 1. SoA (Structure of Arrays) 数据布局 — 缓存友好，SIMD加载高效
 * 2. AVX2 向量化 — 单条指令处理4个double (256位寄存器)
 * 3. SSE2 向量化 — 兼容所有x86-64 CPU，单条指令处理2个double
 * 4. 运行时CPU特性检测 — 自动选择最快路径
 * 5. 预计算省份哈希索引 — 消除热路径中的QString规范化开销
 * 6. 分段分组SIMD计算 — 同一重量段的订单批量计算
 *
 * SIMD优化覆盖的运算：
 * - effectiveWeight = max(actualWeight, volumetricWeight)  → _mm256_max_pd
 * - firstPrice + (weight - firstWeight) * additionalPrice   → _mm256_sub_pd + _mm256_mul_pd
 * - firstPrice + (ceil(weight) - firstWeight) * additionalPrice (全续) → _mm256_round_pd + _mm256_sub_pd
 * - firstPrice + ceil((weight - minWeight) * 10) * (price/10) (百克续) → _mm256_round_pd + _mm256_mul_pd
 */
class SimdCalculator {
public:
    /*
     * SIMD加速批量计算
     *
     * @param orders        订单数组（原地修改freight字段）
     * @param count         订单数量
     * @param rule          客户规则
     * @param defaultTable  默认报价表
     * @param globalRules   全局规则
     * @return              错误数量
     */
    static int calculateChunk(
        OrderData* __restrict orders,
        int count,
        const CustomerRule& rule,
        const QList<PriceRule>& defaultTable,
        const GlobalRules& globalRules
    );

    // 检测CPU是否支持AVX2
    static bool hasAvx2();
    // 检测CPU是否支持SSE4.1
    static bool hasSse41();

private:
    // ---- SIMD 内核 (带 target 属性，运行时分发) ----

    // 批量计算有效重量: max(actual, volumetric)
    static void computeEffectiveWeightsMax(
        const double* __restrict actual,
        const double* __restrict vol,
        double* __restrict out,
        int count
    );

    // 批量计算有效重量: (actual + volumetric) / 2
    static void computeEffectiveWeightsAvg(
        const double* __restrict actual,
        const double* __restrict vol,
        double* __restrict out,
        int count
    );

    // 标准计算: firstPrice + ceil(weight - firstWeight) * additionalPrice
    static void calcStandardBatch(
        const double* __restrict weights,
        double* __restrict results,
        int count,
        double firstWeight,
        double firstPrice,
        double additionalPrice
    );

    // 全续计算: max(firstPrice, ceil(weight) * additionalPrice)
    static void calcFullAdditionalBatch(
        const double* __restrict weights,
        double* __restrict results,
        int count,
        double firstPrice,
        double additionalPrice
    );

    // 百克续计算: firstPrice + ceil((weight - minWeight) * 10) * (additionalPrice / 10)
    static void calcHundredGramBatch(
        const double* __restrict weights,
        double* __restrict results,
        int count,
        double minWeight,
        double firstPrice,
        double additionalPrice
    );

    // 省份名规范化 + 哈希索引构建
    static QHash<QString, int> buildProvinceIndex(
        const QList<PriceRule>& rules
    );

    static QString normalizeProvince(const QString& province);
};

#endif // SIMDCALCULATOR_H
