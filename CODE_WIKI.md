# 悟空运费结算系统 - Code Wiki

## 目录

- [1. 项目概述](#1-项目概述)
- [2. 项目架构](#2-项目架构)
- [3. 目录结构](#3-目录结构)
- [4. 核心模块详解](#4-核心模块详解)
  - [4.1 DataModel 数据模型模块](#41-datamodel-数据模型模块)
  - [4.2 Calculation 计算模块](#42-calculation-计算模块)
  - [4.3 Excel 处理模块](#43-excel-处理模块)
  - [4.4 UI 界面模块](#44-ui-界面模块)
  - [4.5 Utils 工具模块](#45-utils-工具模块)
- [5. 关键类与函数说明](#5-关键类与函数说明)
- [6. 依赖关系](#6-依赖关系)
- [7. 计算规则详解](#7-计算规则详解)
- [8. 构建与运行](#8-构建与运行)
- [9. 配置与持久化](#9-配置与持久化)
- [10. 性能优化](#10-性能优化)

---

## 1. 项目概述

### 1.1 项目简介

**悟空运费结算系统**（WukongFreight）是一款基于 Qt6 框架开发的桌面端运费批量计算工具，专为物流/电商企业设计。系统支持从 Excel 文件批量导入订单数据，根据灵活配置的价格规则和客户规则进行运费计算，并将结果导出为 Excel 文件。

### 1.2 核心功能

- **Excel 批量导入**：支持多文件批量导入，自动识别列映射
- **灵活的价格规则**：支持按区域、省份配置多级重量段价格
- **多计算模式**：实际重量、拉均重、百克续、全续四种计算模式
- **客户级规则**：每个客户可独立配置计算模式和自定义报价
- **全局规则**：活动折扣、临时加价、省份加价等全局配置
- **批量计算**：多线程 + SIMD 加速，支持海量数据快速计算
- **结果导出**：支持按客户拆分导出、合并导出多种模式
- **快速测试**：单条运费快速测算功能

### 1.3 技术栈

| 类别 | 技术 | 版本/说明 |
|------|------|-----------|
| 开发框架 | Qt6 | 6.12.0+ |
| 编程语言 | C++ | C++17 |
| 构建系统 | CMake | 3.16+ |
| Excel 处理 | QXlsx | 1.4.4（静态库集成） |
| 并发框架 | QtConcurrent / QThreadPool | 多线程并行计算 |
| SIMD 优化 | AVX2 / SSE4.1 | 运行时 CPU 特性检测 |
| 配置存储 | QSettings / JSON | 系统配置 / 规则持久化 |

### 1.4 开发/发行信息

- **应用名称**：悟空运费结算
- **版本号**：1.0.0
- **开发组织**：杭州喵喵至家网络有限公司
- **目标平台**：Windows（主要）、macOS、Linux

---

## 2. 项目架构

### 2.1 整体架构图

```
┌───────────────────────────────────────────────────────────┐
│                        UI 层                               │
│  ┌──────────┐  ┌──────────────┐  ┌───────────────────┐   │
│  │MainWindow│  │RuleManagerDlg│  │各种Dialog/Widget   │   │
│  └────┬─────┘  └──────┬───────┘  └─────────┬─────────┘   │
└───────┼───────────────┼─────────────────────┼─────────────┘
        │               │                     │
┌───────▼───────────────▼─────────────────────▼─────────────┐
│                     业务逻辑层                              │
│  ┌───────────────┐  ┌─────────────┐  ┌───────────────┐   │
│  │ FreightCalc-  │  │ RuleManager │  │ ExcelImporter │   │
│  │ ulator        │  │             │  │ ExcelExporter │   │
│  └───────┬───────┘  └──────┬──────┘  └───────┬───────┘   │
│          │                 │                 │           │
│  ┌───────▼───────┐         │        ┌───────▼───────┐   │
│  │ SimdCalculator│         │        │  ExcelEngine  │   │
│  │ ThreadPool    │         │        │  (QXlsx)      │   │
│  └───────────────┘         │        └───────────────┘   │
└────────────────────────────┼─────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────┐
│                     数据模型层                              │
│  OrderData, Customer, PriceTable, CalculationRule...      │
└───────────────────────────────────────────────────────────┘
                             │
┌────────────────────────────▼─────────────────────────────┐
│                     工具层                                 │
│  Logger（日志）  ConfigManager（配置）                     │
└───────────────────────────────────────────────────────────┘
```

### 2.2 架构分层说明

| 层级 | 职责 | 关键类/文件 |
|------|------|-------------|
| UI 层 | 用户交互、界面展示、数据绑定 | MainWindow、各类Dialog、Widget |
| 业务逻辑层 | 运费计算、规则管理、Excel读写 | FreightCalculator、RuleManager、ExcelImporter/Exporter |
| 数据模型层 | 数据结构定义、纯计算逻辑 | OrderData、PriceTable（各种结构体）、CalculationRule |
| 工具层 | 日志记录、配置管理 | Logger、ConfigManager |

### 2.3 核心数据流

```
Excel文件输入
    │
    ▼
ExcelImporter  ──►  ExcelEngine (QXlsx)
    │
    ▼
QList<OrderData>  ──►  FreightCalculator.calculateBatch()
    │                         │
    │                         ├──► RuleManager（获取客户规则/全局规则）
    │                         ├──► CalculationRule（纯计算函数）
    │                         └──► SimdCalculator（SIMD加速）
    │
    ▼
QList<OrderData>（含运费结果）
    │
    ▼
ExcelExporter  ──►  ExcelEngine (QXlsx)
    │
    ▼
Excel文件输出
```

---

## 3. 目录结构

```
QT6-main/
├── CMakeLists.txt              # 顶层 CMake 构建配置
├── build.bat                   # Windows Batch 构建脚本
├── build.ps1                   # PowerShell 构建脚本
├── .gitignore                  # Git 忽略文件
│
├── src/                        # 源代码目录
│   ├── main.cpp                # 程序入口
│   ├── MainWindow.h            # 主窗口头文件
│   ├── MainWindow.cpp          # 主窗口实现
│   ├── MainWindow.ui           # 主窗口 UI 定义
│   │
│   ├── DataModel/              # 数据模型模块
│   │   ├── OrderData.h/cpp     # 订单数据结构
│   │   ├── Customer.h/cpp      # 客户类
│   │   ├── PriceTable.h        # 价格表相关结构体
│   │   ├── PriceTable.cpp
│   │   ├── CalculationRule.h   # 计算规则工具类
│   │   └── CalculationRule.cpp
│   │
│   ├── Calculation/            # 计算模块
│   │   ├── FreightCalculator.h/cpp    # 运费计算器（核心）
│   │   ├── SimdCalculator.h/cpp       # SIMD 加速计算器
│   │   ├── RuleManager.h/cpp          # 规则管理器
│   │   └── ThreadPool.h/cpp           # 线程池单例
│   │
│   ├── Excel/                  # Excel 处理模块
│   │   ├── ExcelEngine.h/cpp   # Excel 底层引擎（QXlsx封装）
│   │   ├── ExcelImporter.h/cpp # Excel 导入器
│   │   └── ExcelExporter.h/cpp # Excel 导出器
│   │
│   ├── UI/                     # UI 组件模块
│   │   ├── CustomerDialog.h/cpp        # 客户规则对话框
│   │   ├── PriceTableDialog.h/cpp      # 价格表对话框
│   │   ├── RuleManagerDialog.h/cpp     # 规则管理对话框
│   │   ├── RuleManagerDialog.ui        # 规则管理 UI
│   │   ├── GlobalRulesDialog.h/cpp     # 全局规则对话框
│   │   ├── QuickTestWidget.h/cpp       # 快速测试组件
│   │   └── ProgressWidget.h/cpp        # 进度条组件
│   │
│   └── Utils/                  # 工具模块
│       ├── Logger.h/cpp        # 日志工具（单例）
│       └── ConfigManager.h/cpp # 配置管理器
│
├── resources/                  # 资源文件
│   ├── resources.qrc           # Qt 资源文件定义
│   ├── style.qss               # 全局样式表（浅色主题）
│   ├── app_icon.png            # 应用图标
│   ├── app_icon_64.png         # 64x64 图标
│   ├── app_icon.ico            # Windows ICO 图标
│   └── app.rc                  # Windows 资源文件
│
├── QXlsx/                      # QXlsx 第三方库（子模块）
│   └── QXlsx/
│       ├── CMakeLists.txt
│       ├── header/             # 头文件
│       ├── source/             # 源文件
│       └── cmake/
│
├── test_e2e.py                 # Python 端到端测试脚本
├── test_excel.py               # Python Excel 测试脚本
├── calculate_freight.py        # Python 参考计算实现
└── calc_pandas.py              # Pandas 版本计算脚本
```

---

## 4. 核心模块详解

### 4.1 DataModel 数据模型模块

**位置**：[src/DataModel/](file:///Users/cxd/QT6-main/src/DataModel)

**职责**：定义项目中所有核心数据结构，提供纯计算逻辑函数，不涉及业务流程控制。

#### 4.1.1 OrderData - 订单数据结构

**文件**：[OrderData.h](file:///Users/cxd/QT6-main/src/DataModel/OrderData.h)

订单数据的核心结构体，包含一条运单的所有信息和计算结果。

| 字段 | 类型 | 说明 |
|------|------|------|
| `businessTime` | `QDate` | 业务时间（只保留年月日） |
| `waybillNo` | `QString` | 运单号 |
| `volumetricWeight` | `double` | 体积重 (kg) |
| `customer` | `QString` | 订单客户 |
| `client` | `QString` | 结算对象/客户 |
| `destinationProvince` | `QString` | 目的省份 |
| `actualWeight` | `double` | 实际重量 (kg) |
| `freight` | `double` | 计算出的运费（结果字段） |
| `usedRule` | `QString` | 辅助字段：使用的规则描述 |
| `errorMessage` | `QString` | 辅助字段：错误信息 |
| `isValid` | `bool` | 辅助字段：数据有效性 |

**辅助方法**：
- `year() const`：获取业务年份
- `month() const`：获取业务月份
- `day() const`：获取业务日期

#### 4.1.2 PriceTable - 价格表数据结构

**文件**：[PriceTable.h](file:///Users/cxd/QT6-main/src/DataModel/PriceTable.h)

定义了价格体系相关的所有结构体，是整个系统规则配置的基础。

**WeightSegment（重量段）**：
| 字段 | 类型 | 说明 |
|------|------|------|
| `minWeight` | `double` | 最小重量 (kg) |
| `maxWeight` | `double` | 最大重量 (kg), -1 表示无上限 |
| `firstWeightPrice` | `double` | 首重价格 (元) |
| `additionalPrice` | `double` | 续重价格 (元/kg) |
| `isFullAdditional` | `bool` | 是否全续模式 |
| `isHundredGram` | `bool` | 是否百克续模式 |

**PriceRule（价格规则）**：
- `region`：区域名称（一区、二区...）
- `province`：省份
- `segments`：重量段列表 `QList<WeightSegment>`

**ActivityRule（活动规则）**：
- `name`：活动名称
- `startTime` / `endTime`：活动时间范围
- `discountRate`：折扣率（0.8 = 八折）
- `fixedDiscount`：固定减免金额
- `isFixedDiscount`：true=固定减免, false=折扣率
- `isActive`：是否启用
- `isInRange(time)`：判断时间是否在活动范围内

**TempPriceIncrease（临时加价规则）**：
- `name`：规则名称
- `startTime` / `endTime`：加价时间范围
- `increaseAmount`：固定加价金额
- `increaseRate`：加价比例（0.1 = 加10%）
- `isFixedAmount`：true=固定金额, false=比例
- `isActive`：是否启用

**ProvincePriceIncrease（按省份加价）**：
- `province`：省份
- `increaseAmount`：加价金额
- `isActive`：是否启用

**GlobalRules（全局规则）**：
所有客户共享的规则配置。
- `noWeightDefaultPrice`：无重量默认价格
- `activityRules`：活动规则列表
- `tempPriceIncreases`：临时加价规则列表
- `provincePriceIncreases`：按省份加价规则列表
- `avgWeightBase` / `avgWeightUpperLimit` / `avgWeightIncrement` / `avgWeightSurcharge`：拉均重模式加价配置
- `runtimeAvgSurcharge`：运行时计算的每包裹加价（非持久化）

**CustomerRule（客户规则）**：
每个客户独立的规则配置。
- `customerName`：客户名称
- `mappingHeader`：映射的表头名称
- `calculationMode`：计算模式（"实际重量"|"体积重"|"拉均重"）
- `useDefaultPrice`：是否使用默认报价
- `customPriceRules`：自定义报价规则列表

**RegionMapping（区域-省份映射）**：
- `region`：区域名称
- `provinces`：该区域包含的省份列表

#### 4.1.3 Customer - 客户类

**文件**：[Customer.h](file:///Users/cxd/QT6-main/src/DataModel/Customer.h)

封装客户信息及其规则。

| 方法 | 说明 |
|------|------|
| `Customer(name)` | 构造函数 |
| `name() / setName()` | 客户名称访问器 |
| `rule() / setRule()` | 客户规则访问器 |
| `hasCustomPrice()` | 是否有自定义价格 |

#### 4.1.4 CalculationRule - 计算规则工具类

**文件**：[CalculationRule.h](file:///Users/cxd/QT6-main/src/DataModel/CalculationRule.h)

提供运费计算相关的纯静态工具函数，无状态，不涉及业务对象。

**计算模式枚举 `Mode`**：
- `ActualWeight`：实际重量
- `AverageWeight`：拉均重
- `HundredGram`：百克续
- `FullAdditional`：全续

**核心函数**：

| 函数 | 说明 |
|------|------|
| `modeFromString(str)` | 字符串转枚举 |
| `modeToString(mode)` | 枚举转字符串 |
| `allModes()` | 获取所有模式字符串列表 |
| `getEffectiveWeight(actual, vol, mode)` | 获取有效重量 |
| `isWeightInRange(weight, segment)` | 判断重量是否在某段内 |
| `calculateStandard(weight, segment)` | 标准计算（首重+续重） |
| `calculateHundredGram(weight, segment)` | 百克续计算 |
| `calculateFullAdditional(weight, segment)` | 全续计算 |
| `applyActivityRules(freight, rules, date)` | 应用活动规则 |
| `applyTempPriceIncrease(freight, rules, date)` | 应用临时加价 |
| `applyProvincePriceIncrease(freight, rules, province)` | 应用省份加价 |

---

### 4.2 Calculation 计算模块

**位置**：[src/Calculation/](file:///Users/cxd/QT6-main/src/Calculation)

**职责**：核心业务计算逻辑，包括运费计算、规则管理、SIMD加速、线程池。

#### 4.2.1 FreightCalculator - 运费计算器

**文件**：[FreightCalculator.h](file:///Users/cxd/QT6-main/src/Calculation/FreightCalculator.h)

系统核心类，封装运费计算的完整业务流程。

**关键属性**：
- `m_defaultPriceTable`：默认报价表
- `m_globalRules`：全局规则
- `m_cachedCustomRules` / `m_cachedDefaultRules`：规则缓存（按省份哈希索引）
- `BATCH_SIZE = 10000`：批量处理批次大小

**核心方法**：

| 方法 | 说明 |
|------|------|
| `calculateFreight(order, rule)` | 单条订单运费计算 |
| `calculateBatch(orders, rule, threadCount)` | 批量计算（多线程） |
| `quickCalculate(weight, province, rule, time)` | 快速测算 |
| `buildRuleCache(rule)` | 构建规则缓存（性能优化） |
| `setDefaultPriceTable(rules)` | 设置默认报价表 |
| `setGlobalRules(rules)` | 设置全局规则 |

**信号**：
- `progressUpdated(int percent)`：计算进度更新
- `calculationComplete(int totalCount, int errorCount)`：计算完成

**内部流程（单条计算）**：
1. 确定计算模式，计算有效重量
2. 有效重量为0且配置了无重量默认价 → 返回默认价 + 全局调整
3. 查找匹配的价格规则（自定义优先，否则用默认）
4. 匹配重量段，根据模式调用对应计算函数
5. 依次应用活动规则、临时加价、省份加价
6. 四舍五入保留两位小数

#### 4.2.2 SimdCalculator - SIMD 加速计算器

**文件**：[SimdCalculator.h](file:///Users/cxd/QT6-main/src/Calculation/SimdCalculator.h)

SIMD 向量化加速的批量运费计算器，用于高性能计算场景。

**技术特性**：
1. **SoA (Structure of Arrays) 数据布局**：缓存友好，SIMD 加载高效
2. **AVX2 向量化**：单条指令处理4个double（256位寄存器）
3. **SSE2 向量化**：兼容所有x86-64 CPU，单条指令处理2个double
4. **运行时CPU特性检测**：自动选择最快路径
5. **预计算省份哈希索引**：消除热路径中的QString规范化开销
6. **分段分组SIMD计算**：同一重量段的订单批量计算

**核心方法**：

| 方法 | 说明 |
|------|------|
| `calculateChunk(orders, count, rule, defaultTable, globalRules)` | 批量计算一个数据块 |
| `hasAvx2()` | 检测 CPU 是否支持 AVX2 |
| `hasSse41()` | 检测 CPU 是否支持 SSE4.1 |

**SIMD 内核函数**：
- `computeEffectiveWeightsMax`：批量计算有效重量（max模式）
- `computeEffectiveWeightsAvg`：批量计算有效重量（拉均重模式）
- `calcStandardBatch`：标准计算（首重+续重）
- `calcFullAdditionalBatch`：全续计算
- `calcHundredGramBatch`：百克续计算

**SIMD优化覆盖的运算**：
- `effectiveWeight = max(actualWeight, volumetricWeight)` → `_mm256_max_pd`
- `ceil(weight - firstWeight) * additionalPrice` → `_mm256_round_pd + _mm256_mul_pd`
- `ceil(weight) * additionalPrice`（全续模式）→ `_mm256_round_pd + _mm256_mul_pd`
- `ceil((weight - minWeight) * 10) * (price/10)`（百克续）→ `_mm256_round_pd + _mm256_mul_pd`

#### 4.2.3 RuleManager - 规则管理器

**文件**：[RuleManager.h](file:///Users/cxd/QT6-main/src/Calculation/RuleManager.h)

管理所有计算规则的增删改查和持久化。

**管理的数据**：
- 客户规则集合（`QMap<QString, CustomerRule>`）
- 默认报价表
- 全局规则（含活动规则、临时加价、省份加价、无重量默认价等）
- 区域-省份映射

**客户规则 CRUD**：
- `addCustomerRule(rule)` / `updateCustomerRule(name, rule)`
- `removeCustomerRule(name)` / `getRule(name)`
- `allRules()` / `ruleNames()` / `rulesMap()`
- `saveRule(rule)`

**默认报价表**：
- `setDefaultPriceTable(rules)` / `defaultPriceTable()`
- `initDefaultPriceTable()`：初始化默认报价表数据

**全局规则管理**：
- `globalRules()` / `setGlobalRules(rules)`
- 活动规则：`activityRules()`、`setActivityRules()`、`addActivityRule()`、`removeActivityRule()`
- 临时加价：`tempPriceIncreases()`、`setTempPriceIncreases()`、`addTempPriceIncrease()`、`removeTempPriceIncrease()`
- 省份加价：`provincePriceIncreases()`、`setProvincePriceIncreases()`、`addProvincePriceIncrease()`、`removeProvincePriceIncrease()`
- 无重量默认价：`noWeightDefaultPrice()` / `setNoWeightDefaultPrice()`

**持久化**：
- `saveToFile(filePath)`：保存规则到 JSON 文件
- `loadFromFile(filePath)`：从 JSON 文件加载规则

**信号**：
- `rulesChanged()`：规则变更信号

#### 4.2.4 ThreadPool - 线程池单例

**文件**：[ThreadPool.h](file:///Users/cxd/QT6-main/src/Calculation/ThreadPool.h)

基于 `QThreadPool` 的全局线程池单例封装。

| 方法 | 说明 |
|------|------|
| `instance()` | 获取单例实例 |
| `pool()` | 获取底层 QThreadPool 指针 |
| `maxThreadCount()` / `setMaxThreadCount(count)` | 最大线程数 |
| `activeThreadCount()` | 当前活动线程数 |

---

### 4.3 Excel 处理模块

**位置**：[src/Excel/](file:///Users/cxd/QT6-main/src/Excel)

**职责**：Excel 文件的读写操作，基于 QXlsx 库封装。

#### 4.3.1 ExcelEngine - Excel 底层引擎

**文件**：[ExcelEngine.h](file:///Users/cxd/QT6-main/src/Excel/ExcelEngine.h)

封装 QXlsx 的底层读写操作，提供进度反馈。

| 方法 | 说明 |
|------|------|
| `readExcel(filePath, columnMapping)` | 读取 Excel，返回订单列表 |
| `writeExcel(filePath, orders, headers)` | 写入订单列表到 Excel |
| `getHeaders(filePath)` | 获取表头列表 |
| `countRowsFast(filePath)` | 快速统计行数 |

**内部常量**：
- `BATCH_SIZE = 10000`：批量处理大小
- `MAX_THREADS = 8`：最大线程数

**信号**：
- `progressUpdated(int percent)`
- `errorOccurred(const QString &error)`

#### 4.3.2 ExcelImporter - Excel 导入器

**文件**：[ExcelImporter.h](file:///Users/cxd/QT6-main/src/Excel/ExcelImporter.h)

面向业务的 Excel 导入封装，支持自动列映射。

| 方法 | 说明 |
|------|------|
| `importFromFile(filePath)` | 导入（自动检测列映射） |
| `importFromFile(filePath, columnMapping)` | 导入（自定义列映射） |
| `getAvailableHeaders(filePath)` | 获取可用表头 |

**自动列映射 `autoDetectColumns`**：
根据表头名称自动识别并映射到业务字段（如"运单号"→ waybillNo，"重量"→ actualWeight 等）。

#### 4.3.3 ExcelExporter - Excel 导出器

**文件**：[ExcelExporter.h](file:///Users/cxd/QT6-main/src/Excel/ExcelExporter.h)

面向业务的 Excel 导出封装。

| 方法 | 说明 |
|------|------|
| `exportToFile(filePath, orders)` | 导出订单明细 |
| `exportSummary(filePath, orders)` | 导出汇总数据 |

---

### 4.4 UI 界面模块

**位置**：[src/UI/](file:///Users/cxd/QT6-main/src/UI)

**职责**：所有用户界面组件。

#### 4.4.1 MainWindow - 主窗口

**文件**：[MainWindow.h](file:///Users/cxd/QT6-main/src/MainWindow.h)

应用程序主窗口，统筹所有功能入口。

**核心属性**：
- `m_currentOrders`：当前订单数据
- `m_currentFilePaths`：当前导入的文件路径
- `m_calculator`：运费计算器实例
- `m_ruleManager`：规则管理器实例
- `m_importWatcher` / `m_exportWatcher` / `m_batchWatcher`：异步操作监视
- `m_currentPage` / `m_pageSize` / `m_totalPages`：分页参数

**核心槽函数**：

| 槽函数 | 触发场景 |
|--------|----------|
| `onImportClicked()` | 点击导入按钮 |
| `onCalculateClicked()` | 点击计算按钮 |
| `onExportClicked()` | 点击导出按钮 |
| `onQuickTestClicked()` | 点击快速测试 |
| `onRuleManageClicked()` | 点击规则管理 |
| `onGlobalRulesClicked()` | 点击全局规则 |
| `onCalculationComplete(total, error)` | 计算完成 |
| `onFirstPage/Prev/Next/Last()` | 分页导航 |

**私有方法**：
- `setupConnections()`：建立信号槽连接
- `setupStyle()` / `applyLightStyle()`：设置样式
- `setupStatusBar()`：设置状态栏
- `updateTableView()`：更新表格视图
- `importFilesAsync(filePaths)`：异步导入文件
- `exportFilesAsync(outputDir, mergeExport, perClientExport)`：异步导出
- `showCenterProgress() / hideCenterProgress()`：居中进度提示

#### 4.4.2 RuleManagerDialog - 规则管理对话框

**文件**：[RuleManagerDialog.h](file:///Users/cxd/QT6-main/src/UI/RuleManagerDialog.h)

**UI文件**：[RuleManagerDialog.ui](file:///Users/cxd/QT6-main/src/UI/RuleManagerDialog.ui)

规则管理的主对话框，集成客户管理和价格表管理。

**功能**：
- 客户列表的增删改查
- 选中客户的价格规则编辑
- 批量导入客户
- 重置为默认设置

#### 4.4.3 CustomerDialog - 客户规则对话框

**文件**：[CustomerDialog.h](file:///Users/cxd/QT6-main/src/UI/CustomerDialog.h)

客户规则编辑对话框，左侧客户列表，右侧编辑区。

**编辑内容**：
- 客户名称
- 映射表头
- 计算模式选择
- 是否使用默认报价
- 自定义价格表（表格编辑）

#### 4.4.4 PriceTableDialog - 价格表对话框

**文件**：[PriceTableDialog.h](file:///Users/cxd/QT6-main/src/UI/PriceTableDialog.h)

默认报价表和区域-省份映射管理对话框。

**Tab页**：
1. 默认价格表（表格编辑，支持导入导出Excel）
2. 区域-省份映射（树形结构编辑）

#### 4.4.5 GlobalRulesDialog - 全局规则对话框

**文件**：[GlobalRulesDialog.h](file:///Users/cxd/QT6-main/src/UI/GlobalRulesDialog.h)

全局规则配置对话框。

**Tab页**：
1. 无重量默认价格设置
2. 活动规则表格
3. 临时加价规则表格
4. 省份加价规则表格

#### 4.4.6 QuickTestWidget - 快速测试组件

**文件**：[QuickTestWidget.h](file:///Users/cxd/QT6-main/src/UI/QuickTestWidget.h)

单条运费快速测算对话框。

**输入**：
- 客户规则选择
- 目的省份
- 重量
- 计算模式

**输出**：计算结果实时显示

#### 4.4.7 ProgressWidget - 进度条组件

**文件**：[ProgressWidget.h](file:///Users/cxd/QT6-main/src/UI/ProgressWidget.h)

可复用的进度条组件，包含进度条、百分比标签、状态文本和取消按钮。

---

### 4.5 Utils 工具模块

**位置**：[src/Utils/](file:///Users/cxd/QT6-main/src/Utils)

#### 4.5.1 Logger - 日志工具

**文件**：[Logger.h](file:///Users/cxd/QT6-main/src/Utils/Logger.h)

线程安全的单例日志记录器。

**日志级别**：
- `DEBUG`：调试信息
- `INFO`：一般信息
- `WARNING`：警告
- `ERROR`：错误

**方法**：
- `instance()`：获取单例
- `setLogFile(filePath)`：设置日志文件路径
- `debug(msg)` / `info(msg)` / `warning(msg)` / `error(msg)`
- `log(level, message)`：通用日志方法

**特性**：
- 基于 `QFile + QTextStream` 写入
- `QMutex` 保证多线程安全
- 日志格式包含时间戳和级别

#### 4.5.2 ConfigManager - 配置管理器

**文件**：[ConfigManager.h](file:///Users/cxd/QT6-main/src/Utils/ConfigManager.h)

基于 `QSettings` 的应用配置管理。

**配置项**：

| 配置项 | 类型 | 说明 |
|--------|------|------|
| `defaultThreadCount` | int | 默认线程数 |
| `batchSize` | int | 批处理大小 |
| `lastImportPath` | QString | 上次导入路径 |
| `lastExportPath` | QString | 上次导出路径 |
| `rulesFilePath` | QString | 规则文件路径 |

**方法**：
- 每个配置项都有对应的 getter/setter
- `sync()`：同步写入磁盘

---

## 5. 关键类与函数说明

### 5.1 类关系图

```
QObject
├── MainWindow
├── FreightCalculator
├── RuleManager
├── ExcelEngine
├── ExcelImporter
├── ExcelExporter
├── ConfigManager
└── ThreadPool（内含 QThreadPool）

QWidget / QDialog
├── MainWindow（继承 QMainWindow）
├── CustomerDialog
├── PriceTableDialog
├── RuleManagerDialog
├── GlobalRulesDialog
├── QuickTestWidget
└── ProgressWidget

纯数据结构/工具类
├── OrderData（struct）
├── Customer
├── CalculationRule（全静态）
├── SimdCalculator（全静态）
└── Logger（单例）
```

### 5.2 核心流程函数详解

#### 5.2.1 主程序入口

**文件**：[main.cpp](file:///Users/cxd/QT6-main/src/main.cpp)

```
main(argc, argv)
├── 创建 QApplication
├── 设置应用信息（名称、版本、组织）
├── 设置应用图标
├── 加载全局样式表（style.qss）
├── 初始化日志（freight_calculator.log）
├── 创建并显示 MainWindow
└── 进入事件循环 app.exec()
```

#### 5.2.2 运费计算完整流程

**入口**：`FreightCalculator::calculateFreight(order, rule)`

```
calculateFreight(order, rule)
│
├── 1. 确定计算模式，计算有效重量
│   └── CalculationRule::getEffectiveWeight(actual, vol, mode)
│
├── 2. 有效重量为 0 且配置了无重量默认价
│   ├── 返回默认价
│   └── 应用全局调整（活动/临时加价/省份加价）
│
├── 3. 查找价格规则
│   ├── buildRuleCache(rule) ── 构建/获取缓存
│   └── findPriceRule(province, rule)
│       ├── 优先查客户自定义规则缓存
│       └── 无匹配则查默认规则缓存（模糊匹配）
│
├── 4. 匹配重量段并计算基础运费
│   └── 遍历 priceRule.segments
│       ├── isWeightInRange(effectiveWeight, seg)
│       └── 根据模式调用:
│           ├── calculateStandard()        ── 标准首续重
│           ├── calculateFullAdditional() ── 全续
│           └── calculateHundredGram()    ── 百克续
│
├── 5. 应用全局调整
│   ├── applyActivityRules(freight, activityRules, date)
│   ├── applyTempPriceIncrease(freight, tempIncreases, date)
│   └── applyProvincePriceIncrease(freight, provinceIncreases, province)
│
└── 6. 四舍五入保留两位小数，返回结果
```

#### 5.2.3 批量计算流程

**入口**：`FreightCalculator::calculateBatch(orders, rule, threadCount)`

```
calculateBatch(orders, rule, threadCount)
│
├── 构建规则缓存 buildRuleCache(rule)
│
├── 按批次拆分（BATCH_SIZE = 10000）
│
└── 使用 QtConcurrent::map 并行处理
    │
    ├── 每个线程处理一批订单
    │   └── 逐条调用 calculateFreight()
    │
    └── 进度报告（原子计数器 + 节流更新）
```

---

## 6. 依赖关系

### 6.1 外部依赖

| 依赖 | 版本 | 用途 | 引入方式 |
|------|------|------|----------|
| Qt6::Core | 6.12.0+ | 核心框架 | find_package |
| Qt6::Widgets | 6.12.0+ | GUI 组件 | find_package |
| Qt6::Concurrent | 6.12.0+ | 并发计算 | find_package |
| QXlsx | 1.4.4 | Excel 读写 | add_subdirectory（静态库） |

### 6.2 内部模块依赖

```
MainWindow
├── DataModel (OrderData, PriceTable)
├── Calculation (FreightCalculator, RuleManager)
├── Excel (ExcelImporter, ExcelExporter)
├── UI (各种Dialog/Widget)
└── Utils (Logger, ConfigManager)

Calculation
├── DataModel (所有数据结构)
└── Utils (Logger)

Excel
└── DataModel (OrderData)

UI (Dialogs)
├── Calculation (RuleManager)
└── DataModel (PriceTable)

DataModel
└── (无内部依赖，纯数据结构)

Utils
└── (无内部依赖)
```

### 6.3 模块耦合度分析

| 模块 | 被依赖数 | 依赖数 | 耦合类型 |
|------|----------|--------|----------|
| DataModel | 5 | 0 | 低耦合（纯数据） |
| Calculation | 2 | 2 | 中耦合（核心业务） |
| Excel | 1 | 1 | 低耦合 |
| UI | 0 | 2 | 高耦合（界面层） |
| Utils | 2 | 0 | 低耦合（工具） |

---

## 7. 计算规则详解

### 7.1 计算模式

| 模式 | 字符串标识 | 有效重量 | 计算方式 |
|------|-----------|----------|----------|
| 实际重量 | "实际重量" | max(实际重, 体积重) | 首重 + 续重 |
| 拉均重 | "拉均重" | max(实际重, 体积重) | 首重 + 续重（额外有拉均重加价） |
| 百克续 | "百克续" | max(实际重, 体积重) | 按百克续重 |
| 全续 | "全续" | max(实际重, 体积重) | 全重量续重（无首重） |

### 7.2 标准计算（首重+续重）

```
公式：
运费 = 首重价格 + ceil(重量 - 首重重量) × 续重单价

特殊规则：
- 当重量段为 30kg 以上且无上限时，首重按 3kg 计算
- 向上取整使用 ceil() 函数
```

### 7.3 百克续计算

```
公式：
运费 = 首重价格 + ceil((重量 - 起始重量) × 10) × (续重单价 / 10)

说明：
- 以 100 克为续重单位
- 精度更高，适用于小件物品
```

### 7.4 全续计算

```
公式：
运费 = ceil(重量) × 续重单价

说明：
- 没有首重概念
- 每公斤都是续重价格
- 适用于重货场景
```

### 7.5 全局调整顺序

运费计算结果依次应用以下调整：

1. **活动规则**：折扣或固定减免
2. **临时加价**：固定金额或比例加价
3. **省份加价**：按省份固定加价

### 7.6 区域划分（默认报价表）

| 区域 | 包含省份 |
|------|----------|
| 一区 | 江苏、浙江、安徽、上海 |
| 二区 | 山东、广东、福建、北京、河南、湖北、湖南、江西、天津、河北 |
| 三区 | 山西、广西、四川、重庆、陕西、贵州、辽宁、吉林、黑龙江、云南 |
| 四区 | 海南、甘肃、青海、内蒙古、宁夏 |
| 五区 | 新疆、西藏 |

### 7.7 省份匹配规则

1. **规范化处理**：去掉"省"、"市"、"自治区"、"特别行政区"等后缀
2. **精确匹配**：规范化后的名称完全一致
3. **包含匹配**：一方包含另一方
4. **两字前缀匹配**：前两个字相同即匹配（模糊匹配兜底）

---

## 8. 构建与运行

### 8.1 前置要求

| 工具 | 推荐版本 | 说明 |
|------|----------|------|
| Qt | 6.12.0+ | 含 MinGW 或 MSVC 编译器 |
| CMake | 3.16+ | 构建系统 |
| Ninja | - | 构建生成器（可选，推荐） |
| Python | 3.x | 测试脚本（可选） |

### 8.2 Windows 构建

#### 方式一：Batch 脚本

```bash
build.bat
```

脚本会自动完成：
1. 下载 QXlsx 库（如不存在）
2. CMake 配置（MinGW + Ninja）
3. Ninja 编译
4. windeployqt 收集依赖并打包

#### 方式二：PowerShell 脚本

```powershell
.\build.ps1
```

功能同 build.bat，PowerShell 版本。

#### 方式三：手动 CMake

```bash
mkdir build
cd build
cmake .. -G "Ninja" -DCMAKE_PREFIX_PATH=<Qt路径> -DCMAKE_BUILD_TYPE=Release
ninja
```

### 8.3 macOS / Linux 构建

```bash
mkdir build
cd build
cmake .. -DCMAKE_PREFIX_PATH=<Qt路径> -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 8.4 运行

构建完成后，在输出目录找到可执行文件：
- Windows: `WukongFreight.exe`
- macOS: `WukongFreight.app`
- Linux: `WukongFreight`

### 8.5 输出目录结构（Windows打包后）

```
dist/
├── WukongFreight.exe        # 主程序
├── Qt6Core.dll              # Qt 核心库
├── Qt6Gui.dll               # Qt GUI 库
├── Qt6Widgets.dll           # Qt Widgets 库
├── Qt6Concurrent.dll        # Qt 并发库
├── platforms/               # Qt 平台插件
├── styles/                  # 样式插件
└── ...                      # 其他依赖
```

---

## 9. 配置与持久化

### 9.1 系统配置（QSettings）

**存储位置**（因平台而异）：
- Windows: 注册表 `HKEY_CURRENT_USER\Software\杭州喵喵至家网络有限公司\悟空运费结算`
- macOS: `~/Library/Preferences/com.杭州喵喵至家网络有限公司.悟空运费结算.plist`
- Linux: `~/.config/杭州喵喵至家网络有限公司/悟空运费结算.conf`

**配置项**：参见 [4.5.2 ConfigManager](#452-configmanager---配置管理器)

### 9.2 规则持久化（JSON）

规则数据保存为 JSON 文件，默认文件名为 `rules.json`。

**JSON 结构示意**：

```json
{
  "customers": {
    "客户A": {
      "customerName": "客户A",
      "mappingHeader": "结算对象",
      "calculationMode": "实际重量",
      "useDefaultPrice": true,
      "customPriceRules": []
    }
  },
  "defaultPriceTable": [...],
  "globalRules": {
    "noWeightDefaultPrice": 0,
    "activityRules": [...],
    "tempPriceIncreases": [...],
    "provincePriceIncreases": [...]
  }
}
```

### 9.3 日志文件

**文件名**：`freight_calculator.log`

**存储位置**：程序运行目录

**日志格式**：
```
[时间戳] [级别] 消息内容
```

---

## 10. 性能优化

### 10.1 多线程并行计算

- 使用 `QtConcurrent::map` 进行数据并行
- 按 10000 条/批次 拆分任务
- 可配置线程数（默认 8 线程）
- 原子计数器保证进度统计线程安全

### 10.2 SIMD 向量化

- AVX2：单指令处理 4 个 double（256位）
- SSE4.1：单指令处理 2 个 double（128位）
- 运行时 CPU 特性检测，自动降级
- SoA 数据布局优化缓存命中率

### 10.3 规则缓存

- 省份名称规范化结果缓存
- 自定义规则按省份哈希索引
- 默认规则全局缓存（一次性构建）
- 避免热路径中的 QString 操作

### 10.4 进度节流

- 进度更新使用时间戳节流
- 避免频繁的 UI 刷新导致卡顿
- 确保进度条流畅同时不影响计算性能

### 10.5 Excel 批量读写

- 分批次读取写入，减少内存占用
- 自动列映射检测，减少手动配置
- QXlsx 底层优化（直接操作 xlsx zip 结构）

---

## 附录

### A. 样式主题

系统采用现代浅色主题，主色调为橙色系（#ff8c32 / #ff7f20），详见 [style.qss](file:///Users/cxd/QT6-main/resources/style.qss)。

### B. 第三方库说明

- **QXlsx**：MIT 协议，Excel xlsx 文件读写库，QtXlsxWriter 的社区继任者
  - GitHub: https://github.com/QtExcel/QXlsx
  - 版本: 1.4.4
  - 集成方式：源码级子模块，静态编译

### C. Python 参考实现

项目包含 Python 版本的参考实现，用于验证计算逻辑：
- [calculate_freight.py](file:///Users/cxd/QT6-main/calculate_freight.py)：完整计算逻辑参考
- [test_e2e.py](file:///Users/cxd/QT6-main/test_e2e.py)：端到端测试
- [test_excel.py](file:///Users/cxd/QT6-main/test_excel.py)：Excel 处理测试
- [calc_pandas.py](file:///Users/cxd/QT6-main/calc_pandas.py)：Pandas 版计算

### D. 图标资源

- 应用图标：[app_icon.png](file:///Users/cxd/QT6-main/resources/app_icon.png)
- 资源定义：[resources.qrc](file:///Users/cxd/QT6-main/resources/resources.qrc)

---

*文档版本：1.0*  
*生成时间：2026-07-08*  
*对应项目版本：WukongFreight v1.0.0*
