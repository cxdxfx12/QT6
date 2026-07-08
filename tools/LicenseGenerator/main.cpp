#include <QApplication>
#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QDateEdit>
#include <QCheckBox>
#include <QMessageBox>
#include <QClipboard>
#include <QGuiApplication>
#include <QDate>
#include <QCryptographicHash>
#include <QTimer>
#include <QRegularExpression>

static const QByteArray APP_SECRET = QByteArrayLiteral("WukongFreight2026@v1.0");

QString formatMachineCode(const QString &input)
{
    QString clean = input.trimmed().toUpper();
    clean.remove(QRegularExpression(QStringLiteral("[^A-Z0-9]")));
    if (clean.length() < 16) return QString();
    clean = clean.left(16);
    QString result;
    for (int i = 0; i < 16; ++i) {
        if (i > 0 && i % 4 == 0) result += QLatin1Char('-');
        result += clean[i];
    }
    return result;
}

QString generateLicenseKey(const QString &machineCode, const QString &expireDate)
{
    QString code = formatMachineCode(machineCode);
    if (code.isEmpty()) return QString();

    QString dateCode = expireDate;
    if (dateCode.toLower() == QStringLiteral("life")) {
        dateCode = QStringLiteral("LIFE");
    } else {
        QDate date = QDate::fromString(dateCode, QStringLiteral("yyyy-MM-dd"));
        if (!date.isValid()) return QString();
        dateCode = date.toString(QStringLiteral("yyMMdd"));
    }

    QString signData = QStringLiteral("%1-%2").arg(code, dateCode);
    QByteArray hash = QCryptographicHash::hash(
        (signData + APP_SECRET).toUtf8(), QCryptographicHash::Sha256
    ).toHex().toUpper();
    QString sign = hash.left(8);

    return QStringLiteral("WKF-%1-%2-%3")
        .arg(code, dateCode, sign);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("授权码生成器"));

    QWidget w;
    w.setWindowTitle(QStringLiteral("授权码生成器"));
    w.resize(460, 380);

    QVBoxLayout *layout = new QVBoxLayout(&w);
    layout->setSpacing(12);
    layout->setContentsMargins(30, 30, 30, 30);

    QLabel *title = new QLabel(QStringLiteral("悟空运费结算 - 授权码生成器"));
    title->setStyleSheet(QStringLiteral("font-size: 18px; font-weight: bold; color: #2563EB;"));
    title->setAlignment(Qt::AlignCenter);
    layout->addWidget(title);

    layout->addSpacing(10);

    layout->addWidget(new QLabel(QStringLiteral("机器码：")));
    QLineEdit *machineEdit = new QLineEdit();
    machineEdit->setPlaceholderText(QStringLiteral("请输入或粘贴机器码"));
    layout->addWidget(machineEdit);

    layout->addWidget(new QLabel(QStringLiteral("过期日期：")));
    QHBoxLayout *dateRow = new QHBoxLayout();
    QDateEdit *dateEdit = new QDateEdit(QDate::currentDate().addYears(1));
    dateEdit->setCalendarPopup(true);
    dateEdit->setDisplayFormat(QStringLiteral("yyyy-MM-dd"));
    dateRow->addWidget(dateEdit, 1);
    QCheckBox *lifeCheck = new QCheckBox(QStringLiteral("永久授权"));
    dateRow->addWidget(lifeCheck);
    layout->addLayout(dateRow);

    layout->addSpacing(10);

    QPushButton *genBtn = new QPushButton(QStringLiteral("生成授权码"));
    genBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #2563EB; color: white; padding: 12px; font-size: 15px; font-weight: bold; border-radius: 8px; }"
        "QPushButton:hover { background-color: #1D4ED8; }"
    ));
    layout->addWidget(genBtn);

    layout->addSpacing(10);

    QLabel *resultTitle = new QLabel(QStringLiteral("生成结果："));
    resultTitle->setVisible(false);
    layout->addWidget(resultTitle);

    QLineEdit *resultEdit = new QLineEdit();
    resultEdit->setReadOnly(true);
    resultEdit->setVisible(false);
    resultEdit->setStyleSheet(QStringLiteral(
        "QLineEdit { background-color: #F1F5F9; color: #0F172A; padding: 10px; font-family: monospace; font-size: 14px; border: 1px solid #2563EB; border-radius: 6px; }"
    ));
    layout->addWidget(resultEdit);

    QPushButton *copyBtn = new QPushButton(QStringLiteral("复制"));
    copyBtn->setVisible(false);
    layout->addWidget(copyBtn);

    layout->addStretch();

    QObject::connect(lifeCheck, &QCheckBox::toggled, dateEdit, &QDateEdit::setDisabled);

    QObject::connect(genBtn, &QPushButton::clicked, [&]() {
        QString raw = machineEdit->text().trimmed();
        if (raw.isEmpty()) {
            QMessageBox::warning(&w, QStringLiteral("提示"), QStringLiteral("请输入机器码"));
            return;
        }

        QString code = formatMachineCode(raw);
        if (code.isEmpty()) {
            QMessageBox::warning(&w, QStringLiteral("提示"), QStringLiteral("机器码格式不对，至少需要16位字母数字"));
            return;
        }

        QString expireDate;
        if (lifeCheck->isChecked()) {
            expireDate = QStringLiteral("LIFE");
        } else {
            expireDate = dateEdit->date().toString(QStringLiteral("yyyy-MM-dd"));
        }

        QString key = generateLicenseKey(code, expireDate);
        if (key.isEmpty()) {
            QMessageBox::critical(&w, QStringLiteral("错误"), QStringLiteral("生成失败"));
            return;
        }

        machineEdit->setText(code);
        resultEdit->setText(key);
        resultTitle->setVisible(true);
        resultEdit->setVisible(true);
        copyBtn->setVisible(true);
    });

    QObject::connect(copyBtn, &QPushButton::clicked, [&]() {
        QGuiApplication::clipboard()->setText(resultEdit->text());
        copyBtn->setText(QStringLiteral("已复制"));
        QTimer::singleShot(1500, copyBtn, [copyBtn]() {
            copyBtn->setText(QStringLiteral("复制"));
        });
    });

    w.show();
    return app.exec();
}
