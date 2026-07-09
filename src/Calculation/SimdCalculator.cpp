#include "SimdCalculator.h"
#include "DataModel/CalculationRule.h"

#include <cmath>
#include <cstring>

// 架构检测：x86/x64 才启用 SIMD 加速
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
    #define SIMD_X86_ENABLED 1

    // GCC/Clang: 用 __attribute__((target)) 做运行时分发
    #if defined(__GNUC__) || defined(__clang__)
        #define SIMD_TARGET_AVX2 __attribute__((target("avx2,fma")))
        #define SIMD_TARGET_SSE41 __attribute__((target("sse4.1")))
    #else
        #define SIMD_TARGET_AVX2
        #define SIMD_TARGET_SSE41
    #endif

    // immintrin.h 包含所有SIMD头文件 (SSE, SSE2, SSE4.1, AVX, AVX2, FMA)
    #include <immintrin.h>

    #ifndef _MM_FROUND_CEIL
        #define _MM_FROUND_CEIL 0x02
    #endif
#else
    // 非 x86 架构（如 arm64 / Apple Silicon）：纯标量回退
    #define SIMD_X86_ENABLED 0
#endif

// ============================================================
// CPU 特性检测
// ============================================================

bool SimdCalculator::hasAvx2() {
#if SIMD_X86_ENABLED && (defined(__GNUC__) || defined(__clang__))
    return __builtin_cpu_supports("avx2");
#else
    return false;
#endif
}

bool SimdCalculator::hasSse41() {
#if SIMD_X86_ENABLED && (defined(__GNUC__) || defined(__clang__))
    return __builtin_cpu_supports("sse4.1");
#else
    return false;
#endif
}

// ============================================================
// 省份名规范化 + 哈希索引构建
// ============================================================

QString SimdCalculator::normalizeProvince(const QString &province) {
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

QHash<QString, int> SimdCalculator::buildProvinceIndex(const QList<PriceRule> &rules) {
    QHash<QString, int> index;
    index.reserve(rules.size() * 2);
    for (int i = 0; i < rules.size(); ++i) {
        QString key = normalizeProvince(rules[i].province);
        if (!key.isEmpty()) {
            index.insert(key, i);
            // 也插入原始名称，用于模糊匹配
            if (rules[i].province != key) {
                index.insert(rules[i].province, i);
            }
        }
    }
    return index;
}

#if SIMD_X86_ENABLED

// ============================================================
// SIMD 内核 — AVX2 版本 (4个double并行)
// ============================================================

SIMD_TARGET_AVX2
static void computeEffectiveWeightsMaxAvx2(
    const double* __restrict actual,
    const double* __restrict vol,
    double* __restrict out,
    int count
) {
    int i = 0;
    int limit = count - 3;  // 4的倍数

    for (; i < limit; i += 4) {
        __m256d a = _mm256_loadu_pd(actual + i);
        __m256d v = _mm256_loadu_pd(vol + i);
        __m256d result = _mm256_max_pd(a, v);
        _mm256_storeu_pd(out + i, result);
    }

    // 尾部处理
    for (; i < count; ++i) {
        out[i] = std::max(actual[i], vol[i]);
    }
}

SIMD_TARGET_AVX2
static void computeEffectiveWeightsAvgAvx2(
    const double* __restrict actual,
    const double* __restrict vol,
    double* __restrict out,
    int count
) {
    int i = 0;
    int limit = count - 3;
    __m256d half = _mm256_set1_pd(0.5);

    for (; i < limit; i += 4) {
        __m256d a = _mm256_loadu_pd(actual + i);
        __m256d v = _mm256_loadu_pd(vol + i);
        __m256d sum = _mm256_add_pd(a, v);
        __m256d result = _mm256_mul_pd(sum, half);
        _mm256_storeu_pd(out + i, result);
    }

    for (; i < count; ++i) {
        out[i] = (actual[i] + vol[i]) * 0.5;
    }
}

SIMD_TARGET_AVX2
static void calcStandardBatchAvx2(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double firstWeight,
    double firstPrice,
    double additionalPrice
) {
    int i = 0;
    int limit = count - 3;

    __m256d vFirstW = _mm256_set1_pd(firstWeight);
    __m256d vFirstP = _mm256_set1_pd(firstPrice);
    __m256d vAddP   = _mm256_set1_pd(additionalPrice);
    __m256d zero    = _mm256_setzero_pd();

    for (; i < limit; i += 4) {
        __m256d w = _mm256_loadu_pd(weights + i);
        // extra = weight - firstWeight
        __m256d extra = _mm256_sub_pd(w, vFirstW);
        // 下限保护：extra 不能小于 0
        extra = _mm256_max_pd(extra, zero);
        // freight = firstPrice + extra * additionalPrice
        __m256d freight = _mm256_add_pd(vFirstP, _mm256_mul_pd(extra, vAddP));
        _mm256_storeu_pd(results + i, freight);
    }

    for (; i < count; ++i) {
        double extra = weights[i] - firstWeight;
        if (extra < 0) extra = 0;
        results[i] = firstPrice + extra * additionalPrice;
    }
}

SIMD_TARGET_AVX2
static void calcFullAdditionalBatchAvx2(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double firstPrice,
    double additionalPrice
) {
    int i = 0;
    int limit = count - 3;

    __m256d vFirstP = _mm256_set1_pd(firstPrice);
    __m256d vAddP = _mm256_set1_pd(additionalPrice);

    for (; i < limit; i += 4) {
        __m256d w = _mm256_loadu_pd(weights + i);
        // fullKg = ceil(weight)
        __m256d fullKg = _mm256_round_pd(w, _MM_FROUND_CEIL);
        // freight = fullKg * additionalPrice
        __m256d freight = _mm256_mul_pd(fullKg, vAddP);
        // freight = max(firstPrice, freight)  — 最低收费保护
        freight = _mm256_max_pd(freight, vFirstP);
        _mm256_storeu_pd(results + i, freight);
    }

    for (; i < count; ++i) {
        int fullKg = static_cast<int>(std::ceil(weights[i]));
        double result = fullKg * additionalPrice;
        if (result < firstPrice) {
            result = firstPrice;
        }
        results[i] = result;
    }
}

SIMD_TARGET_AVX2
static void calcHundredGramBatchAvx2(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double minWeight,
    double firstPrice,
    double additionalPrice
) {
    int i = 0;
    int limit = count - 3;

    __m256d vMinW = _mm256_set1_pd(minWeight);
    __m256d vFirstP = _mm256_set1_pd(firstPrice);
    __m256d vTen = _mm256_set1_pd(10.0);
    __m256d vAddP10 = _mm256_set1_pd(additionalPrice / 10.0);

    for (; i < limit; i += 4) {
        __m256d w = _mm256_loadu_pd(weights + i);
        // extra = weight - minWeight
        __m256d extra = _mm256_sub_pd(w, vMinW);
        // scaledExtra = extra * 10
        __m256d scaledExtra = _mm256_mul_pd(extra, vTen);
        // hundredGramUnits = ceil(scaledExtra)
        __m256d units = _mm256_round_pd(scaledExtra, _MM_FROUND_CEIL);
        // 下限保护：units 不能小于 0
        __m256d zero = _mm256_setzero_pd();
        units = _mm256_max_pd(units, zero);
        // freight = firstPrice + units * (additionalPrice / 10)
        __m256d freight = _mm256_add_pd(vFirstP, _mm256_mul_pd(units, vAddP10));
        _mm256_storeu_pd(results + i, freight);
    }

    for (; i < count; ++i) {
        double extra = weights[i] - minWeight;
        int hundredGramUnits = static_cast<int>(std::ceil(extra * 10));
        if (hundredGramUnits < 0) hundredGramUnits = 0;
        results[i] = firstPrice + hundredGramUnits * (additionalPrice / 10.0);
    }
}

// ============================================================
// SIMD 内核 — SSE2 版本 (2个double并行，兼容所有x86-64)
// ============================================================

static void computeEffectiveWeightsMaxSse2(
    const double* __restrict actual,
    const double* __restrict vol,
    double* __restrict out,
    int count
) {
    int i = 0;
    int limit = count - 1;

    for (; i < limit; i += 2) {
        __m128d a = _mm_loadu_pd(actual + i);
        __m128d v = _mm_loadu_pd(vol + i);
        __m128d result = _mm_max_pd(a, v);
        _mm_storeu_pd(out + i, result);
    }

    for (; i < count; ++i) {
        out[i] = std::max(actual[i], vol[i]);
    }
}

static void computeEffectiveWeightsAvgSse2(
    const double* __restrict actual,
    const double* __restrict vol,
    double* __restrict out,
    int count
) {
    int i = 0;
    int limit = count - 1;
    __m128d half = _mm_set1_pd(0.5);

    for (; i < limit; i += 2) {
        __m128d a = _mm_loadu_pd(actual + i);
        __m128d v = _mm_loadu_pd(vol + i);
        __m128d sum = _mm_add_pd(a, v);
        __m128d result = _mm_mul_pd(sum, half);
        _mm_storeu_pd(out + i, result);
    }

    for (; i < count; ++i) {
        out[i] = (actual[i] + vol[i]) * 0.5;
    }
}

static void calcStandardBatchSse2(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double firstWeight,
    double firstPrice,
    double additionalPrice
) {
    for (int i = 0; i < count; ++i) {
        double extra = weights[i] - firstWeight;
        if (extra < 0) extra = 0;
        results[i] = firstPrice + extra * additionalPrice;
    }
}

static void calcFullAdditionalBatchSse2(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double firstPrice,
    double additionalPrice
) {
    int i = 0;
    int limit = count - 1;
    __m128d vFirstP = _mm_set1_pd(firstPrice);
    __m128d vAddP = _mm_set1_pd(additionalPrice);

    for (; i < limit; i += 2) {
        // ceil 需要SSE4.1，SSE2用标量
        double w0 = std::ceil(weights[i]);
        double w1 = std::ceil(weights[i + 1]);
        __m128d fullKg = _mm_set_pd(w1, w0);
        __m128d freight = _mm_mul_pd(fullKg, vAddP);
        // 最低收费保护
        freight = _mm_max_pd(freight, vFirstP);
        _mm_storeu_pd(results + i, freight);
    }

    for (; i < count; ++i) {
        int fullKg = static_cast<int>(std::ceil(weights[i]));
        double result = fullKg * additionalPrice;
        if (result < firstPrice) {
            result = firstPrice;
        }
        results[i] = result;
    }
}

static void calcHundredGramBatchSse2(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double minWeight,
    double firstPrice,
    double additionalPrice
) {
    for (int i = 0; i < count; ++i) {
        double extra = weights[i] - minWeight;
        int hundredGramUnits = static_cast<int>(std::ceil(extra * 10));
        if (hundredGramUnits < 0) hundredGramUnits = 0;
        results[i] = firstPrice + hundredGramUnits * (additionalPrice / 10.0);
    }
}

#endif // SIMD_X86_ENABLED

// ============================================================
// 标量版本 (非 x86 架构兜底，或 x86 下作为参考)
// ============================================================

static void computeEffectiveWeightsMaxScalar(
    const double* __restrict actual,
    const double* __restrict vol,
    double* __restrict out,
    int count
) {
    for (int i = 0; i < count; ++i) {
        out[i] = std::max(actual[i], vol[i]);
    }
}

static void computeEffectiveWeightsAvgScalar(
    const double* __restrict actual,
    const double* __restrict vol,
    double* __restrict out,
    int count
) {
    for (int i = 0; i < count; ++i) {
        out[i] = (actual[i] + vol[i]) * 0.5;
    }
}

static void calcStandardBatchScalar(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double firstWeight,
    double firstPrice,
    double additionalPrice
) {
    for (int i = 0; i < count; ++i) {
        double extra = weights[i] - firstWeight;
        if (extra < 0) extra = 0;
        results[i] = firstPrice + extra * additionalPrice;
    }
}

static void calcFullAdditionalBatchScalar(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double firstPrice,
    double additionalPrice
) {
    for (int i = 0; i < count; ++i) {
        int fullKg = static_cast<int>(std::ceil(weights[i]));
        double result = fullKg * additionalPrice;
        if (result < firstPrice) {
            result = firstPrice;
        }
        results[i] = result;
    }
}

static void calcHundredGramBatchScalar(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double minWeight,
    double firstPrice,
    double additionalPrice
) {
    for (int i = 0; i < count; ++i) {
        double extra = weights[i] - minWeight;
        int hundredGramUnits = static_cast<int>(std::ceil(extra * 10));
        if (hundredGramUnits < 0) hundredGramUnits = 0;
        results[i] = firstPrice + hundredGramUnits * (additionalPrice / 10.0);
    }
}

// ============================================================
// 运行时分发封装
// ============================================================

void SimdCalculator::computeEffectiveWeightsMax(
    const double* __restrict actual,
    const double* __restrict vol,
    double* __restrict out,
    int count
) {
#if SIMD_X86_ENABLED
    if (hasAvx2()) {
        computeEffectiveWeightsMaxAvx2(actual, vol, out, count);
    } else {
        computeEffectiveWeightsMaxSse2(actual, vol, out, count);
    }
#else
    computeEffectiveWeightsMaxScalar(actual, vol, out, count);
#endif
}

void SimdCalculator::computeEffectiveWeightsAvg(
    const double* __restrict actual,
    const double* __restrict vol,
    double* __restrict out,
    int count
) {
#if SIMD_X86_ENABLED
    if (hasAvx2()) {
        computeEffectiveWeightsAvgAvx2(actual, vol, out, count);
    } else {
        computeEffectiveWeightsAvgSse2(actual, vol, out, count);
    }
#else
    computeEffectiveWeightsAvgScalar(actual, vol, out, count);
#endif
}

void SimdCalculator::calcStandardBatch(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double firstWeight,
    double firstPrice,
    double additionalPrice
) {
#if SIMD_X86_ENABLED
    if (hasAvx2()) {
        calcStandardBatchAvx2(weights, results, count, firstWeight, firstPrice, additionalPrice);
    } else {
        calcStandardBatchSse2(weights, results, count, firstWeight, firstPrice, additionalPrice);
    }
#else
    calcStandardBatchScalar(weights, results, count, firstWeight, firstPrice, additionalPrice);
#endif
}

void SimdCalculator::calcFullAdditionalBatch(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double firstPrice,
    double additionalPrice
) {
#if SIMD_X86_ENABLED
    if (hasAvx2()) {
        calcFullAdditionalBatchAvx2(weights, results, count, firstPrice, additionalPrice);
    } else {
        calcFullAdditionalBatchSse2(weights, results, count, firstPrice, additionalPrice);
    }
#else
    calcFullAdditionalBatchScalar(weights, results, count, firstPrice, additionalPrice);
#endif
}

void SimdCalculator::calcHundredGramBatch(
    const double* __restrict weights,
    double* __restrict results,
    int count,
    double minWeight,
    double firstPrice,
    double additionalPrice
) {
#if SIMD_X86_ENABLED
    if (hasAvx2()) {
        calcHundredGramBatchAvx2(weights, results, count, minWeight, firstPrice, additionalPrice);
    } else {
        calcHundredGramBatchSse2(weights, results, count, minWeight, firstPrice, additionalPrice);
    }
#else
    calcHundredGramBatchScalar(weights, results, count, minWeight, firstPrice, additionalPrice);
#endif
}

// ============================================================
// 主计算入口 — SoA布局 + 分组SIMD
// ============================================================

int SimdCalculator::calculateChunk(
    OrderData* __restrict orders,
    int count,
    const CustomerRule& rule,
    const QList<PriceRule>& defaultTable,
    const GlobalRules& globalRules
) {
    if (count <= 0) return 0;

    // ---- Phase 1: 准备 SoA 数据 ----
    // 将 AoS (Array of Structures) 转换为 SoA (Structure of Arrays)
    // 这使得SIMD加载连续内存，避免跨步访问

    QVector<double> actualWeights(count);
    QVector<double> volWeights(count);
    QVector<double> effWeights(count);
    QVector<double> freights(count);
    QVector<int> priceRuleIdx(count, -1);  // 价格规则索引
    QVector<int> segmentIdx(count, -1);    // 重量段索引
    QVector<QString> originalProvinces(count);  // 保留原始省份名用于省份加价规则

    // 填充 SoA 数据
    for (int i = 0; i < count; ++i) {
        actualWeights[i] = orders[i].actualWeight;
        volWeights[i] = orders[i].volumetricWeight;
        originalProvinces[i] = orders[i].destinationProvince;
    }

    // ---- Phase 2: SIMD 计算有效重量 ----
    CalculationRule::Mode mode = CalculationRule::modeFromString(rule.calculationMode);

    // 所有模式都取实际重量和体积重的最大值
    computeEffectiveWeightsMax(
        actualWeights.constData(),
        volWeights.constData(),
        effWeights.data(),
        count
    );

    // ---- Phase 3: 预匹配省份 → 价格规则 (哈希O(1) + 两字模糊) ----
    QHash<QString, int> customIdx = buildProvinceIndex(rule.customPriceRules);
    QHash<QString, int> defaultIdx = buildProvinceIndex(defaultTable);

    // 辅助: 两字前缀模糊匹配（此前已通过哈希精确匹配和contains模糊匹配）
    auto fuzzyMatch = [](const QString &a, const QString &b) -> bool {
        if (a.size() >= 2 && b.size() >= 2 && a.left(2) == b.left(2)) return true;
        return false;
    };

    for (int i = 0; i < count; ++i) {
        QString normalized = normalizeProvince(originalProvinces[i]);

        // 先查自定义规则
        auto it = customIdx.constFind(normalized);
        if (it != customIdx.constEnd()) {
            priceRuleIdx[i] = it.value();
            // 标记使用自定义规则 (用正数索引)
        } else {
            // 模糊匹配: contains
            bool found = false;
            for (auto cit = customIdx.constBegin(); cit != customIdx.constEnd(); ++cit) {
                if (normalized.contains(cit.key()) || cit.key().contains(normalized)) {
                    priceRuleIdx[i] = cit.value();
                    found = true;
                    break;
                }
            }

            // 模糊匹配: 两字前缀
            if (!found) {
                for (auto cit = customIdx.constBegin(); cit != customIdx.constEnd(); ++cit) {
                    if (fuzzyMatch(normalized, cit.key())) {
                        priceRuleIdx[i] = cit.value();
                        found = true;
                        break;
                    }
                }
            }

            if (!found) {
                // 查默认规则
                auto dit = defaultIdx.constFind(normalized);
                if (dit != defaultIdx.constEnd()) {
                    // 用负数偏移标记默认规则: -(idx+1)
                    priceRuleIdx[i] = -(dit.value() + 1);
                } else {
                    bool foundDefault = false;
                    // contains模糊匹配
                    for (auto dit2 = defaultIdx.constBegin(); dit2 != defaultIdx.constEnd(); ++dit2) {
                        if (normalized.contains(dit2.key()) || dit2.key().contains(normalized)) {
                            priceRuleIdx[i] = -(dit2.value() + 1);
                            foundDefault = true;
                            break;
                        }
                    }
                    // 两字前缀模糊匹配
                    if (!foundDefault) {
                        for (auto dit2 = defaultIdx.constBegin(); dit2 != defaultIdx.constEnd(); ++dit2) {
                            if (fuzzyMatch(normalized, dit2.key())) {
                                priceRuleIdx[i] = -(dit2.value() + 1);
                                foundDefault = true;
                                break;
                            }
                        }
                    }
                    if (!foundDefault && !defaultTable.isEmpty()) {
                        priceRuleIdx[i] = -1;  // 使用默认第一条
                    }
                }
            }
        }
    }

    // ---- Phase 4: 匹配重量段 ----
    for (int i = 0; i < count; ++i) {
        const PriceRule* pr = nullptr;

        if (priceRuleIdx[i] >= 0) {
            pr = &rule.customPriceRules[priceRuleIdx[i]];
        } else if (priceRuleIdx[i] < 0) {
            int idx = -priceRuleIdx[i] - 1;
            pr = &defaultTable[idx];
        }

        if (!pr) continue;

        double matchWeight = effWeights[i];
        if (mode == CalculationRule::Mode::FullAdditional) {
            matchWeight = std::ceil(matchWeight);
        }

        for (int s = 0; s < pr->segments.size(); ++s) {
            if (CalculationRule::isWeightInRange(matchWeight, pr->segments[s])) {
                segmentIdx[i] = s;
                break;
            }
        }
    }

    // ---- Phase 5: 分组SIMD计算运费 ----
    // 按 (priceRuleIdx, segmentIdx) 分组，同组订单用同一公式批量SIMD计算

    // 构建分组: key = priceRuleIdx * 1000 + segmentIdx
    QHash<int, QVector<int>> groups;
    groups.reserve(count / 10 + 1);

    for (int i = 0; i < count; ++i) {
        if (effWeights[i] <= 0) {
            // 无重量处理
            if (globalRules.noWeightDefaultPrice > 0) {
                freights[i] = globalRules.noWeightDefaultPrice;
            } else {
                freights[i] = 0;
            }
            continue;
        }

        if (segmentIdx[i] < 0) {
            freights[i] = 0;
            continue;
        }

        int key = priceRuleIdx[i] * 10000 + segmentIdx[i];
        groups[key].append(i);
    }

    // 对每个分组执行SIMD批量计算
    for (auto it = groups.begin(); it != groups.end(); ++it) {
        const QVector<int>& indices = it.value();
        int groupSize = indices.size();
        if (groupSize == 0) continue;

        // 提取该组的价格规则和重量段
        int sampleIdx = indices[0];
        const PriceRule* pr = nullptr;
        if (priceRuleIdx[sampleIdx] >= 0) {
            pr = &rule.customPriceRules[priceRuleIdx[sampleIdx]];
        } else {
            int idx = -priceRuleIdx[sampleIdx] - 1;
            pr = &defaultTable[idx];
        }

        const WeightSegment& seg = pr->segments[segmentIdx[sampleIdx]];

        // 计算全续模式下的最低收费：取匹配段及之前所有段的最大首重价（防止价格倒挂）
        // 只遍历到匹配段（含），与标量路径 CalculationRule::calculateFullAdditional 行为一致
        double minFullAdditionalPrice = 0;
        if (mode == CalculationRule::Mode::FullAdditional || seg.isFullAdditional) {
            int matchSegIdx = segmentIdx[sampleIdx];
            for (int s = 0; s <= matchSegIdx && s < pr->segments.size(); ++s) {
                double fp = pr->segments[s].firstWeightPrice;
                if (fp > minFullAdditionalPrice) {
                    minFullAdditionalPrice = fp;
                }
            }
        }

        // 将该组的权重提取到连续数组 (SIMD需要连续内存)
        QVector<double> groupWeights(groupSize);
        for (int j = 0; j < groupSize; ++j) {
            groupWeights[j] = effWeights[indices[j]];
        }

        QVector<double> groupResults(groupSize);

        // 根据计算模式选择SIMD内核（计算模式优先于重量段设置）
        if (mode == CalculationRule::Mode::FullAdditional) {
            QVector<double> fullWeights = groupWeights;
            for (int j = 0; j < groupSize; ++j) {
                fullWeights[j] = std::ceil(fullWeights[j]);
            }
            double firstWeight = seg.minWeight;
            if (seg.maxWeight < 0 && seg.minWeight >= 30) {
                firstWeight = 3.0;
            }
            calcStandardBatch(
                fullWeights.constData(),
                groupResults.data(),
                groupSize,
                firstWeight,
                seg.firstWeightPrice,
                seg.additionalPrice
            );
            for (int j = 0; j < groupSize; ++j) {
                if (groupResults[j] < minFullAdditionalPrice) {
                    groupResults[j] = minFullAdditionalPrice;
                }
            }
        } else if (mode == CalculationRule::Mode::HundredGram) {
            calcHundredGramBatch(
                groupWeights.constData(),
                groupResults.data(),
                groupSize,
                seg.minWeight,
                seg.firstWeightPrice,
                seg.additionalPrice
            );
        } else if (seg.isFullAdditional) {
            QVector<double> fullWeights = groupWeights;
            for (int j = 0; j < groupSize; ++j) {
                fullWeights[j] = std::ceil(fullWeights[j]);
            }
            double firstWeight = seg.minWeight;
            if (seg.maxWeight < 0 && seg.minWeight >= 30) {
                firstWeight = 3.0;
            }
            calcStandardBatch(
                fullWeights.constData(),
                groupResults.data(),
                groupSize,
                firstWeight,
                seg.firstWeightPrice,
                seg.additionalPrice
            );
            for (int j = 0; j < groupSize; ++j) {
                if (groupResults[j] < minFullAdditionalPrice) {
                    groupResults[j] = minFullAdditionalPrice;
                }
            }
        } else if (seg.isHundredGram) {
            calcHundredGramBatch(
                groupWeights.constData(),
                groupResults.data(),
                groupSize,
                seg.minWeight,
                seg.firstWeightPrice,
                seg.additionalPrice
            );
        } else {
            // 标准计算
            double firstWeight = seg.minWeight;
            if (seg.maxWeight < 0 && seg.minWeight >= 30) {
                firstWeight = 3.0;
            }
            calcStandardBatch(
                groupWeights.constData(),
                groupResults.data(),
                groupSize,
                firstWeight,
                seg.firstWeightPrice,
                seg.additionalPrice
            );
        }

        // 散布结果回主数组
        for (int j = 0; j < groupSize; ++j) {
            freights[indices[j]] = groupResults[j];
        }
    }

    // ---- Phase 6: 应用全局规则 (标量，条件分支) + 记录使用的规则 ----
    // 活动规则、临时加价、省份加价 都是条件逻辑，不适合SIMD
    for (int i = 0; i < count; ++i) {
        if (freights[i] <= 0) continue;

        double freight = freights[i];
        QString ruleDesc;

        // 基础价格规则名
        const PriceRule* pr = nullptr;
        bool isCustomRule = false;
        if (priceRuleIdx[i] >= 0) {
            pr = &rule.customPriceRules[priceRuleIdx[i]];
            isCustomRule = true;
        } else if (priceRuleIdx[i] < 0) {
            int idx = -priceRuleIdx[i] - 1;
            if (idx >= 0 && idx < defaultTable.size()) {
                pr = &defaultTable[idx];
            }
        }
        if (pr) {
            if (isCustomRule && !rule.customerName.isEmpty()) {
                ruleDesc = rule.customerName + QStringLiteral("-");
            } else {
                ruleDesc = QStringLiteral("默认报价-");
            }
            ruleDesc += pr->province;
            if (!pr->region.isEmpty())
                ruleDesc += QStringLiteral("(%1)").arg(pr->region);
        }

        // 活动加价
        QDateTime bizTime = QDateTime(orders[i].businessTime, QTime(0, 0));
        for (const ActivityRule &ar : globalRules.activityRules) {
            if (ar.isInRange(bizTime)) {
                ruleDesc += QStringLiteral("+%1").arg(ar.name.isEmpty() ? QStringLiteral("活动加价") : ar.name);
                if (ar.isFixedAmount) {
                    freight += ar.increaseAmount;
                } else {
                    freight *= (1.0 + ar.increaseRate);
                }
            }
        }

        // 临时加价
        for (const TempPriceIncrease &tpi : globalRules.tempPriceIncreases) {
            if (tpi.isInRange(bizTime)) {
                ruleDesc += QStringLiteral("+%1").arg(tpi.name.isEmpty() ? QStringLiteral("临时加价") : tpi.name);
                if (tpi.isFixedAmount) {
                    freight += tpi.increaseAmount;
                } else {
                    freight *= (1.0 + tpi.increaseRate);
                }
            }
        }

        // 省份加价（规范化匹配）
        static const QStringList provSuffixes = {
            QStringLiteral("省"), QStringLiteral("市"),
            QStringLiteral("自治区"), QStringLiteral("特别行政区")
        };
        QString normOrderProv = orders[i].destinationProvince;
        for (const QString &sf : provSuffixes) {
            if (normOrderProv.endsWith(sf)) {
                normOrderProv.chop(sf.size());
                break;
            }
        }
        normOrderProv = normOrderProv.trimmed();

        for (const ProvincePriceIncrease &ppi : globalRules.provincePriceIncreases) {
            if (!ppi.isActive) continue;
            QString ruleProv = ppi.province;
            for (const QString &sf : provSuffixes) {
                if (ruleProv.endsWith(sf)) {
                    ruleProv.chop(sf.size());
                    break;
                }
            }
            ruleProv = ruleProv.trimmed();
            if (ruleProv == normOrderProv || ppi.province == orders[i].destinationProvince) {
                ruleDesc += QStringLiteral("+省份加价");
                freight += ppi.increaseAmount;
                // 不 break：与标量路径 applyProvincePriceIncrease 一致，允许多条省份加价规则叠加
            }
        }

        // 拉均重加价
        if (globalRules.runtimeAvgSurcharge > 0 && mode == CalculationRule::Mode::AverageWeight) {
            freight += globalRules.runtimeAvgSurcharge;
            ruleDesc += QStringLiteral("+拉均重加价%1").arg(globalRules.runtimeAvgSurcharge, 0, 'f', 2);
        }

        // 四舍五入到分
        freights[i] = std::round(freight * 100.0) / 100.0;
        orders[i].usedRule = ruleDesc;
    }

    // ---- Phase 7: 写回结果 ----
    int errorCount = 0;
    for (int i = 0; i < count; ++i) {
        orders[i].freight = freights[i];
        if (freights[i] <= 0 && orders[i].actualWeight > 0) {
            orders[i].isValid = false;
            orders[i].errorMessage = QStringLiteral("计算结果为0，可能未匹配到价格规则");
            errorCount++;
        } else {
            orders[i].isValid = true;
            orders[i].errorMessage.clear();
        }
    }

    return errorCount;
}
