#include "LoginDialog.h"
#include "ui_LoginDialog.h"
#include "LicenseManager.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QMessageBox>
#include <QRegularExpression>
#include <QTimer>
#include <QPainter>
#include <QPaintEvent>

// 现代化配色
static const QString PRIMARY_COLOR    = QStringLiteral("#2563EB");
static const QString PRIMARY_HOVER    = QStringLiteral("#1D4ED8");
static const QString SUCCESS_COLOR    = QStringLiteral("#10B981");
static const QString WARNING_COLOR    = QStringLiteral("#F59E0B");
static const QString DANGER_COLOR     = QStringLiteral("#EF4444");
static const QString BG_DARK          = QStringLiteral("#0F172A");
static const QString BG_CARD          = QStringLiteral("#1E293B");
static const QString BG_INPUT         = QStringLiteral("#334155");
static const QString BG_TRIAL         = QStringLiteral("#1E3A5F");
static const QString TEXT_LIGHT       = QStringLiteral("#FFFFFF");
static const QString TEXT_PRIMARY     = QStringLiteral("#E2E8F0");
static const QString TEXT_SECONDARY   = QStringLiteral("#64748B");
static const QString BORDER_COLOR     = QStringLiteral("#475569");

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
    , m_licenseManager(std::make_unique<LicenseManager>())
{
    ui->setupUi(this);
    setWindowTitle(QStringLiteral("软件授权"));
    setWindowFlags(Qt::Dialog | Qt::WindowTitleHint | Qt::CustomizeWindowHint);
    setFixedSize(560, 600);
    setAttribute(Qt::WA_StyledBackground, true);
    setAutoFillBackground(true);

    setupStyle();

    // 隐藏机器码显示框和复制按钮
    ui->machineCodeTitleLabel->setVisible(false);
    ui->machineCodeLabel->setVisible(false);
    ui->copyBtn->setVisible(false);

    updateStatusDisplay();

    connect(ui->activateBtn, &QPushButton::clicked, this, &LoginDialog::onActivateClicked);
    connect(ui->trialBtn, &QPushButton::clicked, this, &LoginDialog::onTrialClicked);
    connect(ui->copyBtn, &QPushButton::clicked, this, &LoginDialog::onCopyMachineCodeClicked);
    connect(ui->buyBtn, &QPushButton::clicked, this, &LoginDialog::onBuyLicenseClicked);
    connect(ui->licenseInput, &QLineEdit::textChanged, this, &LoginDialog::onTextChanged);

    updateTrialButtonState();

    if (m_licenseManager->isAuthorized()) {
        m_authorized = true;
        if (!m_licenseManager->isTrialValid() || m_licenseManager->currentLicense().isValid) {
            QTimer::singleShot(800, this, [this]() { accept(); });
        }
    }
}

LoginDialog::~LoginDialog() = default;

bool LoginDialog::isAuthorized() const
{
    return m_authorized;
}

void LoginDialog::setupStyle()
{
    setStyleSheet(QStringLiteral(
        "QDialog {"
        "  background-color: %1;"
        "}"
        "QLabel {"
        "  color: %2;"
        "  font-family: 'PingFang SC', 'Microsoft YaHei', sans-serif;"
        "}"
        "QLabel#titleLabel {"
        "  color: %7;"
        "  font-size: 24px;"
        "  font-weight: 600;"
        "}"
        "QLabel#subtitleLabel {"
        "  color: %4;"
        "  font-size: 13px;"
        "}"
        "QLabel#statusLabel {"
        "  font-size: 12px;"
        "  padding: 6px 12px;"
        "  border-radius: 6px;"
        "}"
        "QLabel#machineCodeLabel {"
        "  color: %10;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "  font-family: 'Menlo', 'Consolas', 'Courier New', monospace;"
        "  letter-spacing: 1px;"
        "  background-color: %9;"
        "  padding: 10px 16px;"
        "  border-radius: 8px;"
        "  border: 1px solid %5;"
        "}"
        "QLineEdit {"
        "  background-color: %6;"
        "  color: %3;"
        "  border: 1.5px solid %5;"
        "  border-radius: 8px;"
        "  padding: 12px 14px;"
        "  font-size: 14px;"
        "  font-family: 'Menlo', 'Consolas', 'Courier New', monospace;"
        "  selection-background-color: %7;"
        "}"
        "QLineEdit:focus {"
        "  border-color: %7;"
        "}"
        "QLineEdit::placeholder {"
        "  color: %4;"
        "}"
        "QPushButton {"
        "  border: none;"
        "  border-radius: 8px;"
        "  padding: 10px 20px;"
        "  font-size: 14px;"
        "  font-weight: 500;"
        "}"
        "QPushButton#activateBtn {"
        "  background-color: %7;"
        "  color: white;"
        "}"
        "QPushButton#activateBtn:hover {"
        "  background-color: %8;"
        "}"
        "QPushButton#activateBtn:disabled {"
        "  background-color: %5;"
        "  color: %4;"
        "}"
        "QPushButton#trialBtn {"
        "  background-color: %9;"
        "  color: %7;"
        "  border: 1px solid %7;"
        "}"
        "QPushButton#trialBtn:hover {"
        "  background-color: rgba(37, 99, 235, 0.2);"
        "}"
        "QPushButton#trialBtn:disabled {"
        "  background-color: %6;"
        "  color: %4;"
        "  border-color: %5;"
        "}"
        "QPushButton#copyBtn {"
        "  background-color: %6;"
        "  color: %4;"
        "  border: 1px solid %5;"
        "  padding: 8px 16px;"
        "  font-size: 13px;"
        "}"
        "QPushButton#copyBtn:hover {"
        "  background-color: %5;"
        "  color: %3;"
        "}"
        "QPushButton#buyBtn {"
        "  background-color: transparent;"
        "  color: %7;"
        "  text-decoration: underline;"
        "  font-size: 12px;"
        "}"
        "QPushButton#buyBtn:hover {"
        "  color: %8;"
        "}"
    ).arg(BG_DARK, TEXT_PRIMARY, TEXT_PRIMARY, TEXT_SECONDARY,
          BORDER_COLOR, BG_INPUT, PRIMARY_COLOR, PRIMARY_HOVER, BG_CARD, TEXT_LIGHT));

    ui->iconWidget->setStyleSheet(QStringLiteral(
        "QWidget#iconWidget {"
        "  background-color: %1;"
        "  border-radius: 16px;"
        "}"
    ).arg(BG_CARD));
}

void LoginDialog::updateStatusDisplay()
{
    QString machineCode = LicenseManager::generateMachineCode();
    qDebug() << "Machine code:" << machineCode;
    ui->machineCodeLabel->setText(machineCode);

    if (m_licenseManager->currentLicense().isValid) {
        ui->statusLabel->setText(QStringLiteral("已授权"));
        ui->statusLabel->setStyleSheet(QStringLiteral(
            "color: %1; background-color: rgba(16, 185, 129, 0.15);"
        ).arg(SUCCESS_COLOR));
        ui->activateBtn->setText(QStringLiteral("已激活"));
        ui->activateBtn->setEnabled(false);
    } else if (m_licenseManager->isTrialValid()) {
        int days = m_licenseManager->trialDaysLeft();
        ui->statusLabel->setText(QStringLiteral("试用期剩余 %1 天").arg(days));
        ui->statusLabel->setStyleSheet(QStringLiteral(
            "color: %1; background-color: rgba(245, 158, 11, 0.15);"
        ).arg(WARNING_COLOR));
        ui->activateBtn->setText(QStringLiteral("立即激活"));
        ui->activateBtn->setEnabled(true);
    } else {
        ui->statusLabel->setText(QStringLiteral("未授权"));
        ui->statusLabel->setStyleSheet(QStringLiteral(
            "color: %1; background-color: rgba(239, 68, 68, 0.15);"
        ).arg(DANGER_COLOR));
        ui->activateBtn->setText(QStringLiteral("立即激活"));
        ui->activateBtn->setEnabled(false);
    }
}

void LoginDialog::updateTrialButtonState()
{
    if (m_licenseManager->isTrialValid()) {
        ui->trialBtn->setText(QStringLiteral("继续试用"));
        ui->trialBtn->setEnabled(true);
    } else if (m_licenseManager->currentLicense().isValid) {
        ui->trialBtn->setText(QStringLiteral("已激活"));
        ui->trialBtn->setEnabled(false);
    } else {
        ui->trialBtn->setText(QStringLiteral("免费试用 7 天"));
        ui->trialBtn->setEnabled(true);
    }
}

void LoginDialog::onActivateClicked()
{
    QString key = ui->licenseInput->text().trimmed();
    if (key.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"),
                             QStringLiteral("请输入授权码"));
        return;
    }

    setButtonLoading(true);

    if (m_licenseManager->validateLicense(key)) {
        m_authorized = true;
        updateStatusDisplay();
        updateTrialButtonState();
        QMessageBox::information(this, QStringLiteral("激活成功"),
                                 QStringLiteral("授权码验证通过，感谢您的使用！"));
        accept();
    } else {
        QMessageBox::critical(this, QStringLiteral("激活失败"),
                              QStringLiteral("授权码无效或与本机不匹配，请检查后重试。\n\n"
                                             "如需购买授权，请联系客服提供机器码：\n%1")
                              .arg(ui->machineCodeLabel->text()));
    }

    setButtonLoading(false);
}

void LoginDialog::onTrialClicked()
{
    if (m_licenseManager->isTrialValid()) {
        m_authorized = true;
        int days = m_licenseManager->trialDaysLeft();
        QMessageBox::information(this, QStringLiteral("欢迎试用"),
                                 QStringLiteral("您的试用期还剩 %1 天，祝您使用愉快！").arg(days));
        accept();
    } else {
        QMessageBox::warning(this, QStringLiteral("试用已结束"),
                             QStringLiteral("您的试用期已结束，请购买授权码继续使用。\n\n"
                                            "请复制机器码联系客服购买授权。"));
    }
}

void LoginDialog::onCopyMachineCodeClicked()
{
    QString code = ui->machineCodeLabel->text();
    QGuiApplication::clipboard()->setText(code);

    ui->copyBtn->setText(QStringLiteral("已复制"));
    QTimer::singleShot(1500, this, [this]() {
        ui->copyBtn->setText(QStringLiteral("复制"));
    });
}

void LoginDialog::onBuyLicenseClicked()
{
    QString machineCode = ui->machineCodeLabel->text();
    QString message = QStringLiteral(
        "请复制下方机器码，联系客服购买授权：\n\n"
        "%1\n\n"
        "联系方式：\n"
        "微信：photohz-cn\n"
        "QQ：1669282017\n\n"
        "购买后您将收到授权码，在此输入即可激活。"
    ).arg(machineCode);
    QMessageBox::information(this, QStringLiteral("购买授权"), message);
}

void LoginDialog::onTextChanged()
{
    QString text = ui->licenseInput->text().trimmed();
    bool hasText = !text.isEmpty();
    bool canActivate = !m_licenseManager->currentLicense().isValid;
    ui->activateBtn->setEnabled(hasText && canActivate);
}

void LoginDialog::setButtonLoading(bool loading)
{
    ui->activateBtn->setEnabled(!loading);
    if (loading) {
        ui->activateBtn->setText(QStringLiteral("验证中..."));
    } else {
        onTextChanged();
    }
}
