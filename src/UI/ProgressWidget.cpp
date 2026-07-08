#include "ProgressWidget.h"
#include <QVBoxLayout>

ProgressWidget::ProgressWidget(QWidget *parent)
    : QWidget(parent)
    , m_progressBar(nullptr)
    , m_percentLabel(nullptr)
    , m_statusLabel(nullptr)
    , m_cancelButton(nullptr)
{
    setupUI();
    applyDarkStyle();
}

void ProgressWidget::setupUI()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 8, 12, 8);
    mainLayout->setSpacing(8);

    // 进度条 + 百分比
    auto *progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(8);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    m_progressBar->setMinimumHeight(8);

    m_percentLabel = new QLabel(QStringLiteral("0%"), this);
    m_percentLabel->setMinimumWidth(42);
    m_percentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    progressLayout->addWidget(m_progressBar);
    progressLayout->addWidget(m_percentLabel);

    mainLayout->addLayout(progressLayout);

    // 状态文字 + 取消按钮
    auto *statusLayout = new QHBoxLayout();
    statusLayout->setSpacing(8);

    m_statusLabel = new QLabel(QStringLiteral("就绪"), this);
    m_statusLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_cancelButton = new QPushButton(QStringLiteral("取消"), this);
    m_cancelButton->setObjectName(QStringLiteral("cancelButton"));
    m_cancelButton->setMinimumHeight(36);
    m_cancelButton->setFixedWidth(80);
    m_cancelButton->setVisible(false);

    statusLayout->addWidget(m_statusLabel, 1);
    statusLayout->addWidget(m_cancelButton);

    mainLayout->addLayout(statusLayout);

    connect(m_cancelButton, &QPushButton::clicked, this, &ProgressWidget::cancelled);
}

void ProgressWidget::applyDarkStyle()
{
    setStyleSheet(QStringLiteral(R"(
        ProgressWidget {
            background-color: #1a1a2e;
            color: #eaeaea;
        }
        QLabel {
            color: #eaeaea;
            background: transparent;
        }
        QProgressBar {
            background-color: #16213e;
            border: none;
            border-radius: 4px;
            min-height: 8px;
        }
        QProgressBar::chunk {
            background-color: #0f3460;
            border-radius: 4px;
        }
        QPushButton#cancelButton {
            background-color: transparent;
            color: #e94560;
            border: 1px solid #e94560;
            border-radius: 8px;
            padding: 4px 12px;
            min-height: 36px;
        }
        QPushButton#cancelButton:hover {
            background-color: #e94560;
            color: #eaeaea;
        }
    )"));
}

void ProgressWidget::setProgress(int percent)
{
    m_progressBar->setValue(qBound(0, percent, 100));
    m_percentLabel->setText(QStringLiteral("%1%").arg(percent));
}

void ProgressWidget::setStatusText(const QString &text)
{
    m_statusLabel->setText(text);
}

void ProgressWidget::setCancelButtonVisible(bool visible)
{
    m_cancelButton->setVisible(visible);
}
