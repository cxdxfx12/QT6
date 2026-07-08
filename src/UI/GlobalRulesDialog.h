#ifndef GLOBALRULESDIALOG_H
#define GLOBALRULESDIALOG_H

#include <QDialog>
#include <QDoubleSpinBox>
#include <QTableWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QGroupBox>
#include <QComboBox>

#include "DataModel/PriceTable.h"

class RuleManager;

class GlobalRulesDialog : public QDialog {
    Q_OBJECT
public:
    explicit GlobalRulesDialog(RuleManager *ruleManager, QWidget *parent = nullptr);

private slots:
    void onSaveClicked();
    void onCancelClicked();

private:
    void setupUI();
    void loadRules();
    GlobalRules buildGlobalRules() const;
    void populateProvinceCombo(QComboBox *combo) const;

    RuleManager *m_ruleManager;

    // 无重量默认价格
    QDoubleSpinBox *m_noWeightPriceSpin;

    // 拉均重参数
    QDoubleSpinBox *m_avgBaseSpin;
    QDoubleSpinBox *m_avgLimitSpin;
    QDoubleSpinBox *m_avgStepSpin;
    QDoubleSpinBox *m_avgSurchargeSpin;

    // Tab页
    QTabWidget *m_tabWidget;

    // 活动规则表
    QTableWidget *m_activityTable;
    QPushButton *m_addActivityBtn;
    QPushButton *m_removeActivityBtn;

    // 临时加价规则表
    QTableWidget *m_tempIncreaseTable;
    QPushButton *m_addTempBtn;
    QPushButton *m_removeTempBtn;

    // 省份加价规则表
    QTableWidget *m_provinceIncreaseTable;
    QPushButton *m_addProvinceBtn;
    QPushButton *m_removeProvinceBtn;

    // 保存/取消
    QPushButton *m_saveBtn;
    QPushButton *m_cancelBtn;
};

#endif // GLOBALRULESDIALOG_H
