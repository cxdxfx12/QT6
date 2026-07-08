#ifndef RULEMANAGERDIALOG_H
#define RULEMANAGERDIALOG_H

#include <QDialog>
#include <QList>
#include "Calculation/RuleManager.h"
#include "DataModel/PriceTable.h"

namespace Ui {
class RuleManagerDialog;
}

class RuleManagerDialog : public QDialog {
    Q_OBJECT

public:
    explicit RuleManagerDialog(RuleManager *ruleManager, QWidget *parent = nullptr);
    ~RuleManagerDialog();

private slots:
    void onCustomerSelected(const QModelIndex &index);
    void onAddCustomer();
    void onEditCustomer();
    void onDeleteCustomer();
    void onBatchImportCustomers();
    void onAddPriceRule();
    void onEditPriceRule();
    void onDeletePriceRule();
    void onApplyChanges();
    void onResetDefault();

private:
    Ui::RuleManagerDialog *ui;
    RuleManager *m_ruleManager;
    QList<CustomerRule> m_rules;
    CustomerRule m_currentRule;
    int m_currentRuleIndex;

    void loadCustomerList();
    void loadPriceTable();
    void saveCurrentPriceTable();
    void updateButtons();
};

#endif // RULEMANAGERDIALOG_H