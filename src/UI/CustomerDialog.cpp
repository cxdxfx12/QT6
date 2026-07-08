#include "CustomerDialog.h"
#include "Calculation/RuleManager.h"
#include "DataModel/CalculationRule.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>

CustomerDialog::CustomerDialog(RuleManager *ruleManager, QWidget *parent)
    : QDialog(parent)
    , m_ruleManager(ruleManager)
{
    setupUI();
    applyDarkStyle();
    loadCustomerList();
}

void CustomerDialog::setupUI()
{
    setWindowTitle(QStringLiteral("客户管理"));
    setMinimumSize(900, 600);

    auto *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    // ========== 左侧：客户列表 ==========
    auto *leftWidget = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftWidget);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    auto *leftTitle = new QLabel(QStringLiteral("客户列表"), leftWidget);
    leftTitle->setObjectName(QStringLiteral("sectionTitle"));

    m_customerList = new QListWidget(leftWidget);
    m_customerList->setSelectionMode(QAbstractItemView::SingleSelection);

    auto *listBtnLayout = new QHBoxLayout();
    m_addBtn = new QPushButton(QStringLiteral("添加"), leftWidget);
    m_addBtn->setObjectName(QStringLiteral("primaryButton"));
    m_addBtn->setMinimumHeight(36);
    m_removeBtn = new QPushButton(QStringLiteral("删除"), leftWidget);
    m_removeBtn->setObjectName(QStringLiteral("dangerButton"));
    m_removeBtn->setMinimumHeight(36);
    listBtnLayout->addWidget(m_addBtn);
    listBtnLayout->addWidget(m_removeBtn);

    leftLayout->addWidget(leftTitle);
    leftLayout->addWidget(m_customerList, 1);
    leftLayout->addLayout(listBtnLayout);

    leftWidget->setFixedWidth(200);

    // ========== 右侧：编辑区域 ==========
    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *editContainer = new QWidget(m_scrollArea);
    auto *editLayout = new QVBoxLayout(editContainer);
    editLayout->setContentsMargins(8, 8, 8, 8);
    editLayout->setSpacing(12);

    // 基本信息组
    auto *basicGroup = new QGroupBox(QStringLiteral("基本信息"), editContainer);
    auto *basicForm = new QFormLayout(basicGroup);
    basicForm->setSpacing(10);

    m_nameEdit = new QLineEdit(basicGroup);
    m_nameEdit->setMinimumHeight(36);
    m_nameEdit->setPlaceholderText(QStringLiteral("输入客户名称"));
    basicForm->addRow(QStringLiteral("客户名称:"), m_nameEdit);

    m_mappingCombo = new QComboBox(basicGroup);
    m_mappingCombo->setEditable(true);
    m_mappingCombo->setMinimumHeight(36);
    m_mappingCombo->addItems(QStringList{QStringLiteral("客户"), QStringLiteral("客户名称"),
                                          QStringLiteral("客户简称"), QStringLiteral("结算客户")});
    basicForm->addRow(QStringLiteral("表头映射:"), m_mappingCombo);

    m_calcModeCombo = new QComboBox(basicGroup);
    m_calcModeCombo->addItems(CalculationRule::allModes());
    m_calcModeCombo->setMinimumHeight(36);
    basicForm->addRow(QStringLiteral("计算模式:"), m_calcModeCombo);

    m_useDefaultCheck = new QCheckBox(QStringLiteral("使用默认报价表"), basicGroup);
    basicForm->addRow(m_useDefaultCheck);

    editLayout->addWidget(basicGroup);

    // 自定义报价规则组
    auto *priceGroup = new QGroupBox(QStringLiteral("自定义报价规则"), editContainer);
    auto *priceLayout = new QVBoxLayout(priceGroup);
    priceLayout->setSpacing(8);

    m_customPriceTable = new QTableWidget(0, 4, priceGroup);
    m_customPriceTable->setHorizontalHeaderLabels(QStringList()
        << QStringLiteral("省份") << QStringLiteral("首重")
        << QStringLiteral("续重") << QStringLiteral("模式"));
    m_customPriceTable->horizontalHeader()->setStretchLastSection(true);
    m_customPriceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_customPriceTable->setMinimumHeight(150);

    auto *priceBtnLayout = new QHBoxLayout();
    m_addPriceRowBtn = new QPushButton(QStringLiteral("添加行"), priceGroup);
    m_addPriceRowBtn->setObjectName(QStringLiteral("smallButton"));
    m_addPriceRowBtn->setMinimumHeight(36);
    m_removePriceRowBtn = new QPushButton(QStringLiteral("删除行"), priceGroup);
    m_removePriceRowBtn->setObjectName(QStringLiteral("smallButton"));
    m_removePriceRowBtn->setMinimumHeight(36);
    priceBtnLayout->addWidget(m_addPriceRowBtn);
    priceBtnLayout->addWidget(m_removePriceRowBtn);
    priceBtnLayout->addStretch();

    priceLayout->addWidget(m_customPriceTable);
    priceLayout->addLayout(priceBtnLayout);
    editLayout->addWidget(priceGroup);

    editLayout->addStretch();

    // 保存/取消按钮
    auto *bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(12);

    m_saveBtn = new QPushButton(QStringLiteral("保存"), editContainer);
    m_saveBtn->setObjectName(QStringLiteral("primaryButton"));
    m_saveBtn->setMinimumHeight(36);
    m_saveBtn->setFixedWidth(120);

    m_cancelBtn = new QPushButton(QStringLiteral("取消"), editContainer);
    m_cancelBtn->setObjectName(QStringLiteral("secondaryButton"));
    m_cancelBtn->setMinimumHeight(36);
    m_cancelBtn->setFixedWidth(120);

    bottomLayout->addStretch();
    bottomLayout->addWidget(m_saveBtn);
    bottomLayout->addWidget(m_cancelBtn);

    editLayout->addLayout(bottomLayout);

    m_scrollArea->setWidget(editContainer);

    // 组合左右布局
    mainLayout->addWidget(leftWidget);
    mainLayout->addWidget(m_scrollArea, 1);

    // 连接信号
    connect(m_addBtn, &QPushButton::clicked, this, &CustomerDialog::onAddCustomer);
    connect(m_removeBtn, &QPushButton::clicked, this, &CustomerDialog::onRemoveCustomer);
    connect(m_customerList, &QListWidget::itemClicked, this, &CustomerDialog::onCustomerSelected);
    connect(m_saveBtn, &QPushButton::clicked, this, &CustomerDialog::onSaveClicked);
    connect(m_cancelBtn, &QPushButton::clicked, this, &CustomerDialog::onCancelClicked);

    // 自定义报价表行操作
    connect(m_addPriceRowBtn, &QPushButton::clicked, [this]() {
        int row = m_customPriceTable->rowCount();
        m_customPriceTable->insertRow(row);
        auto *provCombo = new QComboBox(m_customPriceTable);
        populateProvinceCombo(provCombo);
        m_customPriceTable->setCellWidget(row, 0, provCombo);
        m_customPriceTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("0")));
        m_customPriceTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("0")));
        auto *modeCombo = new QComboBox(m_customPriceTable);
        modeCombo->addItems(CalculationRule::allModes());
        m_customPriceTable->setCellWidget(row, 3, modeCombo);
    });
    connect(m_removePriceRowBtn, &QPushButton::clicked, [this]() {
        int row = m_customPriceTable->currentRow();
        if (row >= 0) m_customPriceTable->removeRow(row);
    });
}

void CustomerDialog::applyDarkStyle()
{
    setStyleSheet(QStringLiteral(R"(
        CustomerDialog {
            background-color: #1a1a2e;
            color: #eaeaea;
        }
        QLabel {
            color: #eaeaea;
            background: transparent;
        }
        QLabel#sectionTitle {
            font-size: 14px;
            font-weight: bold;
            padding: 4px;
            color: #eaeaea;
        }
        QListWidget {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 8px;
            padding: 4px;
            outline: none;
        }
        QListWidget::item {
            padding: 8px 12px;
            border-radius: 4px;
        }
        QListWidget::item:selected {
            background-color: #0f3460;
            color: #eaeaea;
        }
        QListWidget::item:hover {
            background-color: #1a3a5c;
        }
        QGroupBox {
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 8px;
            margin-top: 12px;
            padding-top: 16px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 6px;
        }
        QScrollArea {
            background-color: #1a1a2e;
            border: none;
        }
        QLineEdit, QComboBox {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 8px;
            padding: 6px 12px;
            min-height: 36px;
        }
        QComboBox::drop-down {
            border: none;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            selection-background-color: #0f3460;
        }
        QCheckBox {
            color: #eaeaea;
            spacing: 8px;
        }
        QCheckBox::indicator {
            width: 18px;
            height: 18px;
            border-radius: 4px;
            border: 2px solid #0f3460;
            background-color: #16213e;
        }
        QCheckBox::indicator:checked {
            background-color: #e94560;
            border-color: #e94560;
        }
        QTableWidget {
            background-color: #16213e;
            color: #eaeaea;
            border: 1px solid #0f3460;
            border-radius: 8px;
            gridline-color: #0f3460;
            selection-background-color: #0f3460;
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
        QPushButton#primaryButton {
            background-color: #0f3460;
            color: #eaeaea;
            border: none;
            border-radius: 8px;
            padding: 8px 20px;
            min-height: 36px;
            font-weight: bold;
        }
        QPushButton#primaryButton:hover {
            background-color: #e94560;
        }
        QPushButton#primaryButton:pressed {
            background-color: #c73550;
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
    )"));
}

void CustomerDialog::loadCustomerList()
{
    m_customerList->clear();
    QStringList names = m_ruleManager->ruleNames();
    for (const QString &name : names) {
        m_customerList->addItem(name);
    }
    clearEditArea();
}

void CustomerDialog::loadCustomerRule(const QString &name)
{
    CustomerRule rule = m_ruleManager->getRule(name);
    if (rule.customerName.isEmpty()) {
        clearEditArea();
        return;
    }

    m_currentCustomer = name;
    m_nameEdit->setText(rule.customerName);
    m_mappingCombo->setCurrentText(rule.mappingHeader);
    m_calcModeCombo->setCurrentText(rule.calculationMode);
    m_useDefaultCheck->setChecked(rule.useDefaultPrice);

    // 自定义报价规则
    m_customPriceTable->setRowCount(0);
    for (const PriceRule &pr : rule.customPriceRules) {
        int row = m_customPriceTable->rowCount();
        m_customPriceTable->insertRow(row);
        auto *provCombo = new QComboBox(m_customPriceTable);
        populateProvinceCombo(provCombo);
        provCombo->setCurrentText(pr.province);
        m_customPriceTable->setCellWidget(row, 0, provCombo);
        if (!pr.segments.isEmpty()) {
            m_customPriceTable->setItem(row, 1, new QTableWidgetItem(
                QString::number(pr.segments.first().firstWeightPrice, 'f', 2)));
            m_customPriceTable->setItem(row, 2, new QTableWidgetItem(
                QString::number(pr.segments.first().additionalPrice, 'f', 2)));
        } else {
            m_customPriceTable->setItem(row, 1, new QTableWidgetItem(QStringLiteral("0")));
            m_customPriceTable->setItem(row, 2, new QTableWidgetItem(QStringLiteral("0")));
        }
        auto *modeCombo = new QComboBox(m_customPriceTable);
        modeCombo->addItems(CalculationRule::allModes());
        m_customPriceTable->setCellWidget(row, 3, modeCombo);
    }
}

void CustomerDialog::clearEditArea()
{
    m_currentCustomer.clear();
    m_nameEdit->clear();
    m_mappingCombo->setCurrentIndex(0);
    m_calcModeCombo->setCurrentIndex(0);
    m_useDefaultCheck->setChecked(true);
    m_customPriceTable->setRowCount(0);
}

CustomerRule CustomerDialog::buildCustomerRule() const
{
    CustomerRule rule;
    rule.customerName = m_nameEdit->text().trimmed();
    rule.mappingHeader = m_mappingCombo->currentText();
    rule.calculationMode = m_calcModeCombo->currentText();
    rule.useDefaultPrice = m_useDefaultCheck->isChecked();

    // 自定义报价规则
    for (int i = 0; i < m_customPriceTable->rowCount(); ++i) {
        PriceRule pr;
        auto *provCombo = qobject_cast<QComboBox*>(m_customPriceTable->cellWidget(i, 0));
        pr.province = provCombo ? provCombo->currentText() : m_customPriceTable->item(i, 0)->text();
        WeightSegment seg;
        seg.firstWeightPrice = m_customPriceTable->item(i, 1) ?
            m_customPriceTable->item(i, 1)->text().toDouble() : 0;
        seg.additionalPrice = m_customPriceTable->item(i, 2) ?
            m_customPriceTable->item(i, 2)->text().toDouble() : 0;
        pr.segments.append(seg);
        rule.customPriceRules.append(pr);
    }

    return rule;
}

void CustomerDialog::populateProvinceCombo(QComboBox *combo) const
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

void CustomerDialog::onAddCustomer()
{
    clearEditArea();
    m_nameEdit->setFocus();
    m_customerList->clearSelection();
}

void CustomerDialog::onRemoveCustomer()
{
    QListWidgetItem *item = m_customerList->currentItem();
    if (!item) return;

    QString name = item->text();
    auto ret = QMessageBox::question(this, QStringLiteral("确认删除"),
        QStringLiteral("确定要删除客户 \"%1\" 吗？").arg(name),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (ret == QMessageBox::Yes) {
        m_ruleManager->removeCustomerRule(name);
        loadCustomerList();
        clearEditArea();
    }
}

void CustomerDialog::onCustomerSelected(QListWidgetItem *item)
{
    if (!item) return;
    loadCustomerRule(item->text());
}

void CustomerDialog::onSaveClicked()
{
    CustomerRule rule = buildCustomerRule();
    if (rule.customerName.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("客户名称不能为空"));
        return;
    }

    if (m_currentCustomer.isEmpty() || m_currentCustomer != rule.customerName) {
        // 新增或重命名
        m_ruleManager->addCustomerRule(rule);
    } else {
        m_ruleManager->updateCustomerRule(m_currentCustomer, rule);
    }

    m_currentCustomer = rule.customerName;
    loadCustomerList();

    // 重新选中当前客户
    for (int i = 0; i < m_customerList->count(); ++i) {
        if (m_customerList->item(i)->text() == rule.customerName) {
            m_customerList->setCurrentRow(i);
            break;
        }
    }
}

void CustomerDialog::onCancelClicked()
{
    reject();
}
