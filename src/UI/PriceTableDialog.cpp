#include "PriceTableDialog.h"
#include "Calculation/RuleManager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QFileDialog>
#include <QApplication>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTreeWidgetItem>

PriceTableDialog::PriceTableDialog(RuleManager *ruleManager, QWidget *parent)
    : QDialog(parent)
    , m_ruleManager(ruleManager)
{
    setupUI();
    applyDarkStyle();
    loadDefaultPriceTable();
    loadRegionMappings();
}

void PriceTableDialog::setupUI()
{
    setWindowTitle(QStringLiteral("报价表管理"));
    setMinimumSize(1000, 700);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(12);

    m_tabWidget = new QTabWidget(this);

    // ========== Tab1: 默认报价表 ==========
    auto *priceTab = new QWidget(m_tabWidget);
    auto *priceTabLayout = new QVBoxLayout(priceTab);
    priceTabLayout->setContentsMargins(8, 8, 8, 8);
    priceTabLayout->setSpacing(8);

    m_priceTable = new QTableWidget(0, 8, priceTab);
    m_priceTable->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("区域") << QStringLiteral("省份")
        << QStringLiteral("最小重量") << QStringLiteral("最大重量")
        << QStringLiteral("首重价格") << QStringLiteral("续重价格")
        << QStringLiteral("百克续") << QStringLiteral("全续"));
    m_priceTable->horizontalHeader()->setStretchLastSection(true);
    m_priceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_priceTable->setAlternatingRowColors(true);
    m_priceTable->setSortingEnabled(false);

    auto *priceBtnLayout = new QHBoxLayout();
    priceBtnLayout->setSpacing(8);

    m_addRowBtn = new QPushButton(QStringLiteral("添加行"), priceTab);
    m_addRowBtn->setObjectName(QStringLiteral("smallButton"));
    m_addRowBtn->setMinimumHeight(36);

    m_removeRowBtn = new QPushButton(QStringLiteral("删除行"), priceTab);
    m_removeRowBtn->setObjectName(QStringLiteral("smallButton"));
    m_removeRowBtn->setMinimumHeight(36);

    m_importBtn = new QPushButton(QStringLiteral("导入Excel"), priceTab);
    m_importBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_importBtn->setMinimumHeight(36);

    m_exportBtn = new QPushButton(QStringLiteral("导出Excel"), priceTab);
    m_exportBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_exportBtn->setMinimumHeight(36);

    priceBtnLayout->addWidget(m_addRowBtn);
    priceBtnLayout->addWidget(m_removeRowBtn);
    priceBtnLayout->addStretch();
    priceBtnLayout->addWidget(m_importBtn);
    priceBtnLayout->addWidget(m_exportBtn);

    priceTabLayout->addWidget(m_priceTable, 1);
    priceTabLayout->addLayout(priceBtnLayout);

    m_tabWidget->addTab(priceTab, QStringLiteral("默认报价表"));

    // ========== Tab2: 区域省份映射 ==========
    auto *regionTab = new QWidget(m_tabWidget);
    auto *regionTabLayout = new QVBoxLayout(regionTab);
    regionTabLayout->setContentsMargins(8, 8, 8, 8);
    regionTabLayout->setSpacing(8);

    m_regionTree = new QTreeWidget(regionTab);
    m_regionTree->setHeaderLabels(QStringList()
        << QStringLiteral("区域/省份"));
    m_regionTree->setAlternatingRowColors(true);
    m_regionTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);

    auto *regionBtnLayout = new QHBoxLayout();
    regionBtnLayout->setSpacing(8);

    m_addRegionBtn = new QPushButton(QStringLiteral("添加区域"), regionTab);
    m_addRegionBtn->setObjectName(QStringLiteral("smallButton"));
    m_addRegionBtn->setMinimumHeight(36);

    m_addProvinceBtn = new QPushButton(QStringLiteral("添加省份"), regionTab);
    m_addProvinceBtn->setObjectName(QStringLiteral("smallButton"));
    m_addProvinceBtn->setMinimumHeight(36);

    m_removeTreeNodeBtn = new QPushButton(QStringLiteral("删除节点"), regionTab);
    m_removeTreeNodeBtn->setObjectName(QStringLiteral("dangerButton"));
    m_removeTreeNodeBtn->setMinimumHeight(36);

    regionBtnLayout->addWidget(m_addRegionBtn);
    regionBtnLayout->addWidget(m_addProvinceBtn);
    regionBtnLayout->addWidget(m_removeTreeNodeBtn);
    regionBtnLayout->addStretch();

    regionTabLayout->addWidget(m_regionTree, 1);
    regionTabLayout->addLayout(regionBtnLayout);

    m_tabWidget->addTab(regionTab, QStringLiteral("区域省份映射"));

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

    // 连接信号
    connect(m_saveBtn, &QPushButton::clicked, this, &PriceTableDialog::onSaveClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &PriceTableDialog::onCancelClicked);
    connect(m_addRowBtn, &QPushButton::clicked, this, &PriceTableDialog::onAddPriceRow);
    connect(m_removeRowBtn, &QPushButton::clicked, this, &PriceTableDialog::onRemovePriceRow);
    connect(m_importBtn, &QPushButton::clicked, this, &PriceTableDialog::onImportExcel);
    connect(m_exportBtn, &QPushButton::clicked, this, &PriceTableDialog::onExportExcel);

    connect(m_addRegionBtn, &QPushButton::clicked, [this]() {
        auto *item = new QTreeWidgetItem(m_regionTree, QStringList{QStringLiteral("新区")});
        item->setFlags(item->flags() | Qt::ItemIsEditable);
        m_regionTree->editItem(item, 0);
    });

    connect(m_addProvinceBtn, &QPushButton::clicked, [this]() {
        QTreeWidgetItem *current = m_regionTree->currentItem();
        if (!current) {
            QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择一个区域节点"));
            return;
        }
        // 如果选中的是省份节点，获取其父节点
        QTreeWidgetItem *parentItem = current->parent() ? current->parent() : current;
        auto *child = new QTreeWidgetItem(parentItem, QStringList{QStringLiteral("新省")});
        child->setFlags(child->flags() | Qt::ItemIsEditable);
        m_regionTree->editItem(child, 0);
        m_regionTree->expandItem(parentItem);
    });

    connect(m_removeTreeNodeBtn, &QPushButton::clicked, [this]() {
        QTreeWidgetItem *current = m_regionTree->currentItem();
        if (!current) return;
        QTreeWidgetItem *parent = current->parent();
        if (parent) {
            parent->removeChild(current);
        } else {
            int idx = m_regionTree->indexOfTopLevelItem(current);
            if (idx >= 0) delete m_regionTree->takeTopLevelItem(idx);
        }
    });
}

void PriceTableDialog::applyDarkStyle()
{
    setStyleSheet(QStringLiteral(R"(
        PriceTableDialog {
            background-color: #1a1a2e;
            color: #eaeaea;
        }
        QLabel {
            color: #eaeaea;
            background: transparent;
        }
        QTabWidget::pane {
            background-color: #1a1a2e;
            border: 1px solid #0f3460;
            border-radius: 8px;
        }
        QTabBar::tab {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-bottom: none;
            border-top-left-radius: 8px;
            border-top-right-radius: 8px;
            padding: 8px 20px;
            min-height: 36px;
        }
        QTabBar::tab:selected {
            background-color: #0f3460;
            color: #eaeaea;
        }
        QTabBar::tab:hover:!selected {
            background-color: #1a3a5c;
        }
        QTableWidget {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 8px;
            gridline-color: #0f3460;
            selection-background-color: #0f3460;
            alternate-background-color: #1a2744;
        }
        QTableWidget::item {
            padding: 4px 8px;
        }
        QHeaderView::section {
            background-color: #0f3460;
            color: #eaeaea;
            border: 1px solid #1a1a2e;
            padding: 6px 8px;
            font-weight: bold;
        }
        QTreeWidget {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 8px;
            alternate-background-color: #1a2744;
            outline: none;
        }
        QTreeWidget::item {
            padding: 6px 8px;
            border-radius: 4px;
        }
        QTreeWidget::item:selected {
            background-color: #0f3460;
        }
        QTreeWidget::item:hover {
            background-color: #1a3a5c;
        }
        QPushButton#primaryButton {
            background-color: #0f3460;
            color: #eaeaea;
            border: none;
            border-radius: 8px;
            padding: 8px 24px;
            min-height: 36px;
            font-weight: bold;
        }
        QPushButton#primaryButton:hover {
            background-color: #e94560;
        }
        QPushButton#primaryButton:pressed {
            background-color: #c73550;
        }
        QPushButton#secondaryButton {
            background-color: transparent;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 8px;
            padding: 8px 20px;
            min-height: 36px;
        }
        QPushButton#secondaryButton:hover {
            background-color: #0f3460;
        }
        QPushButton#smallButton {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 8px;
            padding: 4px 12px;
            min-height: 36px;
        }
        QPushButton#smallButton:hover {
            background-color: #0f3460;
        }
        QPushButton#dangerButton {
            background-color: transparent;
            color: #e94560;
            border: 1px solid #e94560;
            border-radius: 8px;
            padding: 8px 20px;
            min-height: 36px;
        }
        QPushButton#dangerButton:hover {
            background-color: #e94560;
            color: #eaeaea;
        }
        QComboBox {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 8px;
            padding: 4px 8px;
        }
        QComboBox QAbstractItemView {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            selection-background-color: #0f3460;
        }
        QDoubleSpinBox, QSpinBox {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 8px;
            padding: 4px 8px;
        }
        QCheckBox {
            color: #eaeaea;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
            border-radius: 3px;
            border: 2px solid #0f3460;
            background-color: #16213e;
        }
        QCheckBox::indicator:checked {
            background-color: #e94560;
            border-color: #e94560;
        }
    )"));
}

void PriceTableDialog::loadDefaultPriceTable()
{
    m_priceTable->setRowCount(0);
    QList<PriceRule> rules = m_ruleManager->defaultPriceTable();
    for (const PriceRule &rule : rules) {
        for (const WeightSegment &seg : rule.segments) {
            int row = m_priceTable->rowCount();
            m_priceTable->insertRow(row);

            m_priceTable->setItem(row, 0, new QTableWidgetItem(rule.region));
            m_priceTable->setItem(row, 1, new QTableWidgetItem(rule.province));
            m_priceTable->setItem(row, 2, new QTableWidgetItem(
                seg.maxWeight < 0 ? QString::number(seg.minWeight, 'f', 2) :
                QString::number(seg.minWeight, 'f', 2)));
            m_priceTable->setItem(row, 3, new QTableWidgetItem(
                seg.maxWeight < 0 ? QStringLiteral("无上限") :
                QString::number(seg.maxWeight, 'f', 2)));
            m_priceTable->setItem(row, 4, new QTableWidgetItem(
                QString::number(seg.firstWeightPrice, 'f', 2)));
            m_priceTable->setItem(row, 5, new QTableWidgetItem(
                QString::number(seg.additionalPrice, 'f', 2)));

            auto *hundredCheck = new QTableWidgetItem();
            hundredCheck->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            hundredCheck->setCheckState(seg.isHundredGram ? Qt::Checked : Qt::Unchecked);
            m_priceTable->setItem(row, 6, hundredCheck);

            auto *fullCheck = new QTableWidgetItem();
            fullCheck->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
            fullCheck->setCheckState(seg.isFullAdditional ? Qt::Checked : Qt::Unchecked);
            m_priceTable->setItem(row, 7, fullCheck);
        }
    }

    m_priceTable->resizeColumnsToContents();
}

void PriceTableDialog::loadRegionMappings()
{
    m_regionTree->clear();

    // 从默认报价表中提取区域和省份的映射关系
    QList<PriceRule> rules = m_ruleManager->defaultPriceTable();
    QMap<QString, QStringList> regionMap;

    for (const PriceRule &rule : rules) {
        if (!regionMap.contains(rule.region)) {
            regionMap[rule.region] = QStringList();
        }
        if (!regionMap[rule.region].contains(rule.province)) {
            regionMap[rule.region].append(rule.province);
        }
    }

    // 按区域顺序显示
    QStringList regionOrder = {QStringLiteral("一区"), QStringLiteral("二区"),
                               QStringLiteral("三区"), QStringLiteral("四区"), QStringLiteral("五区")};

    for (const QString &region : regionOrder) {
        if (!regionMap.contains(region)) continue;
        auto *regionItem = new QTreeWidgetItem(m_regionTree, QStringList{region});
        regionItem->setFlags(regionItem->flags() | Qt::ItemIsEditable);
        regionItem->setExpanded(true);

        for (const QString &prov : regionMap[region]) {
            auto *provItem = new QTreeWidgetItem(regionItem, QStringList{prov});
            provItem->setFlags(provItem->flags() | Qt::ItemIsEditable);
        }
    }

    // 也添加不在默认顺序中的区域
    for (const QString &region : regionMap.keys()) {
        if (regionOrder.contains(region)) continue;
        auto *regionItem = new QTreeWidgetItem(m_regionTree, QStringList{region});
        regionItem->setFlags(regionItem->flags() | Qt::ItemIsEditable);
        regionItem->setExpanded(true);

        for (const QString &prov : regionMap[region]) {
            auto *provItem = new QTreeWidgetItem(regionItem, QStringList{prov});
            provItem->setFlags(provItem->flags() | Qt::ItemIsEditable);
        }
    }

    m_regionTree->expandAll();
}

QMap<QString, QStringList> PriceTableDialog::extractRegionMappingsFromTable() const
{
    QMap<QString, QStringList> result;
    for (int i = 0; i < m_regionTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *regionItem = m_regionTree->topLevelItem(i);
        QString region = regionItem->text(0);
        QStringList provinces;
        for (int j = 0; j < regionItem->childCount(); ++j) {
            provinces.append(regionItem->child(j)->text(0));
        }
        result[region] = provinces;
    }
    return result;
}

void PriceTableDialog::collectPriceTableData()
{
    QList<PriceRule> rules;
    QMap<QString, PriceRule> ruleMap; // key = "region|province"

    for (int i = 0; i < m_priceTable->rowCount(); ++i) {
        QString region = m_priceTable->item(i, 0) ? m_priceTable->item(i, 0)->text() : QString();
        QString province = m_priceTable->item(i, 1) ? m_priceTable->item(i, 1)->text() : QString();

        if (region.isEmpty() || province.isEmpty()) continue;

        double minWeight = m_priceTable->item(i, 2) ? m_priceTable->item(i, 2)->text().toDouble() : 0;
        QString maxWeightStr = m_priceTable->item(i, 3) ? m_priceTable->item(i, 3)->text() : QStringLiteral("-1");
        double maxWeight = (maxWeightStr == QStringLiteral("无上限")) ? -1 : maxWeightStr.toDouble();
        double firstPrice = m_priceTable->item(i, 4) ? m_priceTable->item(i, 4)->text().toDouble() : 0;
        double addPrice = m_priceTable->item(i, 5) ? m_priceTable->item(i, 5)->text().toDouble() : 0;
        bool isHundredGram = m_priceTable->item(i, 6) ?
            (m_priceTable->item(i, 6)->checkState() == Qt::Checked) : false;
        bool isFullAdditional = m_priceTable->item(i, 7) ?
            (m_priceTable->item(i, 7)->checkState() == Qt::Checked) : false;

        QString key = region + QStringLiteral("|") + province;
        if (!ruleMap.contains(key)) {
            PriceRule rule;
            rule.region = region;
            rule.province = province;
            ruleMap[key] = rule;
        }

        WeightSegment seg;
        seg.minWeight = minWeight;
        seg.maxWeight = maxWeight;
        seg.firstWeightPrice = firstPrice;
        seg.additionalPrice = addPrice;
        seg.isHundredGram = isHundredGram;
        seg.isFullAdditional = isFullAdditional;
        ruleMap[key].segments.append(seg);
    }

    rules = ruleMap.values();
    m_ruleManager->setDefaultPriceTable(rules);
}

void PriceTableDialog::collectRegionMappingData()
{
    // 区域映射信息已从报价表中隐含，此处用于树形编辑后的数据同步
    // 暂不持久化区域映射到RuleManager（因RuleManager未暴露公共接口）
}

void PriceTableDialog::onAddPriceRow()
{
    int row = m_priceTable->rowCount();
    m_priceTable->insertRow(row);
    m_priceTable->setItem(row, 0, new QTableWidgetItem(QStringLiteral("一区")));
    m_priceTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("江苏")));
    m_priceTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("0")));
    m_priceTable->setItem(row, 3, new QTableWidgetItem(QStringLiteral("-1")));
    m_priceTable->setItem(row, 4, new QTableWidgetItem(QStringLiteral("0")));
    m_priceTable->setItem(row, 5, new QTableWidgetItem(QStringLiteral("0")));
    auto *hundredCheck = new QTableWidgetItem();
    hundredCheck->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    hundredCheck->setCheckState(Qt::Unchecked);
    m_priceTable->setItem(row, 6, hundredCheck);
    auto *fullCheck = new QTableWidgetItem();
    fullCheck->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
    fullCheck->setCheckState(Qt::Unchecked);
    m_priceTable->setItem(row, 7, fullCheck);
    m_priceTable->scrollToBottom();
}

void PriceTableDialog::onRemovePriceRow()
{
    int row = m_priceTable->currentRow();
    if (row >= 0) m_priceTable->removeRow(row);
}

void PriceTableDialog::onImportExcel()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        QStringLiteral("导入报价表"), QString(),
        QStringLiteral("Excel文件 (*.xlsx *.xls);;所有文件 (*.*)"));

    if (filePath.isEmpty()) return;

    // TODO: 调用ExcelImporter导入报价表数据
    QMessageBox::information(this, QStringLiteral("导入"),
        QStringLiteral("导入功能待集成Excel模块后实现\n文件: %1").arg(filePath));
}

void PriceTableDialog::onExportExcel()
{
    QString filePath = QFileDialog::getSaveFileName(this,
        QStringLiteral("导出报价表"), QString(),
        QStringLiteral("Excel文件 (*.xlsx);;所有文件 (*.*)"));

    if (filePath.isEmpty()) return;

    // TODO: 调用ExcelExporter导出报价表数据
    QMessageBox::information(this, QStringLiteral("导出"),
        QStringLiteral("导出功能待集成Excel模块后实现\n文件: %1").arg(filePath));
}

void PriceTableDialog::onSaveClicked()
{
    collectPriceTableData();
    collectRegionMappingData();
    QMessageBox::information(this, QStringLiteral("保存成功"), QStringLiteral("报价表已更新"));
    accept();
}

void PriceTableDialog::onCancelClicked()
{
    reject();
}
