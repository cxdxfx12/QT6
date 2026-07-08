#include "RuleManagerDialog.h"
#include "ui_RuleManagerDialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QStandardItemModel>
#include <QHeaderView>
#include "DataModel/CalculationRule.h"
#include "xlsxdocument.h"

RuleManagerDialog::RuleManagerDialog(RuleManager *ruleManager, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::RuleManagerDialog)
    , m_ruleManager(ruleManager)
    , m_currentRuleIndex(-1)
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("规则管理"));

    connect(ui->customerList, &QListView::clicked, this, &RuleManagerDialog::onCustomerSelected);
    connect(ui->addCustomerBtn, &QPushButton::clicked, this, &RuleManagerDialog::onAddCustomer);
    connect(ui->editCustomerBtn, &QPushButton::clicked, this, &RuleManagerDialog::onEditCustomer);
    connect(ui->deleteCustomerBtn, &QPushButton::clicked, this, &RuleManagerDialog::onDeleteCustomer);
    connect(ui->batchImportBtn, &QPushButton::clicked, this, &RuleManagerDialog::onBatchImportCustomers);
    connect(ui->addPriceRuleBtn, &QPushButton::clicked, this, &RuleManagerDialog::onAddPriceRule);
    connect(ui->editPriceRuleBtn, &QPushButton::clicked, this, &RuleManagerDialog::onEditPriceRule);
    connect(ui->deletePriceRuleBtn, &QPushButton::clicked, this, &RuleManagerDialog::onDeletePriceRule);
    connect(ui->priceTableView, &QTableView::doubleClicked, this, &RuleManagerDialog::onEditPriceRule);
    connect(ui->applyBtn, &QPushButton::clicked, this, &RuleManagerDialog::onApplyChanges);
    connect(ui->resetDefaultBtn, &QPushButton::clicked, this, &RuleManagerDialog::onResetDefault);

    loadCustomerList();
    updateButtons();
}

RuleManagerDialog::~RuleManagerDialog()
{
    delete ui;
}

void RuleManagerDialog::loadCustomerList()
{
    m_rules.clear();
    QMap<QString, CustomerRule> ruleMap = m_ruleManager->rulesMap();

    for (auto it = ruleMap.begin(); it != ruleMap.end(); ++it) {
        m_rules.append(it.value());
    }

    auto *model = new QStandardItemModel(this);
    for (const CustomerRule &rule : m_rules) {
        auto *item = new QStandardItem(rule.customerName);
        item->setEditable(false);
        model->appendRow(item);
    }

    ui->customerList->setModel(model);
}

void RuleManagerDialog::loadPriceTable()
{
    auto *model = new QStandardItemModel(this);

    QStringList headers;
    headers << QStringLiteral("区域") << QStringLiteral("目的省份")
            << QStringLiteral("0-0.5KG") << QStringLiteral("0.51-1KG")
            << QStringLiteral("1-2KG") << QStringLiteral("2-3KG")
            << QStringLiteral("3-30KG首重") << QStringLiteral("3-30KG续重")
            << QStringLiteral("30KG以上首重") << QStringLiteral("30KG以上续重");

    model->setHorizontalHeaderLabels(headers);

    QString currentRegion;
    for (const PriceRule &pr : m_currentRule.customPriceRules) {
        QList<QStandardItem*> row;

        if (pr.region != currentRegion) {
            currentRegion = pr.region;
            row << new QStandardItem(pr.region);
        } else {
            row << new QStandardItem(QString());
        }

        row << new QStandardItem(pr.province);

        double seg05 = 0, seg1 = 0, seg2 = 0, seg3 = 0;
        double seg30first = 0, seg30add = 0, seg30upFirst = 0, seg30upAdd = 0;

        for (const WeightSegment &seg : pr.segments) {
            if (seg.minWeight <= 0 && seg.maxWeight >= 0.5) {
                seg05 = seg.firstWeightPrice;
            } else if (seg.minWeight <= 0.51 && seg.maxWeight >= 1) {
                seg1 = seg.firstWeightPrice;
            } else if (seg.minWeight <= 1 && seg.maxWeight >= 2) {
                seg2 = seg.firstWeightPrice;
            } else if (seg.minWeight <= 2 && seg.maxWeight >= 3) {
                seg3 = seg.firstWeightPrice;
            } else if (seg.minWeight <= 3 && seg.maxWeight >= 30) {
                seg30first = seg.firstWeightPrice;
                seg30add = seg.additionalPrice;
            } else if (seg.minWeight <= 30 && seg.maxWeight < 0) {
                seg30upFirst = seg.firstWeightPrice;
                seg30upAdd = seg.additionalPrice;
            }
        }

        row << new QStandardItem(QString::number(seg05, 'f', 2));
        row << new QStandardItem(QString::number(seg1, 'f', 2));
        row << new QStandardItem(QString::number(seg2, 'f', 2));
        row << new QStandardItem(QString::number(seg3, 'f', 2));
        row << new QStandardItem(QString::number(seg30first, 'f', 2));
        row << new QStandardItem(QString::number(seg30add, 'f', 2));
        row << new QStandardItem(QString::number(seg30upFirst, 'f', 2));
        row << new QStandardItem(QString::number(seg30upAdd, 'f', 2));

        for (int i = 0; i < row.size(); ++i) {
            auto *item = row[i];
            item->setEditable(i >= 2);
            item->setTextAlignment(Qt::AlignCenter);
        }
        row[0]->setTextAlignment(Qt::AlignCenter);
        row[1]->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        model->appendRow(row);
    }

    ui->priceTableView->setModel(model);
    ui->priceTableView->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    ui->priceTableView->horizontalHeader()->setStretchLastSection(false);
    ui->priceTableView->verticalHeader()->setDefaultSectionSize(24);
}

void RuleManagerDialog::saveCurrentPriceTable()
{
    if (m_currentRuleIndex < 0)
        return;

    // 保存计算模式
    m_currentRule.calculationMode = ui->modeCombo->currentText();

    // 从表格模型读回用户直接编辑的单元格值
    auto *model = qobject_cast<QStandardItemModel*>(ui->priceTableView->model());
    if (model && model->rowCount() == m_currentRule.customPriceRules.size()) {
        // 列映射: col -> (segIndex, isFirstWeight)
        static const int colMap[][3] = {
            {2, 0, 1}, {3, 1, 1}, {4, 2, 1}, {5, 3, 1},
            {6, 4, 1}, {7, 4, 0}, {8, 5, 1}, {9, 5, 0}
        };

        for (int row = 0; row < model->rowCount(); ++row) {
            PriceRule &pr = m_currentRule.customPriceRules[row];
            for (const auto &entry : colMap) {
                int col = entry[0];
                int segIdx = entry[1];
                bool isFirstWeight = (entry[2] == 1);

                if (segIdx >= 0 && segIdx < pr.segments.size()) {
                    QStandardItem *item = model->item(row, col);
                    if (item) {
                        bool ok = false;
                        double val = item->text().toDouble(&ok);
                        if (ok) {
                            if (isFirstWeight)
                                pr.segments[segIdx].firstWeightPrice = val;
                            else
                                pr.segments[segIdx].additionalPrice = val;
                        }
                    }
                }
            }
        }
    }

    m_rules[m_currentRuleIndex] = m_currentRule;
}

void RuleManagerDialog::updateButtons()
{
    bool hasSelection = m_currentRuleIndex >= 0;
    ui->editCustomerBtn->setEnabled(hasSelection);
    ui->deleteCustomerBtn->setEnabled(hasSelection);
    ui->addPriceRuleBtn->setEnabled(hasSelection);
    ui->editPriceRuleBtn->setEnabled(hasSelection);
    ui->deletePriceRuleBtn->setEnabled(hasSelection);
}

void RuleManagerDialog::onCustomerSelected(const QModelIndex &index)
{
    saveCurrentPriceTable();

    m_currentRuleIndex = index.row();
    m_currentRule = m_rules[m_currentRuleIndex];

    ui->customerNameEdit->setText(m_currentRule.customerName);
    ui->modeCombo->setCurrentText(m_currentRule.calculationMode);

    loadPriceTable();
    updateButtons();
}

void RuleManagerDialog::onAddCustomer()
{
    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("添加客户"),
                                         QStringLiteral("客户名称:"), QLineEdit::Normal,
                                         QString(), &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    CustomerRule newRule;
    newRule.customerName = name.trimmed();
    newRule.calculationMode = QStringLiteral("实际重量");
    newRule.useDefaultPrice = true;
    newRule.customPriceRules = m_ruleManager->defaultPriceTable();

    m_rules.append(newRule);
    m_ruleManager->addCustomerRule(newRule);

    loadCustomerList();
}

void RuleManagerDialog::onEditCustomer()
{
    if (m_currentRuleIndex < 0)
        return;

    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("编辑客户"),
                                         QStringLiteral("客户名称:"), QLineEdit::Normal,
                                         m_currentRule.customerName, &ok);
    if (!ok || name.trimmed().isEmpty())
        return;

    QString oldName = m_currentRule.customerName;
    m_currentRule.customerName = name.trimmed();
    m_currentRule.calculationMode = ui->modeCombo->currentText();
    m_rules[m_currentRuleIndex] = m_currentRule;

    if (oldName != name) {
        m_ruleManager->removeCustomerRule(oldName);
    }
    m_ruleManager->addCustomerRule(m_currentRule);

    loadCustomerList();
    ui->customerList->setCurrentIndex(ui->customerList->model()->index(m_currentRuleIndex, 0));
}

void RuleManagerDialog::onDeleteCustomer()
{
    if (m_currentRuleIndex < 0)
        return;

    QString name = m_rules[m_currentRuleIndex].customerName;
    int ret = QMessageBox::question(this, QStringLiteral("删除客户"),
                                    QStringLiteral("确定要删除客户 \"%1\" 吗？").arg(name),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    m_ruleManager->removeCustomerRule(name);
    m_rules.removeAt(m_currentRuleIndex);
    m_currentRuleIndex = -1;
    m_currentRule = CustomerRule();

    ui->customerNameEdit->clear();
    loadCustomerList();
    loadPriceTable();
    updateButtons();
}

void RuleManagerDialog::onBatchImportCustomers()
{
    QString filePath = QFileDialog::getOpenFileName(this,
        QStringLiteral("选择客户列表文件"), QString(),
        QStringLiteral("Excel文件 (*.xlsx *.xls)"));

    if (filePath.isEmpty())
        return;

    QXlsx::Document xlsx(filePath);
    if (!xlsx.load()) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("无法打开文件"));
        return;
    }

    int maxRow = xlsx.dimension().rowCount();
    if (maxRow <= 1) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("文件为空"));
        return;
    }

    int importedCount = 0;
    for (int r = 2; r <= maxRow; ++r) {
        QVariant cell = xlsx.read(r, 1);
        QString name = cell.toString().trimmed();
        if (name.isEmpty())
            continue;

        if (!m_ruleManager->getRule(name).customerName.isEmpty()) {
            continue;
        }

        CustomerRule newRule;
        newRule.customerName = name;
        newRule.calculationMode = QStringLiteral("实际重量");
        newRule.useDefaultPrice = true;
        newRule.customPriceRules = m_ruleManager->defaultPriceTable();

        m_rules.append(newRule);
        m_ruleManager->addCustomerRule(newRule);
        importedCount++;
    }

    loadCustomerList();
    QMessageBox::information(this, QStringLiteral("导入完成"),
                             QStringLiteral("成功导入 %1 个客户").arg(importedCount));
}

void RuleManagerDialog::onAddPriceRule()
{
    bool ok = false;
    QString province = QInputDialog::getText(this, QStringLiteral("添加省份"),
                                             QStringLiteral("省份名称:"), QLineEdit::Normal,
                                             QString(), &ok);
    if (!ok || province.trimmed().isEmpty())
        return;

    bool ok2 = false;
    QString region = QInputDialog::getText(this, QStringLiteral("添加省份"),
                                           QStringLiteral("所属区域:"), QLineEdit::Normal,
                                           QStringLiteral("一区"), &ok2);
    if (!ok2)
        return;

    PriceRule newRule;
    newRule.province = province.trimmed();
    newRule.region = region.trimmed();

    newRule.segments.append({0, 0.5, 0, 0, false, false});
    newRule.segments.append({0.51, 1, 0, 0, false, false});
    newRule.segments.append({1, 2, 0, 0, false, false});
    newRule.segments.append({2, 3, 0, 0, false, false});
    newRule.segments.append({3, 30, 0, 0, false, false});
    newRule.segments.append({30, -1, 0, 0, false, false});

    m_currentRule.customPriceRules.append(newRule);
    loadPriceTable();
}

void RuleManagerDialog::onEditPriceRule()
{
    QModelIndex idx = ui->priceTableView->currentIndex();
    if (!idx.isValid()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择要编辑的单元格"));
        return;
    }

    int row = idx.row();
    int col = idx.column();

    if (row < 0 || row >= m_currentRule.customPriceRules.size())
        return;

    if (col < 2) {
        QMessageBox::information(this, QStringLiteral("提示"),
                                 QStringLiteral("请双击价格列进行编辑"));
        return;
    }

    PriceRule &priceRule = m_currentRule.customPriceRules[row];

    QString colName;
    int segIndex = -1;
    bool isFirstWeight = true;

    switch (col) {
    case 2: colName = QStringLiteral("0-0.5KG"); segIndex = 0; isFirstWeight = true; break;
    case 3: colName = QStringLiteral("0.51-1KG"); segIndex = 1; isFirstWeight = true; break;
    case 4: colName = QStringLiteral("1-2KG"); segIndex = 2; isFirstWeight = true; break;
    case 5: colName = QStringLiteral("2-3KG"); segIndex = 3; isFirstWeight = true; break;
    case 6: colName = QStringLiteral("3-30KG首重"); segIndex = 4; isFirstWeight = true; break;
    case 7: colName = QStringLiteral("3-30KG续重"); segIndex = 4; isFirstWeight = false; break;
    case 8: colName = QStringLiteral("30KG以上首重"); segIndex = 5; isFirstWeight = true; break;
    case 9: colName = QStringLiteral("30KG以上续重"); segIndex = 5; isFirstWeight = false; break;
    }

    if (segIndex < 0 || segIndex >= priceRule.segments.size())
        return;

    WeightSegment &seg = priceRule.segments[segIndex];
    double currentValue = isFirstWeight ? seg.firstWeightPrice : seg.additionalPrice;

    bool ok = false;
    double value = QInputDialog::getDouble(this, QStringLiteral("编辑价格"),
                                           QStringLiteral("%1 - %2 (元):").arg(priceRule.province).arg(colName),
                                           currentValue, 0, 999999, 2, &ok);
    if (!ok)
        return;

    if (isFirstWeight) {
        seg.firstWeightPrice = value;
    } else {
        seg.additionalPrice = value;
    }

    loadPriceTable();

    QModelIndex newIndex = ui->priceTableView->model()->index(row, col);
    ui->priceTableView->setCurrentIndex(newIndex);
}

void RuleManagerDialog::onDeletePriceRule()
{
    QModelIndexList selected = ui->priceTableView->selectionModel()->selectedRows();
    if (selected.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请选择一行省份进行删除"));
        return;
    }

    int row = selected.first().row();
    if (row < 0 || row >= m_currentRule.customPriceRules.size())
        return;

    QString province = m_currentRule.customPriceRules[row].province;
    int ret = QMessageBox::question(this, QStringLiteral("删除省份"),
                                    QStringLiteral("确定要删除省份 \"%1\" 的报价吗？").arg(province),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    m_currentRule.customPriceRules.removeAt(row);
    loadPriceTable();
}

void RuleManagerDialog::onApplyChanges()
{
    saveCurrentPriceTable();

    for (CustomerRule &rule : m_rules) {
        m_ruleManager->addCustomerRule(rule);
    }

    m_ruleManager->saveToFile(RuleManager::defaultFilePath());

    QMessageBox::information(this, QStringLiteral("保存成功"), QStringLiteral("规则已保存"));
    close();
}

void RuleManagerDialog::onResetDefault()
{
    int ret = QMessageBox::question(this, QStringLiteral("恢复默认"),
                                    QStringLiteral("确定要恢复默认报价表吗？所有客户的报价表都将被重置。"),
                                    QMessageBox::Yes | QMessageBox::No);
    if (ret != QMessageBox::Yes)
        return;

    m_ruleManager->initDefaultPriceTable();
    loadCustomerList();
    QMessageBox::information(this, QStringLiteral("重置成功"), QStringLiteral("已恢复默认报价表"));
}