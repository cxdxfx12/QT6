#include "GlobalRulesDialog.h"
#include "Calculation/RuleManager.h"
#include "DataModel/PriceTable.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QComboBox>
#include <QCheckBox>
#include <QDateTimeEdit>
#include <QHeaderView>
#include <QLabel>

GlobalRulesDialog::GlobalRulesDialog(RuleManager *ruleManager, QWidget *parent)
    : QDialog(parent)
    , m_ruleManager(ruleManager)
{
    setupUI();
    loadRules();
}

void GlobalRulesDialog::setupUI()
{
    setWindowTitle(QStringLiteral("全局规则管理"));
    setMinimumSize(800, 600);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    // ========== 顶部：无重量默认价格 ==========
    auto *topGroup = new QGroupBox(QStringLiteral("基础设置"), this);
    auto *topLayout = new QFormLayout(topGroup);
    topLayout->setContentsMargins(12, 16, 12, 12);
    topLayout->setSpacing(8);

    m_noWeightPriceSpin = new QDoubleSpinBox(this);
    m_noWeightPriceSpin->setRange(0, 99999);
    m_noWeightPriceSpin->setDecimals(2);
    m_noWeightPriceSpin->setSuffix(QStringLiteral(" 元"));
    m_noWeightPriceSpin->setMinimumWidth(200);

    topLayout->addRow(QStringLiteral("无重量默认价格："), m_noWeightPriceSpin);

    // 拉均重参数
    m_avgBaseSpin = new QDoubleSpinBox(this);
    m_avgBaseSpin->setRange(0, 99999);
    m_avgBaseSpin->setDecimals(2);
    m_avgBaseSpin->setSuffix(QStringLiteral(" kg"));
    m_avgBaseSpin->setMinimumWidth(200);
    topLayout->addRow(QStringLiteral("均重基准："), m_avgBaseSpin);

    m_avgLimitSpin = new QDoubleSpinBox(this);
    m_avgLimitSpin->setRange(0, 99999);
    m_avgLimitSpin->setDecimals(2);
    m_avgLimitSpin->setSuffix(QStringLiteral(" kg"));
    m_avgLimitSpin->setMinimumWidth(200);
    topLayout->addRow(QStringLiteral("均重上限："), m_avgLimitSpin);

    m_avgStepSpin = new QDoubleSpinBox(this);
    m_avgStepSpin->setRange(0, 99999);
    m_avgStepSpin->setDecimals(2);
    m_avgStepSpin->setSuffix(QStringLiteral(" kg"));
    m_avgStepSpin->setMinimumWidth(200);
    topLayout->addRow(QStringLiteral("均重步长："), m_avgStepSpin);

    m_avgSurchargeSpin = new QDoubleSpinBox(this);
    m_avgSurchargeSpin->setRange(0, 99999);
    m_avgSurchargeSpin->setDecimals(2);
    m_avgSurchargeSpin->setSuffix(QStringLiteral(" 元"));
    m_avgSurchargeSpin->setMinimumWidth(200);
    topLayout->addRow(QStringLiteral("均重加价："), m_avgSurchargeSpin);

    mainLayout->addWidget(topGroup);

    // ========== 中间：Tab页 ==========
    m_tabWidget = new QTabWidget(this);

    // ---- Tab1: 活动加价 ----
    auto *activityTab = new QWidget(m_tabWidget);
    auto *activityTabLayout = new QVBoxLayout(activityTab);
    activityTabLayout->setContentsMargins(8, 8, 8, 8);
    activityTabLayout->setSpacing(8);

    m_activityTable = new QTableWidget(0, 6, activityTab);
    m_activityTable->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("名称") << QStringLiteral("开始时间")
        << QStringLiteral("结束时间") << QStringLiteral("加价数值")
        << QStringLiteral("类型") << QStringLiteral("启用"));
    m_activityTable->horizontalHeader()->setStretchLastSection(false);
    m_activityTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_activityTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_activityTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_activityTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_activityTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_activityTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_activityTable->setColumnWidth(0, 150);
    m_activityTable->setColumnWidth(5, 60);
    m_activityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_activityTable->setAlternatingRowColors(true);
    m_activityTable->setSortingEnabled(false);

    auto *activityBtnLayout = new QHBoxLayout();
    activityBtnLayout->setSpacing(8);

    m_addActivityBtn = new QPushButton(QStringLiteral("添加行"), activityTab);
    m_addActivityBtn->setObjectName(QStringLiteral("smallButton"));
    m_addActivityBtn->setMinimumHeight(36);

    m_removeActivityBtn = new QPushButton(QStringLiteral("删除行"), activityTab);
    m_removeActivityBtn->setObjectName(QStringLiteral("smallButton"));
    m_removeActivityBtn->setMinimumHeight(36);

    activityBtnLayout->addWidget(m_addActivityBtn);
    activityBtnLayout->addWidget(m_removeActivityBtn);
    activityBtnLayout->addStretch();

    activityTabLayout->addWidget(m_activityTable, 1);
    activityTabLayout->addLayout(activityBtnLayout);

    m_tabWidget->addTab(activityTab, QStringLiteral("活动加价"));

    // ---- Tab2: 临时加价 ----
    auto *tempTab = new QWidget(m_tabWidget);
    auto *tempTabLayout = new QVBoxLayout(tempTab);
    tempTabLayout->setContentsMargins(8, 8, 8, 8);
    tempTabLayout->setSpacing(8);

    m_tempIncreaseTable = new QTableWidget(0, 6, tempTab);
    m_tempIncreaseTable->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("名称") << QStringLiteral("开始时间")
        << QStringLiteral("结束时间") << QStringLiteral("加价金额/比例")
        << QStringLiteral("类型") << QStringLiteral("启用"));
    m_tempIncreaseTable->horizontalHeader()->setStretchLastSection(false);
    m_tempIncreaseTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    m_tempIncreaseTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_tempIncreaseTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_tempIncreaseTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_tempIncreaseTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_tempIncreaseTable->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Fixed);
    m_tempIncreaseTable->setColumnWidth(0, 150);
    m_tempIncreaseTable->setColumnWidth(5, 60);
    m_tempIncreaseTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tempIncreaseTable->setAlternatingRowColors(true);
    m_tempIncreaseTable->setSortingEnabled(false);

    auto *tempBtnLayout = new QHBoxLayout();
    tempBtnLayout->setSpacing(8);

    m_addTempBtn = new QPushButton(QStringLiteral("添加行"), tempTab);
    m_addTempBtn->setObjectName(QStringLiteral("smallButton"));
    m_addTempBtn->setMinimumHeight(36);

    m_removeTempBtn = new QPushButton(QStringLiteral("删除行"), tempTab);
    m_removeTempBtn->setObjectName(QStringLiteral("smallButton"));
    m_removeTempBtn->setMinimumHeight(36);

    tempBtnLayout->addWidget(m_addTempBtn);
    tempBtnLayout->addWidget(m_removeTempBtn);
    tempBtnLayout->addStretch();

    tempTabLayout->addWidget(m_tempIncreaseTable, 1);
    tempTabLayout->addLayout(tempBtnLayout);

    m_tabWidget->addTab(tempTab, QStringLiteral("临时加价"));

    // ---- Tab3: 省份加价 ----
    auto *provinceTab = new QWidget(m_tabWidget);
    auto *provinceTabLayout = new QVBoxLayout(provinceTab);
    provinceTabLayout->setContentsMargins(8, 8, 8, 8);
    provinceTabLayout->setSpacing(8);

    m_provinceIncreaseTable = new QTableWidget(0, 3, provinceTab);
    m_provinceIncreaseTable->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("省份") << QStringLiteral("加价金额") << QStringLiteral("启用"));
    m_provinceIncreaseTable->horizontalHeader()->setStretchLastSection(false);
    m_provinceIncreaseTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_provinceIncreaseTable->setColumnWidth(1, 100);
    m_provinceIncreaseTable->setColumnWidth(2, 60);
    m_provinceIncreaseTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_provinceIncreaseTable->setAlternatingRowColors(true);
    m_provinceIncreaseTable->setSortingEnabled(false);

    auto *provinceBtnLayout = new QHBoxLayout();
    provinceBtnLayout->setSpacing(8);

    m_addProvinceBtn = new QPushButton(QStringLiteral("添加行"), provinceTab);
    m_addProvinceBtn->setObjectName(QStringLiteral("smallButton"));
    m_addProvinceBtn->setMinimumHeight(36);

    m_removeProvinceBtn = new QPushButton(QStringLiteral("删除行"), provinceTab);
    m_removeProvinceBtn->setObjectName(QStringLiteral("smallButton"));
    m_removeProvinceBtn->setMinimumHeight(36);

    provinceBtnLayout->addWidget(m_addProvinceBtn);
    provinceBtnLayout->addWidget(m_removeProvinceBtn);
    provinceBtnLayout->addStretch();

    provinceTabLayout->addWidget(m_provinceIncreaseTable, 1);
    provinceTabLayout->addLayout(provinceBtnLayout);

    m_tabWidget->addTab(provinceTab, QStringLiteral("省份加价"));

    mainLayout->addWidget(m_tabWidget, 1);

    // ========== 底部按钮 ==========
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(12);

    m_saveBtn = new QPushButton(QStringLiteral("保存"), this);
    m_saveBtn->setObjectName(QStringLiteral("primaryButton"));
    m_saveBtn->setMinimumHeight(36);
    m_saveBtn->setFixedWidth(120);

    m_cancelBtn = new QPushButton(QStringLiteral("取消"), this);
    m_cancelBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_cancelBtn->setMinimumHeight(36);
    m_cancelBtn->setFixedWidth(120);

    bottomLayout->addStretch();
    bottomLayout->addWidget(m_saveBtn);
    bottomLayout->addWidget(m_cancelBtn);

    mainLayout->addLayout(bottomLayout);

    // ========== 信号连接 ==========
    connect(m_saveBtn, &QPushButton::clicked, this, &GlobalRulesDialog::onSaveClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &GlobalRulesDialog::onCancelClicked);

    // 活动规则：添加/删除行
    connect(m_addActivityBtn, &QPushButton::clicked, this, [this]() {
        int row = m_activityTable->rowCount();
        m_activityTable->insertRow(row);

        m_activityTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("新活动")));

        auto *startEdit = new QDateTimeEdit(QDateTime::currentDateTime(), m_activityTable);
        startEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
        startEdit->setCalendarPopup(true);
        m_activityTable->setCellWidget(row, 1, startEdit);

        auto *endEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(7), m_activityTable);
        endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
        endEdit->setCalendarPopup(true);
        m_activityTable->setCellWidget(row, 2, endEdit);

        auto *valueSpin = new QDoubleSpinBox(m_activityTable);
        valueSpin->setRange(0.0, 9999);
        valueSpin->setDecimals(2);
        valueSpin->setValue(0.1);
        m_activityTable->setCellWidget(row, 3, valueSpin);

        auto *typeCombo = new QComboBox(m_activityTable);
        typeCombo->addItems(QStringList() << QStringLiteral("比例") << QStringLiteral("固定金额"));
        m_activityTable->setCellWidget(row, 4, typeCombo);

        QWidget *checkWidget = new QWidget(m_activityTable);
        auto *checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        auto *activeCheck = new QCheckBox(checkWidget);
        activeCheck->setChecked(true);
        checkLayout->addWidget(activeCheck);
        m_activityTable->setCellWidget(row, 5, checkWidget);

        m_activityTable->selectRow(row);
        m_activityTable->scrollToBottom();
    });

    connect(m_removeActivityBtn, &QPushButton::clicked, this, [this]() {
        int row = m_activityTable->currentRow();
        if (row < 0) return;
        if (QMessageBox::question(this, QStringLiteral("确认删除"),
                QStringLiteral("确定要删除这条活动加价规则吗？")) != QMessageBox::Yes)
            return;
        m_activityTable->removeRow(row);
    });

    // 临时加价：添加/删除行
    connect(m_addTempBtn, &QPushButton::clicked, this, [this]() {
        int row = m_tempIncreaseTable->rowCount();
        m_tempIncreaseTable->insertRow(row);

        m_tempIncreaseTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("新规则")));

        auto *startEdit = new QDateTimeEdit(QDateTime::currentDateTime(), m_tempIncreaseTable);
        startEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
        startEdit->setCalendarPopup(true);
        m_tempIncreaseTable->setCellWidget(row, 1, startEdit);

        auto *endEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(7), m_tempIncreaseTable);
        endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
        endEdit->setCalendarPopup(true);
        m_tempIncreaseTable->setCellWidget(row, 2, endEdit);

        auto *valueSpin = new QDoubleSpinBox(m_tempIncreaseTable);
        valueSpin->setRange(0.01, 9999);
        valueSpin->setDecimals(2);
        valueSpin->setValue(1.0);
        m_tempIncreaseTable->setCellWidget(row, 3, valueSpin);

        auto *typeCombo = new QComboBox(m_tempIncreaseTable);
        typeCombo->addItems(QStringList() << QStringLiteral("固定金额") << QStringLiteral("比例"));
        m_tempIncreaseTable->setCellWidget(row, 4, typeCombo);

        QWidget *checkWidget = new QWidget(m_tempIncreaseTable);
        auto *checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        auto *activeCheck = new QCheckBox(checkWidget);
        activeCheck->setChecked(true);
        checkLayout->addWidget(activeCheck);
        m_tempIncreaseTable->setCellWidget(row, 5, checkWidget);

        m_tempIncreaseTable->selectRow(row);
        m_tempIncreaseTable->scrollToBottom();
    });

    connect(m_removeTempBtn, &QPushButton::clicked, this, [this]() {
        int row = m_tempIncreaseTable->currentRow();
        if (row < 0) return;
        if (QMessageBox::question(this, QStringLiteral("确认删除"),
                QStringLiteral("确定要删除这条临时加价规则吗？")) != QMessageBox::Yes)
            return;
        m_tempIncreaseTable->removeRow(row);
    });

    // 省份加价：添加/删除行
    connect(m_addProvinceBtn, &QPushButton::clicked, this, [this]() {
        // 检查是否已存在所有省份
        int totalProvs = 31;
        if (m_provinceIncreaseTable->rowCount() >= totalProvs) {
            QMessageBox::information(this, QStringLiteral("提示"),
                QStringLiteral("所有省份都已添加。"));
            return;
        }

        // 找第一个还没添加的省份作为默认值
        QStringList allProvs = {
            QStringLiteral("江苏"), QStringLiteral("浙江"), QStringLiteral("安徽"), QStringLiteral("上海"),
            QStringLiteral("山东"), QStringLiteral("广东"), QStringLiteral("福建"), QStringLiteral("北京"),
            QStringLiteral("河南"), QStringLiteral("湖北"), QStringLiteral("湖南"), QStringLiteral("江西"),
            QStringLiteral("天津"), QStringLiteral("河北"), QStringLiteral("山西"), QStringLiteral("广西"),
            QStringLiteral("四川"), QStringLiteral("重庆"), QStringLiteral("陕西"), QStringLiteral("贵州"),
            QStringLiteral("辽宁"), QStringLiteral("吉林"), QStringLiteral("黑龙江"), QStringLiteral("云南"),
            QStringLiteral("海南"), QStringLiteral("甘肃"), QStringLiteral("青海"), QStringLiteral("内蒙古"),
            QStringLiteral("宁夏"), QStringLiteral("新疆"), QStringLiteral("西藏")
        };
        QSet<QString> existing;
        for (int i = 0; i < m_provinceIncreaseTable->rowCount(); ++i) {
            auto *combo = qobject_cast<QComboBox*>(m_provinceIncreaseTable->cellWidget(i, 0));
            if (combo) existing.insert(combo->currentText());
        }
        QString defaultProv;
        for (const QString &p : allProvs) {
            if (!existing.contains(p)) {
                defaultProv = p;
                break;
            }
        }

        int row = m_provinceIncreaseTable->rowCount();
        m_provinceIncreaseTable->insertRow(row);

        auto *provinceCombo = new QComboBox(m_provinceIncreaseTable);
        populateProvinceCombo(provinceCombo);
        if (!defaultProv.isEmpty()) {
            int idx = provinceCombo->findText(defaultProv);
            if (idx >= 0) provinceCombo->setCurrentIndex(idx);
        }
        m_provinceIncreaseTable->setCellWidget(row, 0, provinceCombo);

        auto *amountSpin = new QDoubleSpinBox(m_provinceIncreaseTable);
        amountSpin->setRange(0.01, 9999);
        amountSpin->setDecimals(2);
        amountSpin->setSuffix(QStringLiteral(" 元"));
        amountSpin->setValue(1.0);
        m_provinceIncreaseTable->setCellWidget(row, 1, amountSpin);

        QWidget *checkWidget = new QWidget(m_provinceIncreaseTable);
        auto *checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        auto *activeCheck = new QCheckBox(checkWidget);
        activeCheck->setChecked(true);
        checkLayout->addWidget(activeCheck);
        m_provinceIncreaseTable->setCellWidget(row, 2, checkWidget);

        m_provinceIncreaseTable->selectRow(row);
        m_provinceIncreaseTable->scrollToBottom();
    });

    connect(m_removeProvinceBtn, &QPushButton::clicked, this, [this]() {
        int row = m_provinceIncreaseTable->currentRow();
        if (row < 0) return;
        if (QMessageBox::question(this, QStringLiteral("确认删除"),
                QStringLiteral("确定要删除这条省份加价规则吗？")) != QMessageBox::Yes)
            return;
        m_provinceIncreaseTable->removeRow(row);
    });
}

void GlobalRulesDialog::loadRules()
{
    GlobalRules rules = m_ruleManager->globalRules();

    // 无重量默认价格
    m_noWeightPriceSpin->setValue(rules.noWeightDefaultPrice);

    // 拉均重参数
    m_avgBaseSpin->setValue(rules.avgWeightBase);
    m_avgLimitSpin->setValue(rules.avgWeightUpperLimit);
    m_avgStepSpin->setValue(rules.avgWeightIncrement);
    m_avgSurchargeSpin->setValue(rules.avgWeightSurcharge);

    // 活动规则
    m_activityTable->setRowCount(0);
    for (const ActivityRule &rule : rules.activityRules) {
        int row = m_activityTable->rowCount();
        m_activityTable->insertRow(row);

        m_activityTable->setItem(row, 0, new QTableWidgetItem(rule.name));

        auto *startEdit = new QDateTimeEdit(rule.startTime, m_activityTable);
        startEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
        startEdit->setCalendarPopup(true);
        m_activityTable->setCellWidget(row, 1, startEdit);

        auto *endEdit = new QDateTimeEdit(rule.endTime, m_activityTable);
        endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
        endEdit->setCalendarPopup(true);
        m_activityTable->setCellWidget(row, 2, endEdit);

        auto *valueSpin = new QDoubleSpinBox(m_activityTable);
        valueSpin->setRange(0, 99999);
        valueSpin->setDecimals(2);
        valueSpin->setValue(rule.isFixedAmount ? rule.increaseAmount : rule.increaseRate);
        m_activityTable->setCellWidget(row, 3, valueSpin);

        auto *typeCombo = new QComboBox(m_activityTable);
        typeCombo->addItems(QStringList() << QStringLiteral("比例") << QStringLiteral("固定金额"));
        typeCombo->setCurrentIndex(rule.isFixedAmount ? 1 : 0);
        m_activityTable->setCellWidget(row, 4, typeCombo);

        QWidget *checkWidget = new QWidget(m_activityTable);
        auto *checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        auto *activeCheck = new QCheckBox(checkWidget);
        activeCheck->setChecked(rule.isActive);
        checkLayout->addWidget(activeCheck);
        m_activityTable->setCellWidget(row, 5, checkWidget);
    }

    // 临时加价规则
    m_tempIncreaseTable->setRowCount(0);
    for (const TempPriceIncrease &rule : rules.tempPriceIncreases) {
        int row = m_tempIncreaseTable->rowCount();
        m_tempIncreaseTable->insertRow(row);

        m_tempIncreaseTable->setItem(row, 0, new QTableWidgetItem(rule.name));

        auto *startEdit = new QDateTimeEdit(rule.startTime, m_tempIncreaseTable);
        startEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
        startEdit->setCalendarPopup(true);
        m_tempIncreaseTable->setCellWidget(row, 1, startEdit);

        auto *endEdit = new QDateTimeEdit(rule.endTime, m_tempIncreaseTable);
        endEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd HH:mm"));
        endEdit->setCalendarPopup(true);
        m_tempIncreaseTable->setCellWidget(row, 2, endEdit);

        auto *valueSpin = new QDoubleSpinBox(m_tempIncreaseTable);
        valueSpin->setRange(0, 99999);
        valueSpin->setDecimals(2);
        valueSpin->setValue(rule.isFixedAmount ? rule.increaseAmount : rule.increaseRate);
        m_tempIncreaseTable->setCellWidget(row, 3, valueSpin);

        auto *typeCombo = new QComboBox(m_tempIncreaseTable);
        typeCombo->addItems(QStringList() << QStringLiteral("固定金额") << QStringLiteral("比例"));
        typeCombo->setCurrentIndex(rule.isFixedAmount ? 0 : 1);
        m_tempIncreaseTable->setCellWidget(row, 4, typeCombo);

        auto *activeCheck = new QCheckBox(m_tempIncreaseTable);
        activeCheck->setChecked(rule.isActive);

        QWidget *checkWidget = new QWidget(m_tempIncreaseTable);
        auto *checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        checkLayout->addWidget(activeCheck);
        m_tempIncreaseTable->setCellWidget(row, 5, checkWidget);
    }

    // 省份加价规则
    m_provinceIncreaseTable->setRowCount(0);
    for (const ProvincePriceIncrease &rule : rules.provincePriceIncreases) {
        int row = m_provinceIncreaseTable->rowCount();
        m_provinceIncreaseTable->insertRow(row);

        auto *provinceCombo = new QComboBox(m_provinceIncreaseTable);
        populateProvinceCombo(provinceCombo);
        int idx = provinceCombo->findText(rule.province);
        if (idx >= 0) provinceCombo->setCurrentIndex(idx);
        m_provinceIncreaseTable->setCellWidget(row, 0, provinceCombo);

        auto *amountSpin = new QDoubleSpinBox(m_provinceIncreaseTable);
        amountSpin->setRange(0, 99999);
        amountSpin->setDecimals(2);
        amountSpin->setSuffix(QStringLiteral(" 元"));
        amountSpin->setValue(rule.increaseAmount);
        m_provinceIncreaseTable->setCellWidget(row, 1, amountSpin);

        auto *activeCheck = new QCheckBox(m_provinceIncreaseTable);
        activeCheck->setChecked(rule.isActive);

        QWidget *checkWidget = new QWidget(m_provinceIncreaseTable);
        auto *checkLayout = new QHBoxLayout(checkWidget);
        checkLayout->setContentsMargins(0, 0, 0, 0);
        checkLayout->setAlignment(Qt::AlignCenter);
        checkLayout->addWidget(activeCheck);
        m_provinceIncreaseTable->setCellWidget(row, 2, checkWidget);
    }

    // 列宽自适应
    m_activityTable->resizeColumnsToContents();
    m_tempIncreaseTable->resizeColumnsToContents();
    m_provinceIncreaseTable->resizeColumnsToContents();
}

GlobalRules GlobalRulesDialog::buildGlobalRules() const
{
    GlobalRules rules;
    rules.noWeightDefaultPrice = m_noWeightPriceSpin->value();

    // 拉均重参数
    rules.avgWeightBase = m_avgBaseSpin->value();
    rules.avgWeightUpperLimit = m_avgLimitSpin->value();
    rules.avgWeightIncrement = m_avgStepSpin->value();
    rules.avgWeightSurcharge = m_avgSurchargeSpin->value();

    // 活动规则
    for (int i = 0; i < m_activityTable->rowCount(); ++i) {
        ActivityRule rule;
        rule.name = m_activityTable->item(i, 0) ? m_activityTable->item(i, 0)->text() : QString();

        auto *startEdit = qobject_cast<QDateTimeEdit*>(m_activityTable->cellWidget(i, 1));
        if (startEdit) rule.startTime = startEdit->dateTime();

        auto *endEdit = qobject_cast<QDateTimeEdit*>(m_activityTable->cellWidget(i, 2));
        if (endEdit) rule.endTime = endEdit->dateTime();

        auto *valueSpin = qobject_cast<QDoubleSpinBox*>(m_activityTable->cellWidget(i, 3));
        if (valueSpin) {
            double val = valueSpin->value();
            auto *typeCombo = qobject_cast<QComboBox*>(m_activityTable->cellWidget(i, 4));
            bool isFixed = typeCombo && typeCombo->currentIndex() == 1;
            rule.isFixedAmount = isFixed;
            if (isFixed) {
                rule.increaseAmount = val;
                rule.increaseRate = 0.0;
            } else {
                rule.increaseRate = val;
                rule.increaseAmount = 0;
            }
        }

        QWidget *checkWidget = qobject_cast<QWidget*>(m_activityTable->cellWidget(i, 5));
        if (checkWidget) {
            auto *activeCheck = checkWidget->findChild<QCheckBox*>();
            if (activeCheck) rule.isActive = activeCheck->isChecked();
        }

        rules.activityRules.append(rule);
    }

    // 临时加价规则
    for (int i = 0; i < m_tempIncreaseTable->rowCount(); ++i) {
        TempPriceIncrease rule;
        rule.name = m_tempIncreaseTable->item(i, 0) ? m_tempIncreaseTable->item(i, 0)->text() : QString();

        auto *startEdit = qobject_cast<QDateTimeEdit*>(m_tempIncreaseTable->cellWidget(i, 1));
        if (startEdit) rule.startTime = startEdit->dateTime();

        auto *endEdit = qobject_cast<QDateTimeEdit*>(m_tempIncreaseTable->cellWidget(i, 2));
        if (endEdit) rule.endTime = endEdit->dateTime();

        auto *valueSpin = qobject_cast<QDoubleSpinBox*>(m_tempIncreaseTable->cellWidget(i, 3));
        if (valueSpin) {
            double val = valueSpin->value();
            auto *typeCombo = qobject_cast<QComboBox*>(m_tempIncreaseTable->cellWidget(i, 4));
            bool isFixed = typeCombo && typeCombo->currentIndex() == 0;
            rule.isFixedAmount = isFixed;
            if (isFixed) {
                rule.increaseAmount = val;
                rule.increaseRate = 0;
            } else {
                rule.increaseRate = val;
                rule.increaseAmount = 0;
            }
        }

        QWidget *checkWidget = qobject_cast<QWidget*>(m_tempIncreaseTable->cellWidget(i, 5));
        if (checkWidget) {
            auto *activeCheck = checkWidget->findChild<QCheckBox*>();
            if (activeCheck) rule.isActive = activeCheck->isChecked();
        }

        rules.tempPriceIncreases.append(rule);
    }

    // 省份加价规则
    for (int i = 0; i < m_provinceIncreaseTable->rowCount(); ++i) {
        ProvincePriceIncrease rule;

        auto *provinceCombo = qobject_cast<QComboBox*>(m_provinceIncreaseTable->cellWidget(i, 0));
        if (provinceCombo) rule.province = provinceCombo->currentText();

        auto *amountSpin = qobject_cast<QDoubleSpinBox*>(m_provinceIncreaseTable->cellWidget(i, 1));
        if (amountSpin) rule.increaseAmount = amountSpin->value();

        QWidget *checkWidget = qobject_cast<QWidget*>(m_provinceIncreaseTable->cellWidget(i, 2));
        if (checkWidget) {
            auto *activeCheck = checkWidget->findChild<QCheckBox*>();
            if (activeCheck) rule.isActive = activeCheck->isChecked();
        }

        rules.provincePriceIncreases.append(rule);
    }

    return rules;
}

void GlobalRulesDialog::populateProvinceCombo(QComboBox *combo) const
{
    QStringList provinces = {
        QStringLiteral("江苏"), QStringLiteral("浙江"), QStringLiteral("安徽"), QStringLiteral("上海"),
        QStringLiteral("山东"), QStringLiteral("广东"), QStringLiteral("福建"), QStringLiteral("北京"),
        QStringLiteral("河南"), QStringLiteral("湖北"), QStringLiteral("湖南"), QStringLiteral("江西"),
        QStringLiteral("天津"), QStringLiteral("河北"), QStringLiteral("山西"), QStringLiteral("广西"),
        QStringLiteral("四川"), QStringLiteral("重庆"), QStringLiteral("陕西"), QStringLiteral("贵州"),
        QStringLiteral("辽宁"), QStringLiteral("吉林"), QStringLiteral("黑龙江"), QStringLiteral("云南"),
        QStringLiteral("海南"), QStringLiteral("甘肃"), QStringLiteral("青海"), QStringLiteral("内蒙古"),
        QStringLiteral("宁夏"), QStringLiteral("新疆"), QStringLiteral("西藏")
    };
    combo->addItems(provinces);
}

void GlobalRulesDialog::onSaveClicked()
{
    // 校验：省份加价不能重复
    QSet<QString> provSet;
    for (int i = 0; i < m_provinceIncreaseTable->rowCount(); ++i) {
        auto *combo = qobject_cast<QComboBox*>(m_provinceIncreaseTable->cellWidget(i, 0));
        if (combo) {
            QString prov = combo->currentText();
            if (provSet.contains(prov)) {
                QMessageBox::warning(this, QStringLiteral("保存失败"),
                    QStringLiteral("省份加价存在重复省份：%1").arg(prov));
                m_provinceIncreaseTable->selectRow(i);
                return;
            }
            provSet.insert(prov);
        }
    }

    GlobalRules rules = buildGlobalRules();
    m_ruleManager->setGlobalRules(rules);
    QString filePath = RuleManager::defaultFilePath();
    bool ok = m_ruleManager->saveToFile(filePath);
    if (ok) {
        QMessageBox::information(this, QStringLiteral("保存成功"), QStringLiteral("全局规则已更新并保存"));
    } else {
        QMessageBox::warning(this, QStringLiteral("保存失败"),
            QStringLiteral("全局规则已更新，但写入文件失败\n路径: %1").arg(filePath));
    }
}

void GlobalRulesDialog::onCancelClicked()
{
    reject();
}
