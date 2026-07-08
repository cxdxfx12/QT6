#include "QuickTestWidget.h"
#include "Calculation/RuleManager.h"
#include "Calculation/FreightCalculator.h"
#include "DataModel/CalculationRule.h"
#include "DataModel/OrderData.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMessageBox>
#include <QDateTime>
#include <QGroupBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>

QuickTestWidget::QuickTestWidget(RuleManager *ruleManager, QWidget *parent)
    : QDialog(parent)
    , m_ruleManager(ruleManager)
    , m_calculator(new FreightCalculator(this))
{
    setupUI();
    populateProvinces();
    populateCustomerRules();

    m_calculator->setDefaultPriceTable(m_ruleManager->defaultPriceTable());
    m_calculator->setGlobalRules(m_ruleManager->globalRules());

    recalc();
}

void QuickTestWidget::setupUI()
{
    setWindowTitle(QStringLiteral("快速测试运费"));
    setMinimumWidth(480);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 16, 20, 16);
    mainLayout->setSpacing(12);

    // 标题
    auto *titleLabel = new QLabel(QStringLiteral("快速测试运费"), this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(14);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);

    // 输入区域
    auto *inputGroup = new QGroupBox(QStringLiteral("输入信息"), this);
    auto *formLayout = new QFormLayout(inputGroup);
    formLayout->setContentsMargins(16, 20, 16, 12);
    formLayout->setSpacing(10);
    formLayout->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);

    m_provinceCombo = new QComboBox(this);
    formLayout->addRow(QStringLiteral("目的省份："), m_provinceCombo);

    m_actualWeightSpin = new QDoubleSpinBox(this);
    m_actualWeightSpin->setRange(0, 9999);
    m_actualWeightSpin->setDecimals(2);
    m_actualWeightSpin->setSuffix(QStringLiteral(" kg"));
    m_actualWeightSpin->setSingleStep(0.1);
    m_actualWeightSpin->setValue(1.0);
    formLayout->addRow(QStringLiteral("实际重量："), m_actualWeightSpin);

    m_volWeightSpin = new QDoubleSpinBox(this);
    m_volWeightSpin->setRange(0, 9999);
    m_volWeightSpin->setDecimals(2);
    m_volWeightSpin->setSuffix(QStringLiteral(" kg"));
    m_volWeightSpin->setSingleStep(0.1);
    m_volWeightSpin->setValue(0.5);
    formLayout->addRow(QStringLiteral("体积重量："), m_volWeightSpin);

    m_modeCombo = new QComboBox(this);
    m_modeCombo->addItems(CalculationRule::allModes());
    formLayout->addRow(QStringLiteral("计算模式："), m_modeCombo);

    m_customerRuleCombo = new QComboBox(this);
    formLayout->addRow(QStringLiteral("客户规则："), m_customerRuleCombo);

    mainLayout->addWidget(inputGroup);

    // 结果显示区域
    auto *resultGroup = new QGroupBox(QStringLiteral("计算结果"), this);
    auto *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setContentsMargins(16, 20, 16, 12);
    resultLayout->setSpacing(8);

    m_resultLabel = new QLabel(QStringLiteral("运费: -- 元"), this);
    QFont resultFont = m_resultLabel->font();
    resultFont.setPointSize(18);
    resultFont.setBold(true);
    m_resultLabel->setFont(resultFont);
    m_resultLabel->setAlignment(Qt::AlignCenter);
    m_resultLabel->setStyleSheet(QStringLiteral("color: #ff7f20;"));
    m_resultLabel->setMinimumHeight(40);
    resultLayout->addWidget(m_resultLabel);

    m_detailLabel = new QLabel(QString(), this);
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setStyleSheet(QStringLiteral("color: #888888; font-size: 11px;"));
    m_detailLabel->setAlignment(Qt::AlignCenter);
    resultLayout->addWidget(m_detailLabel);

    mainLayout->addWidget(resultGroup);

    // 按钮区域
    auto *btnLayout = new QHBoxLayout();
    btnLayout->setSpacing(12);

    m_calcButton = new QPushButton(QStringLiteral("重新计算"), this);
    m_calcButton->setObjectName(QStringLiteral("primaryButton"));
    m_calcButton->setMinimumWidth(120);
    m_calcButton->setMinimumHeight(32);

    m_closeButton = new QPushButton(QStringLiteral("关闭"), this);
    m_closeButton->setObjectName(QStringLiteral("secondaryButton"));
    m_closeButton->setMinimumWidth(120);
    m_closeButton->setMinimumHeight(32);

    btnLayout->addStretch();
    btnLayout->addWidget(m_calcButton);
    btnLayout->addWidget(m_closeButton);
    btnLayout->addStretch();

    mainLayout->addLayout(btnLayout);

    // 连接信号
    connect(m_calcButton, &QPushButton::clicked, this, &QuickTestWidget::onCalculateClicked);
    connect(m_closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_customerRuleCombo, &QComboBox::currentTextChanged,
            this, &QuickTestWidget::onCustomerRuleChanged);

    // 实时计算
    connect(m_provinceCombo, &QComboBox::currentTextChanged, this, &QuickTestWidget::recalc);
    connect(m_actualWeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &QuickTestWidget::recalc);
    connect(m_volWeightSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &QuickTestWidget::recalc);
    connect(m_modeCombo, &QComboBox::currentTextChanged, this, &QuickTestWidget::recalc);
}

void QuickTestWidget::populateProvinces()
{
    m_provinceCombo->clear();
    QList<PriceRule> table = m_ruleManager->defaultPriceTable();
    for (const PriceRule &rule : table) {
        m_provinceCombo->addItem(rule.province);
    }
}

void QuickTestWidget::populateCustomerRules()
{
    m_customerRuleCombo->clear();
    m_customerRuleCombo->addItem(QStringLiteral("(默认报价表)"));
    m_customerRuleCombo->addItems(m_ruleManager->ruleNames());
}

void QuickTestWidget::onCalculateClicked()
{
    recalc();
}

void QuickTestWidget::recalc()
{
    QString province = m_provinceCombo->currentText();
    double actualWeight = m_actualWeightSpin->value();
    double volWeight = m_volWeightSpin->value();

    if (actualWeight <= 0 && volWeight <= 0) {
        m_resultLabel->setText(QStringLiteral("运费: 0.00 元"));
        m_detailLabel->setText(QStringLiteral("重量为 0"));
        return;
    }

    CustomerRule rule;
    QString customerName = m_customerRuleCombo->currentText();
    if (customerName == QStringLiteral("(默认报价表)")) {
        rule.calculationMode = m_modeCombo->currentText();
        rule.useDefaultPrice = true;
    } else {
        rule = m_ruleManager->getRule(customerName);
        rule.calculationMode = m_modeCombo->currentText();
    }

    OrderData order;
    order.actualWeight = actualWeight;
    order.volumetricWeight = volWeight;
    order.destinationProvince = province;
    order.businessTime = QDate::currentDate();
    order.isValid = true;

    QString ruleDesc;
    double freight = m_calculator->calculateFreight(order, rule);

    if (freight < 0) {
        m_resultLabel->setText(QStringLiteral("无法计算"));
        m_detailLabel->setText(QStringLiteral("未找到该省份的报价规则"));
    } else {
        double effectiveWeight = qMax(actualWeight, volWeight);
        m_resultLabel->setText(QStringLiteral("运费: %1 元").arg(freight, 0, 'f', 2));

        QString modeText = m_modeCombo->currentText();
        QString weightInfo = QStringLiteral("实际: %1 kg / 体积: %2 kg / 计费: %3 kg")
            .arg(actualWeight, 0, 'f', 2)
            .arg(volWeight, 0, 'f', 2)
            .arg(effectiveWeight, 0, 'f', 2);

        m_detailLabel->setText(QStringLiteral("%1\n计算模式: %2").arg(weightInfo).arg(modeText));
    }
}

void QuickTestWidget::onCustomerRuleChanged(const QString &name)
{
    if (name == QStringLiteral("(默认报价表)")) {
        m_modeCombo->setEnabled(true);
    } else {
        CustomerRule rule = m_ruleManager->getRule(name);
        if (!rule.customerName.isEmpty() && !rule.calculationMode.isEmpty()) {
            m_modeCombo->setCurrentText(rule.calculationMode);
        }
        m_modeCombo->setEnabled(true);
    }
    recalc();
}
