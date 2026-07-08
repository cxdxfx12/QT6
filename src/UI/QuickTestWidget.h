#ifndef QUICKTESTWIDGET_H
#define QUICKTESTWIDGET_H

#include <QDialog>

class RuleManager;
class FreightCalculator;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QPushButton;

class QuickTestWidget : public QDialog {
    Q_OBJECT
public:
    explicit QuickTestWidget(RuleManager *ruleManager, QWidget *parent = nullptr);

private slots:
    void onCalculateClicked();
    void onCustomerRuleChanged(const QString &name);
    void recalc();

private:
    RuleManager *m_ruleManager;
    FreightCalculator *m_calculator;

    QComboBox *m_provinceCombo;
    QDoubleSpinBox *m_actualWeightSpin;
    QDoubleSpinBox *m_volWeightSpin;
    QComboBox *m_modeCombo;
    QComboBox *m_customerRuleCombo;
    QLabel *m_resultLabel;
    QLabel *m_detailLabel;
    QPushButton *m_calcButton;
    QPushButton *m_closeButton;

    void setupUI();
    void populateProvinces();
    void populateCustomerRules();
};

#endif // QUICKTESTWIDGET_H
