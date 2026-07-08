#ifndef CUSTOMERDIALOG_H
#define CUSTOMERDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include <QGroupBox>
#include <QScrollArea>
#include <QLabel>

#include "DataModel/PriceTable.h"

class RuleManager;

class CustomerDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomerDialog(RuleManager *ruleManager, QWidget *parent = nullptr);

private slots:
    void onAddCustomer();
    void onRemoveCustomer();
    void onCustomerSelected(QListWidgetItem *item);
    void onSaveClicked();
    void onCancelClicked();

private:
    void setupUI();
    void applyDarkStyle();
    void loadCustomerList();
    void loadCustomerRule(const QString &name);
    void clearEditArea();
    CustomerRule buildCustomerRule() const;
    void populateProvinceCombo(QComboBox *combo) const;

    RuleManager *m_ruleManager;
    QString m_currentCustomer;

    // 左侧
    QListWidget *m_customerList;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;

    // 右侧编辑区
    QScrollArea *m_scrollArea;
    QLineEdit *m_nameEdit;
    QComboBox *m_mappingCombo;
    QComboBox *m_calcModeCombo;
    QCheckBox *m_useDefaultCheck;
    QTableWidget *m_customPriceTable;
    QPushButton *m_saveBtn;
    QPushButton *m_cancelBtn;

    // 自定义报价表按钮
    QPushButton *m_addPriceRowBtn;
    QPushButton *m_removePriceRowBtn;
};

#endif // CUSTOMERDIALOG_H
