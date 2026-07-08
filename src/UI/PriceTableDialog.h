#ifndef PRICETABLEDIALOG_H
#define PRICETABLEDIALOG_H

#include <QDialog>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QPushButton>
#include <QHeaderView>
#include <QMap>

class RuleManager;

class PriceTableDialog : public QDialog {
    Q_OBJECT
public:
    explicit PriceTableDialog(RuleManager *ruleManager, QWidget *parent = nullptr);

private slots:
    void onSaveClicked();
    void onCancelClicked();
    void onAddPriceRow();
    void onRemovePriceRow();
    void onImportExcel();
    void onExportExcel();

private:
    void setupUI();
    void applyDarkStyle();
    void loadDefaultPriceTable();
    void loadRegionMappings();
    void collectPriceTableData();
    void collectRegionMappingData();
    QMap<QString, QStringList> extractRegionMappingsFromTable() const;

    RuleManager *m_ruleManager;

    QTabWidget *m_tabWidget;
    QTableWidget *m_priceTable;
    QTreeWidget *m_regionTree;
    QPushButton *m_addRowBtn;
    QPushButton *m_removeRowBtn;
    QPushButton *m_importBtn;
    QPushButton *m_exportBtn;
    QPushButton *m_saveBtn;
    QPushButton *m_cancelBtn;

    // 区域省份映射tab按钮
    QPushButton *m_addRegionBtn;
    QPushButton *m_addProvinceBtn;
    QPushButton *m_removeTreeNodeBtn;
};

#endif // PRICETABLEDIALOG_H
