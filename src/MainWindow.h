#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QFuture>
#include <QFutureWatcher>
#include <QMap>
#include <QAtomicInt>
#include <QPointer>
#include <QElapsedTimer>
#include "DataModel/OrderData.h"
#include "DataModel/PriceTable.h"
#include "Calculation/FreightCalculator.h"
#include "Calculation/RuleManager.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void progressUpdated(int percent);
    void errorOccurred(const QString &message);
    void importProgress(int percent);
    void calcProgress(int percent);

private slots:
    void onImportClicked();
    void onCalculateClicked();
    void onExportClicked();
    void onQuickTestClicked();
    void onRuleManageClicked();
    void onGlobalRulesClicked();
    void updateProgress(int percent);
    void onCalculationComplete(int totalCount, int errorCount);
    void onImportFinished();
    void onExportFinished();
    void onAllFilesProcessed();
    void onCalcModeChanged(const QString &mode);
    void onAvgParamChanged();

    void onFirstPage();
    void onPrevPage();
    void onNextPage();
    void onLastPage();

private:
    Ui::MainWindow *ui;
    QList<OrderData> m_currentOrders;
    QList<QString> m_currentFilePaths;
    FreightCalculator *m_calculator;
    RuleManager *m_ruleManager;
    QFutureWatcher<void> *m_importWatcher;
    QFutureWatcher<void> *m_exportWatcher;
    QFutureWatcher<void> *m_batchWatcher;
    int m_totalFileCount;
    QAtomicInt m_processedFileCount;

    QElapsedTimer m_lastProgressTime;
    int m_lastProgressPercent = -1;

    int m_currentPage = 1;
    int m_pageSize = 5000;
    int m_totalPages = 1;

    QWidget *m_overlayWidget = nullptr;
    class QProgressBar *m_centerProgress = nullptr;
    class QLabel *m_centerProgressLabel = nullptr;

    bool m_isCalculating = false;

    void setupConnections();
    void setupStyle();
    void setupStatusBar();
    void updateTableView();
    void updateCustomerCombo();
    void applyLightStyle();
    void importFilesAsync(const QStringList &filePaths);
    void exportFilesAsync(const QString &outputDir, bool mergeExport, bool perClientExport);

    void showCenterProgress(const QString &text);
    void hideCenterProgress();
    void updateCenterProgress(int percent);

    void throttledProgress(int percent);

    void updatePageInfo();
    void displayPage(int page);
};

#endif // MAINWINDOW_H
