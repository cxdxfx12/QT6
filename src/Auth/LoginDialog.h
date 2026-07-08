#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QPointer>
#include <memory>

class LicenseManager;
QT_BEGIN_NAMESPACE
namespace Ui { class LoginDialog; }
QT_END_NAMESPACE

class LoginDialog : public QDialog {
    Q_OBJECT
public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    bool isAuthorized() const;

private slots:
    void onActivateClicked();
    void onTrialClicked();
    void onCopyMachineCodeClicked();
    void onBuyLicenseClicked();
    void onTextChanged();

private:
    void setupStyle();
    void updateStatusDisplay();
    void setButtonLoading(bool loading);
    void updateTrialButtonState();

    Ui::LoginDialog *ui;
    std::unique_ptr<LicenseManager> m_licenseManager;
    bool m_authorized = false;
};

#endif // LOGINDIALOG_H
