#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QElapsedTimer>
#include <QApplication>
#include <QtConcurrent>
#include <QDir>
#include <QGridLayout>
#include <QProgressBar>
#include <QLabel>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QRegularExpression>
#include <memory>
#include <xlsxdocument.h>

#include "Excel/ExcelEngine.h"
#include "Excel/ExcelImporter.h"
#include "Excel/ExcelExporter.h"
#include "UI/RuleManagerDialog.h"
#include "UI/QuickTestWidget.h"
#include "UI/GlobalRulesDialog.h"
#include "Calculation/SimdCalculator.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_calculator(new FreightCalculator(this))
    , m_ruleManager(new RuleManager(this))
    , m_importWatcher(new QFutureWatcher<void>(this))
    , m_exportWatcher(new QFutureWatcher<void>(this))
    , m_batchWatcher(new QFutureWatcher<void>(this))
    , m_totalFileCount(0)
    , m_processedFileCount(0)
{
    ui->setupUi(this);

    setupConnections();
    setupStatusBar();

    m_ruleManager->initDefaultPriceTable();
    m_ruleManager->loadFromFile(RuleManager::defaultFilePath());
    m_calculator->setDefaultPriceTable(m_ruleManager->defaultPriceTable());
    m_calculator->setGlobalRules(m_ruleManager->globalRules());
    updateCustomerCombo();

    // 从全局规则加载拉均重参数到界面（阻塞信号，避免初始化时触发保存）
    GlobalRules gr = m_ruleManager->globalRules();
    ui->avgBaseSpin->blockSignals(true);
    ui->avgLimitSpin->blockSignals(true);
    ui->avgStepSpin->blockSignals(true);
    ui->avgSurchargeSpin->blockSignals(true);
    ui->avgBaseSpin->setValue(gr.avgWeightBase);
    ui->avgLimitSpin->setValue(gr.avgWeightUpperLimit);
    ui->avgStepSpin->setValue(gr.avgWeightIncrement);
    ui->avgSurchargeSpin->setValue(gr.avgWeightSurcharge);
    ui->avgBaseSpin->blockSignals(false);
    ui->avgLimitSpin->blockSignals(false);
    ui->avgStepSpin->blockSignals(false);
    ui->avgSurchargeSpin->blockSignals(false);

    ui->mainSplitter->setStretchFactor(0, 3);
    ui->mainSplitter->setStretchFactor(1, 1);

    // 初始化：拉均重参数默认隐藏
    onCalcModeChanged(ui->modeCombo->currentText());

    m_overlayWidget = new QWidget(ui->dataTableView->parentWidget());
    m_overlayWidget->setObjectName(QStringLiteral("overlayWidget"));

    auto *overlayLayout = new QVBoxLayout(m_overlayWidget);
    overlayLayout->setAlignment(Qt::AlignCenter);

    m_centerProgressLabel = new QLabel(m_overlayWidget);
    m_centerProgressLabel->setAlignment(Qt::AlignCenter);
    m_centerProgressLabel->setStyleSheet(
        QStringLiteral("font-size: 14px; font-weight: bold; color: #4a90d9;"));

    m_centerProgress = new QProgressBar(m_overlayWidget);
    m_centerProgress->setMinimumWidth(300);
    m_centerProgress->setMaximumWidth(500);
    m_centerProgress->setMinimumHeight(24);
    m_centerProgress->setTextVisible(true);
    m_centerProgress->setStyleSheet(
        QStringLiteral("QProgressBar { font-size: 12px; }"));

    overlayLayout->addStretch();
    overlayLayout->addWidget(m_centerProgressLabel, 0, Qt::AlignCenter);
    overlayLayout->addWidget(m_centerProgress, 0, Qt::AlignCenter);
    overlayLayout->addStretch();

    m_overlayWidget->setStyleSheet(
        QStringLiteral("background-color: rgba(255, 255, 255, 200);"));
    m_overlayWidget->hide();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    connect(ui->importBtn, &QPushButton::clicked, this, &MainWindow::onImportClicked);
    connect(ui->calculateBtn, &QPushButton::clicked, this, &MainWindow::onCalculateClicked);
    connect(ui->exportBtn, &QPushButton::clicked, this, &MainWindow::onExportClicked);
    connect(ui->quickTestBtn, &QPushButton::clicked, this, &MainWindow::onQuickTestClicked);
    connect(ui->ruleManageBtn, &QPushButton::clicked, this, &MainWindow::onRuleManageClicked);
    connect(ui->globalRulesBtn, &QPushButton::clicked, this, &MainWindow::onGlobalRulesClicked);
    connect(ui->startCalcBtn, &QPushButton::clicked, this, &MainWindow::onCalculateClicked);

    connect(m_calculator, &FreightCalculator::progressUpdated, this, &MainWindow::throttledProgress);
    connect(m_calculator, &FreightCalculator::calculationComplete, this, &MainWindow::onCalculationComplete);

    connect(m_importWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::onImportFinished);
    connect(m_exportWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::onExportFinished);
    connect(m_batchWatcher, &QFutureWatcher<void>::finished, this, &MainWindow::onAllFilesProcessed);

    connect(ui->firstPageBtn, &QPushButton::clicked, this, &MainWindow::onFirstPage);
    connect(ui->prevPageBtn, &QPushButton::clicked, this, &MainWindow::onPrevPage);
    connect(ui->nextPageBtn, &QPushButton::clicked, this, &MainWindow::onNextPage);
    connect(ui->lastPageBtn, &QPushButton::clicked, this, &MainWindow::onLastPage);

    connect(this, &MainWindow::importProgress, this, [this](int percent) {
        throttledProgress(percent);
        updateCenterProgress(percent);
    });
    connect(this, &MainWindow::calcProgress, this, [this](int percent) {
        throttledProgress(percent);
        updateCenterProgress(percent);
    });

    connect(ui->modeCombo, &QComboBox::currentTextChanged, this, &MainWindow::onCalcModeChanged);

    // 拉均重参数变化时自动保存到全局规则和文件
    connect(ui->avgBaseSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onAvgParamChanged);
    connect(ui->avgLimitSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onAvgParamChanged);
    connect(ui->avgStepSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onAvgParamChanged);
    connect(ui->avgSurchargeSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &MainWindow::onAvgParamChanged);
}

void MainWindow::setupStyle()
{
    applyLightStyle();
}

void MainWindow::setupStatusBar()
{
    auto *statusLabel = new QLabel(QStringLiteral("就绪"), this);
    statusLabel->setObjectName(QStringLiteral("statusLabel"));

    auto *companyLabel = new QLabel(QStringLiteral("杭州喵喵至家网络有限公司"), this);
    companyLabel->setObjectName(QStringLiteral("companyLabel"));
    companyLabel->setStyleSheet(QStringLiteral("color: #999999; font-size: 10px;"));

    auto *recordLabel = new QLabel(QStringLiteral("记录数: 0"), this);
    recordLabel->setObjectName(QStringLiteral("recordLabel"));

    auto *timeLabel = new QLabel(QStringLiteral("耗时: -"), this);
    timeLabel->setObjectName(QStringLiteral("timeLabel"));

    statusBar()->addWidget(statusLabel, 1);
    statusBar()->addPermanentWidget(companyLabel);
    statusBar()->addPermanentWidget(recordLabel);
    statusBar()->addPermanentWidget(timeLabel);
}

void MainWindow::showCenterProgress(const QString &text)
{
    m_centerProgressLabel->setText(text);
    m_centerProgress->setValue(0);
    m_lastProgressPercent = -1;
    m_lastProgressTime.start();

    QWidget *parent = ui->dataTableView->parentWidget();
    QPoint topLeft = ui->dataTableView->mapTo(parent, QPoint(0, 0));
    m_overlayWidget->setGeometry(
        topLeft.x(), topLeft.y(),
        ui->dataTableView->width(),
        ui->dataTableView->height());
    m_overlayWidget->raise();
    m_overlayWidget->show();
}

void MainWindow::hideCenterProgress()
{
    m_overlayWidget->hide();
}

void MainWindow::updateCenterProgress(int percent)
{
    if (m_centerProgress && m_overlayWidget && m_overlayWidget->isVisible()) {
        m_centerProgress->setValue(percent);
    }
}

void MainWindow::throttledProgress(int percent)
{
    if (percent == m_lastProgressPercent)
        return;

    if (percent == 0 || percent == 100 || percent - m_lastProgressPercent >= 1) {
        if (percent != 100 && m_lastProgressTime.isValid() && m_lastProgressTime.elapsed() < 30) {
            return;
        }
        m_lastProgressPercent = percent;
        m_lastProgressTime.restart();
        updateProgress(percent);
    }
}

void MainWindow::applyLightStyle()
{
    QString style = R"(
        QMainWindow {
            background-color: #f5f6fa;
        }

        QPushButton {
            background-color: #4a90d9;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 4px 12px;
            font-size: 11px;
            min-height: 28px;
        }
        QPushButton:hover {
            background-color: #3a7bc8;
        }
        QPushButton:pressed {
            background-color: #2d6bb3;
        }
        QPushButton:disabled {
            background-color: #d0d0d0;
            color: #808080;
        }

        QComboBox {
            background-color: white;
            color: #333333;
            border: 1px solid #d0d0d0;
            border-radius: 4px;
            padding: 3px 8px;
            font-size: 11px;
            min-height: 24px;
        }
        QComboBox:hover {
            border-color: #4a90d9;
        }
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
        QComboBox::down-arrow {
            image: none;
            border-left: 4px solid transparent;
            border-right: 4px solid transparent;
            border-top: 5px solid #666666;
            margin-right: 4px;
        }
        QComboBox QAbstractItemView {
            background-color: white;
            color: #333333;
            selection-background-color: #e0e8f5;
            selection-color: #2d6bb3;
            border: 1px solid #d0d0d0;
            outline: none;
            font-size: 11px;
        }

        QSpinBox {
            background-color: white;
            color: #333333;
            border: 1px solid #d0d0d0;
            border-radius: 4px;
            padding: 3px 8px;
            font-size: 11px;
            min-height: 24px;
        }
        QSpinBox:hover {
            border-color: #4a90d9;
        }
        QSpinBox::up-button, QSpinBox::down-button {
            background-color: #f0f0f0;
            border: none;
            width: 18px;
        }
        QSpinBox::up-button:hover, QSpinBox::down-button:hover {
            background-color: #e0e0e0;
        }
        QSpinBox::up-arrow {
            border-left: 3px solid transparent;
            border-right: 3px solid transparent;
            border-bottom: 4px solid #666666;
        }
        QSpinBox::down-arrow {
            border-left: 3px solid transparent;
            border-right: 3px solid transparent;
            border-top: 4px solid #666666;
        }

        QTableView {
            background-color: white;
            alternate-background-color: #fafafa;
            color: #333333;
            gridline-color: #e0e0e0;
            border: 1px solid #d0d0d0;
            border-radius: 4px;
            font-size: 11px;
            selection-background-color: #e0e8f5;
            selection-color: #2d6bb3;
            outline: none;
        }
        QTableView::item {
            padding: 3px 6px;
        }
        QHeaderView::section {
            background-color: #f5f6fa;
            color: #555555;
            padding: 4px 6px;
            border: none;
            border-right: 1px solid #e0e0e0;
            border-bottom: 1px solid #e0e0e0;
            font-weight: bold;
            font-size: 11px;
        }
        QHeaderView::section:hover {
            background-color: #e8eaef;
        }

        QProgressBar {
            background-color: #e0e0e0;
            border: 1px solid #d0d0d0;
            border-radius: 4px;
            text-align: center;
            color: #555555;
            font-size: 10px;
            min-height: 16px;
        }
        QProgressBar::chunk {
            background-color: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #4a90d9, stop:1 #3a7bc8);
            border-radius: 3px;
        }

        QGroupBox {
            background-color: white;
            border: 1px solid #e0e0e0;
            border-radius: 6px;
            margin-top: 8px;
            padding-top: 12px;
            color: #333333;
            font-size: 12px;
            font-weight: bold;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            subcontrol-position: top left;
            padding: 2px 8px;
            color: #4a90d9;
        }

        QLabel {
            color: #555555;
            font-size: 11px;
        }

        QStatusBar {
            background-color: #f5f6fa;
            color: #888888;
            font-size: 10px;
            border-top: 1px solid #e0e0e0;
        }
        QStatusBar::item {
            border: none;
            padding: 0 6px;
        }
        QStatusBar QLabel {
            color: #888888;
            font-size: 10px;
            padding: 0 4px;
        }

        QSplitter::handle {
            background-color: #d0d0d0;
        }
        QSplitter::handle:horizontal {
            width: 2px;
        }
        QSplitter::handle:vertical {
            height: 2px;
        }
        QSplitter::handle:hover {
            background-color: #4a90d9;
        }

        QScrollBar:vertical {
            background-color: #f0f0f0;
            width: 8px;
            border: none;
        }
        QScrollBar::handle:vertical {
            background-color: #c0c0c0;
            border-radius: 4px;
            min-height: 24px;
        }
        QScrollBar::handle:vertical:hover {
            background-color: #4a90d9;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar:horizontal {
            background-color: #f0f0f0;
            height: 8px;
            border: none;
        }
        QScrollBar::handle:horizontal {
            background-color: #c0c0c0;
            border-radius: 4px;
            min-width: 24px;
        }
        QScrollBar::handle:horizontal:hover {
            background-color: #4a90d9;
        }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
            width: 0;
        }
    )";

    qApp->setStyleSheet(style);
}

void MainWindow::onImportClicked()
{
    auto *statusLabel = statusBar()->findChild<QLabel*>(QStringLiteral("statusLabel"));
    if (statusLabel)
        statusLabel->setText(QStringLiteral("正在打开文件选择对话框..."));

    QStringList filePaths = QFileDialog::getOpenFileNames(
        this,
        QStringLiteral("选择Excel文件（最多5个）"),
        QString(),
        QStringLiteral("Excel文件 (*.xlsx *.xls)"),
        nullptr,
        QFileDialog::DontUseNativeDialog
    );

    if (statusLabel)
        statusLabel->setText(QStringLiteral("文件选择完成"));

    if (filePaths.isEmpty())
        return;

    if (filePaths.size() > 5) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("最多选择5个文件进行处理"));
        filePaths = filePaths.mid(0, 5);
    }

    m_currentFilePaths = filePaths;
    m_currentOrders.clear();
    m_currentPage = 1;

    if (statusLabel)
        statusLabel->setText(QStringLiteral("正在导入..."));

    ui->progressBar->setValue(0);
    ui->importBtn->setEnabled(false);
    ui->calculateBtn->setEnabled(false);
    ui->exportBtn->setEnabled(false);

    showCenterProgress(QStringLiteral("正在导入文件..."));

    importFilesAsync(filePaths);
}

void MainWindow::importFilesAsync(const QStringList &filePaths)
{
    m_totalFileCount = filePaths.size();
    m_processedFileCount = 0;

    QPointer<MainWindow> self = this;

    auto sharedOrders = std::make_shared<QList<OrderData>>();
    auto sharedMutex = std::make_shared<QMutex>();
    auto sharedProcessedRows = std::make_shared<QAtomicInt>(0);
    auto sharedTotalRows = std::make_shared<QAtomicInt>(0);

    emit importProgress(0);

    auto processFunc = [self, filePaths, sharedOrders, sharedMutex, sharedProcessedRows, sharedTotalRows]() {
        try {
            int fileCount = filePaths.size();

            QList<int> fileRowEstimates;
            fileRowEstimates.resize(fileCount);
            int total = 0;

            QList<QFuture<void>> scanFutures;
            for (int i = 0; i < fileCount; ++i) {
                scanFutures.append(QtConcurrent::run([&, i]() {
                    ExcelEngine engine;
                    fileRowEstimates[i] = qMax(0, engine.countRowsFast(filePaths[i]));
                }));
            }
            for (auto &f : scanFutures) f.waitForFinished();

            for (int v : fileRowEstimates) total += v;
            sharedTotalRows->storeRelease(total);

            if (total > 0) {
                sharedOrders->reserve(total + 1000);

                int maxThreads = qMin(4, fileCount);
                QThreadPool::globalInstance()->setMaxThreadCount(maxThreads);

                QList<QFuture<void>> importFutures;
                for (int i = 0; i < fileCount; ++i) {
                    importFutures.append(QtConcurrent::run([&, i]() {
                        ExcelImporter importer;
                        int estimate = fileRowEstimates[i];

                        QObject::connect(&importer, &ExcelImporter::progressUpdated, self.data(),
                            [self, sharedProcessedRows, sharedTotalRows, estimate](int filePercent) {
                                if (!self) return;
                                int localDone = static_cast<int>(filePercent / 100.0 * estimate);
                                int globalDone = sharedProcessedRows->loadAcquire() + localDone;
                                int total = sharedTotalRows->loadAcquire();
                                if (total > 0) {
                                    int percent = static_cast<int>(globalDone * 100.0 / total);
                                    self->importProgress(qMin(99, percent));
                                }
                            }, Qt::QueuedConnection);

                        QList<OrderData> orders = importer.importFromFile(filePaths[i]);

                        QMutexLocker locker(sharedMutex.get());
                        sharedOrders->append(orders);
                        sharedProcessedRows->fetchAndAddOrdered(orders.size());

                        int done = sharedProcessedRows->loadAcquire();
                        int total = sharedTotalRows->loadAcquire();
                        if (total > 0 && done % 5000 == 0) {
                            int percent = static_cast<int>(done * 100.0 / total);
                            if (self) {
                                self->importProgress(qMin(99, percent));
                            }
                        }
                    }));
                }

                for (auto &f : importFutures) f.waitForFinished();
            }
        } catch (...) {
            QMetaObject::invokeMethod(self.data(), [self]() {
                if (self) {
                    auto *statusLabel = self->statusBar()->findChild<QLabel*>(QStringLiteral("statusLabel"));
                    if (statusLabel)
                        statusLabel->setText(QStringLiteral("导入发生错误"));
                }
            }, Qt::QueuedConnection);
        }

        QMetaObject::invokeMethod(self.data(), [self, sharedOrders]() {
            if (self) {
                self->m_currentOrders = std::move(*sharedOrders);
                emit self->importProgress(100);
                self->onImportFinished();
            }
        }, Qt::QueuedConnection);
    };

    QFuture<void> future = QtConcurrent::run(processFunc);
    m_importWatcher->setFuture(future);
}

void MainWindow::onImportFinished()
{
    hideCenterProgress();
    ui->importBtn->setEnabled(true);
    ui->calculateBtn->setEnabled(true);
    ui->exportBtn->setEnabled(true);

    if (!m_currentOrders.isEmpty()) {
        m_currentPage = 1;
        updateTableView();

        auto *recordLabel = statusBar()->findChild<QLabel*>(QStringLiteral("recordLabel"));
        if (recordLabel)
            recordLabel->setText(QStringLiteral("记录数: %1").arg(m_currentOrders.size()));
    }

    auto *statusLabel = statusBar()->findChild<QLabel*>(QStringLiteral("statusLabel"));
    if (statusLabel)
        statusLabel->setText(QStringLiteral("导入完成，共 %1 条").arg(m_currentOrders.size()));

    ui->progressBar->setValue(100);

    // 导入完成后自动开始计算
    if (!m_currentOrders.isEmpty()) {
        onCalculateClicked();
    }
}

void MainWindow::onCalculateClicked()
{
    if (m_currentOrders.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请先导入订单数据。"));
        return;
    }

    if (m_isCalculating) {
        return;
    }
    m_isCalculating = true;

    auto *statusLabel = statusBar()->findChild<QLabel*>(QStringLiteral("statusLabel"));
    if (statusLabel)
        statusLabel->setText(QStringLiteral("正在计算..."));

    ui->startCalcBtn->setEnabled(false);
    ui->calculateBtn->setEnabled(false);
    ui->progressBar->setValue(0);

    showCenterProgress(QStringLiteral("正在计算运费..."));

    QPointer<MainWindow> self = this;
    QString mode = ui->modeCombo->currentText();
    int threadCount = ui->threadSpin->value();
    QString selectedCustomer = ui->customerCombo->currentText();
    QMap<QString, CustomerRule> ruleMap;
    QStringList ruleNames = m_ruleManager->ruleNames();
    for (const QString &name : ruleNames) {
        ruleMap[name] = m_ruleManager->getRule(name);
    }
    QString firstRuleName = ruleNames.isEmpty() ? QString() : ruleNames.first();
    QList<PriceRule> defaultPriceTable = m_calculator->defaultPriceTable();
    GlobalRules globalRules = m_calculator->globalRules();

    // 读取拉均重参数
    globalRules.avgWeightBase = ui->avgBaseSpin->value();
    globalRules.avgWeightUpperLimit = ui->avgLimitSpin->value();
    globalRules.avgWeightIncrement = ui->avgStepSpin->value();
    globalRules.avgWeightSurcharge = ui->avgSurchargeSpin->value();

    QList<OrderData> orders = m_currentOrders;

    // 根据控制面板的客户选择过滤数据
    if (!selectedCustomer.isEmpty() && selectedCustomer != QStringLiteral("全部")) {
        QList<OrderData> filtered;
        filtered.reserve(orders.size());
        for (const OrderData &order : orders) {
            QString client = order.client.isEmpty() ? order.customer : order.client;
            if (client == selectedCustomer) {
                filtered.append(order);
            }
        }
        orders = std::move(filtered);
    }

    auto calcFunc = [self, orders = std::move(orders), mode, threadCount, ruleMap, firstRuleName,
                     defaultPriceTable, globalRules]() mutable {
        if (!self) return;

        int totalCount = orders.size();
        QAtomicInt processed(0);
        QAtomicInt errorCount(0);
        int effectiveThreads = qMax(1, qMin(threadCount, QThread::idealThreadCount()));

        QMap<QString, QPair<int, int>> clientRanges;
        QMap<QString, int> clientCounts;

        for (int i = 0; i < totalCount; ++i) {
            const OrderData &order = orders[i];
            QString client = order.client.isEmpty() ? order.customer : order.client;
            clientCounts[client]++;
        }

        int pos = 0;
        for (auto it = clientCounts.constBegin(); it != clientCounts.constEnd(); ++it) {
            clientRanges[it.key()] = qMakePair(pos, pos + it.value());
            pos += it.value();
        }

        QList<OrderData> sortedOrders;
        sortedOrders.reserve(totalCount);
        sortedOrders.resize(totalCount);

        QMap<QString, int> clientInsertPos;
        for (auto it = clientRanges.constBegin(); it != clientRanges.constEnd(); ++it) {
            clientInsertPos[it.key()] = it.value().first;
        }

        for (int i = 0; i < totalCount; ++i) {
            const OrderData &order = orders[i];
            QString client = order.client.isEmpty() ? order.customer : order.client;
            int &insertIdx = clientInsertPos[client];
            sortedOrders[insertIdx] = order;
            insertIdx++;
        }

        orders.clear();

        QList<QFuture<void>> calcFutures;

        for (auto it = clientRanges.constBegin(); it != clientRanges.constEnd(); ++it) {
            QString client = it.key();
            int start = it.value().first;
            int end = it.value().second;
            int count = end - start;
            if (count <= 0) continue;

            CustomerRule rule = ruleMap.value(client);
            if (rule.customerName.isEmpty()) {
                if (!firstRuleName.isEmpty()) {
                    rule = ruleMap.value(firstRuleName);
                } else {
                    rule.customerName = client;
                    rule.useDefaultPrice = true;
                }
            }
            rule.calculationMode = mode;

            // 拉均重模式：计算客户平均重量和每包裹加价
            GlobalRules clientGlobalRules = globalRules;
            if (mode == QStringLiteral("拉均重")) {
                double totalWeight = 0.0;
                int validCount = 0;
                for (int i = start; i < end; ++i) {
                    double w = std::max(sortedOrders[i].actualWeight, sortedOrders[i].volumetricWeight);
                    if (w > 0) {
                        totalWeight += w;
                        validCount++;
                    }
                }
                if (validCount > 0) {
                    double avgWeight = totalWeight / validCount;
                    double base = globalRules.avgWeightBase;
                    double limit = globalRules.avgWeightUpperLimit;
                    double step = globalRules.avgWeightIncrement;
                    double surcharge = globalRules.avgWeightSurcharge;

                    if (avgWeight > base && step > 0) {
                        // 超出部分封顶在上限
                        double exceeded = std::min(avgWeight, limit) - base;
                        if (exceeded > 0) {
                            int increments = static_cast<int>(std::ceil(exceeded / step));
                            clientGlobalRules.runtimeAvgSurcharge = increments * surcharge;
                        }
                    }
                }
            }

            int chunks = qMax(1, qMin(effectiveThreads, count / 5000 + 1));
            int chunkSize = (count + chunks - 1) / chunks;

            for (int c = 0; c < chunks; ++c) {
                int cs = start + c * chunkSize;
                int ce = qMin(start + (c + 1) * chunkSize, end);
                if (cs >= ce) continue;

                calcFutures.append(QtConcurrent::run([&, cs, ce, rule, clientGlobalRules]() {
                    OrderData *dataPtr = sortedOrders.data();
                    int chunkCount = ce - cs;

                    // SIMD加速批量计算
                    int errors = SimdCalculator::calculateChunk(
                        dataPtr + cs, chunkCount, rule, defaultPriceTable, clientGlobalRules
                    );
                    errorCount.fetchAndAddRelaxed(errors);

                    int done = processed.fetchAndAddRelaxed(chunkCount) + chunkCount;
                    if (done % 10000 == 0 || done == totalCount) {
                        int percent = static_cast<int>(done * 100.0 / totalCount);
                        if (self) {
                            self->calcProgress(qMin(99, percent));
                        }
                    }
                }));
            }
        }

        for (auto &f : calcFutures)
            f.waitForFinished();

        QMetaObject::invokeMethod(self.data(), [self, sortedOrders = std::move(sortedOrders), totalCount, errorCount]() mutable {
            if (!self) return;
            self->m_currentOrders = std::move(sortedOrders);
            self->onCalculationComplete(totalCount, errorCount.loadAcquire());
        }, Qt::QueuedConnection);
    };

    QFuture<void> future = QtConcurrent::run(calcFunc);
    m_batchWatcher->setFuture(future);
}

void MainWindow::onExportClicked()
{
    if (m_currentOrders.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("没有可导出的数据。"));
        return;
    }

    QDialog exportDialog(this);
    exportDialog.setWindowTitle(QStringLiteral("导出选项"));
    exportDialog.setMinimumWidth(300);

    auto *layout = new QVBoxLayout(&exportDialog);

    auto *mergeCheck = new QCheckBox(QStringLiteral("合并导出（所有数据导出到一个文件）"), &exportDialog);
    mergeCheck->setChecked(true);

    auto *perClientCheck = new QCheckBox(QStringLiteral("按客户分别导出（每个客户一个文件）"), &exportDialog);
    perClientCheck->setChecked(false);

    auto *summaryCheck = new QCheckBox(QStringLiteral("导出汇总表"), &exportDialog);
    summaryCheck->setChecked(true);

    layout->addWidget(mergeCheck);
    layout->addWidget(perClientCheck);
    layout->addWidget(summaryCheck);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &exportDialog);
    connect(buttonBox, &QDialogButtonBox::accepted, &exportDialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &exportDialog, &QDialog::reject);
    layout->addWidget(buttonBox);

    if (exportDialog.exec() != QDialog::Accepted)
        return;

    bool mergeExport = mergeCheck->isChecked();
    bool perClientExport = perClientCheck->isChecked();

    if (!mergeExport && !perClientExport) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请至少选择一种导出方式。"));
        return;
    }

    QString outputDir = QFileDialog::getExistingDirectory(
        this,
        QStringLiteral("选择导出目录"),
        QString(),
        QFileDialog::DontUseNativeDialog
    );

    if (outputDir.isEmpty())
        return;

    auto *statusLabel = statusBar()->findChild<QLabel*>(QStringLiteral("statusLabel"));
    if (statusLabel)
        statusLabel->setText(QStringLiteral("正在导出..."));

    ui->progressBar->setValue(0);
    ui->exportBtn->setEnabled(false);

    showCenterProgress(QStringLiteral("正在导出数据..."));

    exportFilesAsync(outputDir, mergeExport, perClientExport);
}

void MainWindow::exportFilesAsync(const QString &outputDir, bool mergeExport, bool perClientExport)
{
    QPointer<MainWindow> self = this;
    QList<OrderData> orders = m_currentOrders;

    auto exportFunc = [self, outputDir, orders, mergeExport, perClientExport]() {
        if (!self) return;

        try {
            QDir dir(outputDir);
            int totalSteps = (mergeExport ? 1 : 0) + (perClientExport ? 1 : 0);
            int step = 0;

            if (mergeExport) {
                QString fileName = QStringLiteral("运费计算结果_全部_%1.xlsx")
                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
                QString filePath = dir.filePath(fileName);

                ExcelExporter exporter;
                QObject::connect(&exporter, &ExcelExporter::progressUpdated, self.data(),
                    [self, step, totalSteps](int filePercent) {
                        if (!self) return;
                        int overall = static_cast<int>((step * 100.0 + filePercent) / totalSteps);
                        self->throttledProgress(overall);
                        self->updateCenterProgress(overall);
                    }, Qt::QueuedConnection);

                exporter.exportToFile(filePath, orders);
                step++;

                QString summaryName = QStringLiteral("运费汇总_%1.xlsx")
                    .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
                exporter.exportSummary(dir.filePath(summaryName), orders);
            }

            if (perClientExport) {
                QMap<QString, QList<OrderData>> ordersByClient;
                for (const OrderData &order : orders) {
                    QString client = order.client.isEmpty() ? order.customer : order.client;
                    if (client.isEmpty())
                        client = QStringLiteral("未知客户");
                    ordersByClient[client].append(order);
                }

                int clientIndex = 0;
                int totalClients = ordersByClient.size();

                for (auto it = ordersByClient.constBegin(); it != ordersByClient.constEnd(); ++it) {
                    QString client = it.key();
                    QString safeName = client;
                    safeName.replace(QRegularExpression("[\\\\/:*?\"<>|]"), "_");

                    QString fileName = QStringLiteral("运费结果_%1_%2.xlsx")
                        .arg(safeName)
                        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
                    QString filePath = dir.filePath(fileName);

                    ExcelExporter exporter;
                    exporter.exportToFile(filePath, it.value());

                    clientIndex++;
                    int overall = static_cast<int>(((step + clientIndex * 1.0 / totalClients) * 100.0) / (step + 1));
                    QMetaObject::invokeMethod(self.data(), [self, overall]() {
                        if (self) {
                            self->throttledProgress(overall);
                            self->updateCenterProgress(overall);
                        }
                    }, Qt::QueuedConnection);
                }
            }
        } catch (...) {
            QMetaObject::invokeMethod(self.data(), [self]() {
                if (self) {
                    auto *statusLabel = self->statusBar()->findChild<QLabel*>(QStringLiteral("statusLabel"));
                    if (statusLabel)
                        statusLabel->setText(QStringLiteral("导出发生错误"));
                }
            }, Qt::QueuedConnection);
        }
    };

    QFuture<void> future = QtConcurrent::run(exportFunc);
    m_exportWatcher->setFuture(future);
}

void MainWindow::onQuickTestClicked()
{
    QuickTestWidget dialog(m_ruleManager, this);
    dialog.exec();
}

void MainWindow::onRuleManageClicked()
{
    RuleManagerDialog dialog(m_ruleManager, this);
    dialog.exec();
    m_calculator->setDefaultPriceTable(m_ruleManager->defaultPriceTable());
    updateCustomerCombo();
}

void MainWindow::onGlobalRulesClicked()
{
    GlobalRulesDialog dialog(m_ruleManager, this);
    dialog.exec();
    GlobalRules gr = m_ruleManager->globalRules();
    m_calculator->setGlobalRules(gr);

    // 同步全局规则中的拉均重参数到主界面（阻塞信号避免循环触发保存）
    ui->avgBaseSpin->blockSignals(true);
    ui->avgLimitSpin->blockSignals(true);
    ui->avgStepSpin->blockSignals(true);
    ui->avgSurchargeSpin->blockSignals(true);
    ui->avgBaseSpin->setValue(gr.avgWeightBase);
    ui->avgLimitSpin->setValue(gr.avgWeightUpperLimit);
    ui->avgStepSpin->setValue(gr.avgWeightIncrement);
    ui->avgSurchargeSpin->setValue(gr.avgWeightSurcharge);
    ui->avgBaseSpin->blockSignals(false);
    ui->avgLimitSpin->blockSignals(false);
    ui->avgStepSpin->blockSignals(false);
    ui->avgSurchargeSpin->blockSignals(false);
}

void MainWindow::onCalcModeChanged(const QString &mode)
{
    bool showAvg = (mode == QStringLiteral("拉均重"));

    QFormLayout *formLayout = ui->formLayout;
    if (!formLayout)
        return;

    // 遍历 formLayout 中的所有行，找到拉均重相关的行并设置可见性
    // 通过控件指针查找行号，确保即使UI行顺序变化也能正确匹配
    QWidget *avgWidgets[] = {
        ui->avgBaseSpin, ui->avgLimitSpin, ui->avgStepSpin, ui->avgSurchargeSpin
    };

    for (QWidget *widget : avgWidgets) {
        int row = -1;
        QFormLayout::ItemRole role;
        int index = formLayout->indexOf(widget);
        if (index >= 0) {
            formLayout->getItemPosition(index, &row, &role);
            if (row >= 0) {
                formLayout->setRowVisible(row, showAvg);
            }
        }
    }
}

void MainWindow::onAvgParamChanged()
{
    GlobalRules rules = m_ruleManager->globalRules();
    rules.avgWeightBase = ui->avgBaseSpin->value();
    rules.avgWeightUpperLimit = ui->avgLimitSpin->value();
    rules.avgWeightIncrement = ui->avgStepSpin->value();
    rules.avgWeightSurcharge = ui->avgSurchargeSpin->value();
    m_ruleManager->setGlobalRules(rules);
    m_calculator->setGlobalRules(rules);
    m_ruleManager->saveToFile(RuleManager::defaultFilePath());
}

void MainWindow::updateProgress(int percent)
{
    ui->progressBar->setValue(percent);

    auto *statusLabel = statusBar()->findChild<QLabel*>(QStringLiteral("statusLabel"));
    if (statusLabel)
        statusLabel->setText(QStringLiteral("进度: %1%").arg(percent));
}

void MainWindow::onCalculationComplete(int totalCount, int errorCount)
{
    m_isCalculating = false;
    hideCenterProgress();
    ui->startCalcBtn->setEnabled(true);
    ui->calculateBtn->setEnabled(true);
    ui->progressBar->setValue(100);

    m_currentPage = 1;
    updateTableView();

    auto *statusLabel = statusBar()->findChild<QLabel*>(QStringLiteral("statusLabel"));
    if (statusLabel)
        statusLabel->setText(QStringLiteral("计算完成"));

    QMessageBox::information(this, QStringLiteral("计算完成"),
                             QStringLiteral("总计: %1 条\n成功: %2 条\n失败: %3 条")
                             .arg(totalCount)
                             .arg(totalCount - errorCount)
                             .arg(errorCount));
}

void MainWindow::onExportFinished()
{
    hideCenterProgress();
    ui->exportBtn->setEnabled(true);
    ui->progressBar->setValue(100);

    auto *statusLabel = statusBar()->findChild<QLabel*>(QStringLiteral("statusLabel"));
    if (statusLabel)
        statusLabel->setText(QStringLiteral("导出完成"));

    QMessageBox::information(this, QStringLiteral("导出成功"),
                             QStringLiteral("数据已导出到选择的目录。"));
}

void MainWindow::onAllFilesProcessed()
{
}

void MainWindow::onFirstPage()
{
    if (m_currentPage != 1) {
        m_currentPage = 1;
        displayPage(m_currentPage);
    }
}

void MainWindow::onPrevPage()
{
    if (m_currentPage > 1) {
        m_currentPage--;
        displayPage(m_currentPage);
    }
}

void MainWindow::onNextPage()
{
    if (m_currentPage < m_totalPages) {
        m_currentPage++;
        displayPage(m_currentPage);
    }
}

void MainWindow::onLastPage()
{
    if (m_currentPage != m_totalPages) {
        m_currentPage = m_totalPages;
        displayPage(m_currentPage);
    }
}

void MainWindow::updatePageInfo()
{
    int totalCount = m_currentOrders.size();
    m_totalPages = qMax(1, (totalCount + m_pageSize - 1) / m_pageSize);
    if (m_currentPage > m_totalPages)
        m_currentPage = m_totalPages;

    ui->pageInfoLabel->setText(
        QStringLiteral("第 %1/%2 页 (共 %3 条)")
            .arg(m_currentPage)
            .arg(m_totalPages)
            .arg(totalCount));

    ui->firstPageBtn->setEnabled(m_currentPage > 1);
    ui->prevPageBtn->setEnabled(m_currentPage > 1);
    ui->nextPageBtn->setEnabled(m_currentPage < m_totalPages);
    ui->lastPageBtn->setEnabled(m_currentPage < m_totalPages);
}

void MainWindow::displayPage(int page)
{
    int totalCount = m_currentOrders.size();
    if (totalCount == 0) {
        updatePageInfo();
        return;
    }

    int startIdx = (page - 1) * m_pageSize;
    int endIdx = qMin(startIdx + m_pageSize, totalCount);

    auto *model = new QStandardItemModel(this);
    model->setHorizontalHeaderLabels({
        QStringLiteral("运单号"),
        QStringLiteral("业务时间"),
        QStringLiteral("体积重(kg)"),
        QStringLiteral("订单客户"),
        QStringLiteral("结算对象"),
        QStringLiteral("目的省份"),
        QStringLiteral("实际重量(kg)"),
        QStringLiteral("运费(元)"),
        QStringLiteral("错误信息")
    });

    for (int i = startIdx; i < endIdx; ++i) {
        const OrderData &order = m_currentOrders[i];
        QList<QStandardItem*> row;
        row << new QStandardItem(order.waybillNo);
        row << new QStandardItem(order.businessTime.toString(QStringLiteral("yyyy-MM-dd")));
        row << new QStandardItem(QString::number(order.volumetricWeight, 'f', 2));
        row << new QStandardItem(order.customer);
        row << new QStandardItem(order.client);
        row << new QStandardItem(order.destinationProvince);
        row << new QStandardItem(QString::number(order.actualWeight, 'f', 2));
        row << new QStandardItem(order.freight > 0 ? QString::number(order.freight, 'f', 2) : QString());
        row << new QStandardItem(order.errorMessage);

        for (auto *item : row) {
            item->setEditable(false);
            item->setTextAlignment(Qt::AlignCenter);
        }
        row.last()->setTextAlignment(Qt::AlignLeft | Qt::AlignVCenter);

        model->appendRow(row);
    }

    QAbstractItemModel *oldModel = ui->dataTableView->model();
    ui->dataTableView->setModel(model);
    delete oldModel;

    ui->dataTableView->resizeColumnsToContents();
    ui->dataTableView->horizontalHeader()->setStretchLastSection(true);

    updatePageInfo();
}

void MainWindow::updateTableView()
{
    displayPage(m_currentPage);
}

void MainWindow::updateCustomerCombo()
{
    ui->customerCombo->clear();
    ui->customerCombo->addItem(QStringLiteral("全部"));
    QStringList names = m_ruleManager->ruleNames();
    ui->customerCombo->addItems(names);
}
