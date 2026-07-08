#include "LicenseManager.h"
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkInterface>
#include <QDataStream>
#include <QSettings>
#include <QSysInfo>
#include <QProcess>

// 应用密钥（用于签名验证）
static const QByteArray APP_SECRET = QByteArrayLiteral("WukongFreight2026@v1.0");
static const int TRIAL_DAYS = 7;

LicenseManager::LicenseManager()
{
    loadLicenseFromFile();
}

QString LicenseManager::generateMachineCode()
{
    QString fingerprint = computeHardwareFingerprint();
    if (fingerprint.isEmpty()) {
        // 备用方案：基于随机种子生成固定ID
        fingerprint = QStringLiteral("WKFALLBACK");
    }

    // 取前16位作为机器码显示，但完整指纹用于验证
    QByteArray hash = QCryptographicHash::hash(
        fingerprint.toUtf8(), QCryptographicHash::Sha256
    ).toHex().toUpper();

    // 格式化为 XXXX-XXXX-XXXX-XXXX
    QString code;
    for (int i = 0; i < 16 && i < hash.length(); ++i) {
        if (i > 0 && i % 4 == 0) code += '-';
        code += hash[i];
    }
    return code;
}

QString LicenseManager::computeHardwareFingerprint()
{
    QStringList components;

    QString mac = getMacAddress();
    if (!mac.isEmpty()) components.append(mac);

    QString cpu = getCpuId();
    if (!cpu.isEmpty()) components.append(cpu);

    QString vol = getVolumeUuid();
    if (!vol.isEmpty()) components.append(vol);

    QString board = getBoardSerial();
    if (!board.isEmpty()) components.append(board);

    if (components.isEmpty()) {
        QString fallback = QStringLiteral("%1_%2_%3")
            .arg(QSysInfo::productType())
            .arg(QSysInfo::currentCpuArchitecture())
            .arg(QSysInfo::kernelVersion());
        components.append(fallback);
    }

    QByteArray data = components.join('|').toUtf8();
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
}

QString LicenseManager::getMacAddress()
{
    QString mac;
    foreach (const QNetworkInterface &iface, QNetworkInterface::allInterfaces()) {
        // 跳过虚拟网卡和回环接口
        if (iface.flags() & QNetworkInterface::IsLoopBack) continue;
        if (iface.name().contains(QStringLiteral("docker"), Qt::CaseInsensitive)) continue;
        if (iface.name().contains(QStringLiteral("vmnet"), Qt::CaseInsensitive)) continue;
        if (iface.name().contains(QStringLiteral("veth"), Qt::CaseInsensitive)) continue;

        QString hwAddr = iface.hardwareAddress();
        if (!hwAddr.isEmpty() && hwAddr != QStringLiteral("00:00:00:00:00:00")) {
            mac = hwAddr.remove(':');
            break;
        }
    }
    return mac;
}

QString LicenseManager::getCpuId()
{
#if defined(Q_OS_MAC)
    QProcess proc;
    proc.start(QStringLiteral("sysctl"), QStringList() << QStringLiteral("-n") << QStringLiteral("machdep.cpu.brand_string"));
    if (proc.waitForFinished(3000)) {
        QString cpu = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!cpu.isEmpty()) return cpu;
    }
    // 备用：获取CPU核心数+架构信息
    proc.start(QStringLiteral("sysctl"), QStringList() << QStringLiteral("-n") << QStringLiteral("hw.physicalcpu"));
    if (proc.waitForFinished(3000)) {
        QString cores = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        return QStringLiteral("MACCPU%1").arg(cores);
    }
#elif defined(Q_OS_WIN)
    QProcess proc;
    proc.start(QStringLiteral("wmic"), QStringList() << QStringLiteral("cpu") << QStringLiteral("get") << QStringLiteral("ProcessorId") << QStringLiteral("/value"));
    if (proc.waitForFinished(3000)) {
        QString output = QString::fromLocal8Bit(proc.readAllStandardOutput());
        QRegularExpression re(QStringLiteral("ProcessorId=(.+)$"));
        auto match = re.match(output);
        if (match.hasMatch()) return match.captured(1).trimmed();
    }
#elif defined(Q_OS_LINUX)
    QFile file(QStringLiteral("/proc/cpuinfo"));
    if (file.open(QIODevice::ReadOnly)) {
        QString content = QString::fromUtf8(file.readAll());
        QRegularExpression re(QStringLiteral("serial\\s*:\\s*(.+)$"), QRegularExpression::MultilineOption);
        auto match = re.match(content);
        if (match.hasMatch()) return match.captured(1).trimmed();

        // 备用：使用model name
        re.setPattern(QStringLiteral("model name\\s*:\\s*(.+)$"));
        match = re.match(content);
        if (match.hasMatch()) return match.captured(1).trimmed();
    }
#endif
    return QString();
}

QString LicenseManager::getVolumeUuid()
{
#if defined(Q_OS_MAC)
    QProcess proc;
    proc.start(QStringLiteral("diskutil"), QStringList() << QStringLiteral("info") << QStringLiteral("/"));
    if (proc.waitForFinished(3000)) {
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        QRegularExpression re(QStringLiteral("Volume UUID:\\s*(.+)$"), QRegularExpression::MultilineOption);
        auto match = re.match(output);
        if (match.hasMatch()) return match.captured(1).trimmed();
    }
#elif defined(Q_OS_WIN)
    QProcess proc;
    proc.start(QStringLiteral("wmic"), QStringList() << QStringLiteral("path") << QStringLiteral("win32_computersystemproduct") << QStringLiteral("get") << QStringLiteral("UUID") << QStringLiteral("/value"));
    if (proc.waitForFinished(3000)) {
        QString output = QString::fromLocal8Bit(proc.readAllStandardOutput());
        QRegularExpression re(QStringLiteral("UUID=(.+)$"));
        auto match = re.match(output);
        if (match.hasMatch()) {
            QString uuid = match.captured(1).trimmed();
            if (uuid != QStringLiteral("FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF"))
                return uuid;
        }
    }
#elif defined(Q_OS_LINUX)
    QProcess proc;
    proc.start(QStringLiteral("blkid"), QStringList() << QStringLiteral("-s") << QStringLiteral("UUID") << QStringLiteral("-o") << QStringLiteral("value") << QStringLiteral("/"));
    if (proc.waitForFinished(3000)) {
        QString uuid = QString::fromUtf8(proc.readAllStandardOutput()).trimmed();
        if (!uuid.isEmpty()) return uuid;
    }
#endif
    return QString();
}

QString LicenseManager::getBoardSerial()
{
#if defined(Q_OS_MAC)
    QProcess proc;
    proc.start(QStringLiteral("ioreg"), QStringList() << QStringLiteral("-l") << QStringLiteral("-p") << QStringLiteral("IODeviceTree"));
    if (proc.waitForFinished(3000)) {
        QString output = QString::fromUtf8(proc.readAllStandardOutput());
        QRegularExpression re(QStringLiteral("\"IOPlatformSerialNumber\"\\s*=\\s*\"([^\"]+)\""));
        auto match = re.match(output);
        if (match.hasMatch()) {
            QString serial = match.captured(1);
            if (serial != QStringLiteral("System Serial Number") &&
                serial != QStringLiteral("Not Available"))
                return serial;
        }
    }
#elif defined(Q_OS_WIN)
    QProcess proc;
    proc.start(QStringLiteral("wmic"), QStringList() << QStringLiteral("baseboard") << QStringLiteral("get") << QStringLiteral("SerialNumber") << QStringLiteral("/value"));
    if (proc.waitForFinished(3000)) {
        QString output = QString::fromLocal8Bit(proc.readAllStandardOutput());
        QRegularExpression re(QStringLiteral("SerialNumber=(.+)$"));
        auto match = re.match(output);
        if (match.hasMatch()) return match.captured(1).trimmed();
    }
#endif
    return QString();
}

bool LicenseManager::validateLicense(const QString &licenseKey)
{
    if (licenseKey.isEmpty()) return false;

    QString machineCode = generateMachineCode();

    if (verifyLicenseKey(licenseKey, machineCode)) {
        LicenseInfo info;
        if (parseLicenseKey(licenseKey, info)) {
            info.isValid = true;
            info.machineCode = machineCode;
            info.licenseKey = licenseKey;
            m_license = info;
            saveLicenseToFile(licenseKey);
            return true;
        }
    }
    return false;
}

bool LicenseManager::verifyLicenseKey(const QString &licenseKey, const QString &machineCode)
{
    // 格式: WKF-XXXX-XXXX-XXXX-XXXX-SIGN
    // XXXX-XXXX-XXXX-XXXX = 机器码
    // SIGN = HMAC-SHA256(机器码+过期日期)
    QStringList parts = licenseKey.split('-');
    if (parts.size() != 7) return false;
    if (parts[0] != QStringLiteral("WKF")) return false;

    // 提取机器码部分
    QString keyMachineCode = parts.mid(1, 4).join('-');
    if (keyMachineCode != machineCode) return false;

    // 验证签名（简化版：使用前8位作为校验）
    QString signData = parts.mid(1, 5).join('-');
    QByteArray expectedHash = QCryptographicHash::hash(
        (signData + APP_SECRET).toUtf8(), QCryptographicHash::Sha256
    ).toHex().toUpper();
    QString expectedSign = expectedHash.left(8);

    return parts[6] == expectedSign;
}

bool LicenseManager::parseLicenseKey(const QString &licenseKey, LicenseInfo &info)
{
    QStringList parts = licenseKey.split('-');
    if (parts.size() != 7) return false;

    // parts[5] 是过期日期编码 (YYMMDD)
    QString dateCode = parts[5];
    if (dateCode == QStringLiteral("LIFE")) {
        info.expireDate = QDate(2099, 12, 31);
    } else if (dateCode.length() == 6) {
        int year = dateCode.mid(0, 2).toInt() + 2000;
        int month = dateCode.mid(2, 2).toInt();
        int day = dateCode.mid(4, 2).toInt();
        info.expireDate = QDate(year, month, day);
    } else {
        return false;
    }

    info.activateDate = QDate::currentDate().toString(QStringLiteral("yyyy-MM-dd"));
    return true;
}

bool LicenseManager::loadLicenseFromFile()
{
    QString path = licenseFilePath();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        // 尝试初始化试用
        initTrialIfNeeded();
        return false;
    }

    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        initTrialIfNeeded();
        return false;
    }

    QJsonObject obj = doc.object();
    QString licenseKey = obj.value(QStringLiteral("licenseKey")).toString();
    QString storedFingerprint = obj.value(QStringLiteral("fingerprint")).toString();

    QString currentFingerprint = computeHardwareFingerprint();

    // 检查硬件是否变更
    if (!storedFingerprint.isEmpty() && storedFingerprint != currentFingerprint) {
        // 硬件变更，许可证失效（除非用户重新激活）
        m_license.isValid = false;
        return false;
    }

    if (!licenseKey.isEmpty()) {
        return validateLicense(licenseKey);
    }

    // 检查是否有试用记录
    QDate trialStart = QDate::fromString(
        obj.value(QStringLiteral("trialStart")).toString(),
        Qt::ISODate
    );
    if (trialStart.isValid()) {
        m_license.isTrial = true;
        m_license.expireDate = trialStart.addDays(TRIAL_DAYS);
        m_license.hardwareFingerprint = currentFingerprint;
    }

    initTrialIfNeeded();
    return false;
}

bool LicenseManager::saveLicenseToFile(const QString &licenseKey)
{
    QString path = licenseFilePath();
    QDir().mkpath(QFileInfo(path).path());

    QJsonObject obj;
    obj[QStringLiteral("licenseKey")] = licenseKey;
    obj[QStringLiteral("fingerprint")] = computeHardwareFingerprint();
    obj[QStringLiteral("activateDate")] = QDate::currentDate().toString(Qt::ISODate);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;

    file.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    return true;
}

void LicenseManager::initTrialIfNeeded()
{
    // 如果没有有效许可证且没有试用记录，初始化试用
    if (!m_license.isValid && m_license.expireDate.isNull()) {
        QString path = licenseFilePath();
        QFile file(path);

        QJsonObject obj;
        if (file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            if (doc.isObject()) obj = doc.object();
        }

        QDate trialStart = QDate::fromString(
            obj.value(QStringLiteral("trialStart")).toString(), Qt::ISODate
        );

        if (!trialStart.isValid()) {
            // 首次启动，初始化试用
            trialStart = QDate::currentDate();
            obj[QStringLiteral("trialStart")] = trialStart.toString(Qt::ISODate);
            obj[QStringLiteral("fingerprint")] = computeHardwareFingerprint();

            QDir().mkpath(QFileInfo(path).path());
            QFile out(path);
            if (out.open(QIODevice::WriteOnly)) {
                out.write(QJsonDocument(obj).toJson(QJsonDocument::Compact));
            }
        }

        m_license.isTrial = true;
        m_license.expireDate = trialStart.addDays(TRIAL_DAYS);
        m_license.hardwareFingerprint = computeHardwareFingerprint();
    }
}

bool LicenseManager::isAuthorized() const
{
    return m_license.isValid || isTrialValid();
}

bool LicenseManager::isTrialValid() const
{
    if (!m_license.isTrial) return false;
    return QDate::currentDate() <= m_license.expireDate;
}

int LicenseManager::trialDaysLeft() const
{
    if (!m_license.isTrial) return 0;
    return qMax(0, QDate::currentDate().daysTo(m_license.expireDate));
}

QString LicenseManager::statusText() const
{
    if (m_license.isValid) {
        if (m_license.expireDate.year() >= 2099) {
            return QStringLiteral("永久授权");
        }
        return QStringLiteral("授权有效期至: %1").arg(m_license.expireDate.toString(QStringLiteral("yyyy-MM-dd")));
    }
    if (isTrialValid()) {
        return QStringLiteral("试用期剩余 %1 天").arg(trialDaysLeft());
    }
    return QStringLiteral("未授权");
}

bool LicenseManager::clearLicense()
{
    m_license = LicenseInfo();
    QString path = licenseFilePath();
    QFile file(path);
    if (file.exists()) {
        file.remove();
    }
    initTrialIfNeeded();
    return true;
}

QString LicenseManager::licenseFilePath()
{
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (appDataDir.isEmpty()) appDataDir = QDir::homePath() + QStringLiteral("/.WukongFreight");
    return appDataDir + QStringLiteral("/license.dat");
}
