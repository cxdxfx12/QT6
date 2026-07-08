#ifndef LICENSEMANAGER_H
#define LICENSEMANAGER_H

#include <QString>
#include <QDate>
#include <QCryptographicHash>

// 许可证信息结构
struct LicenseInfo {
    bool isValid = false;
    bool isTrial = false;
    QString machineCode;
    QString licenseKey;
    QDate expireDate;
    QString activateDate;
    QString hardwareFingerprint;
};

// 机器码和许可证管理器
class LicenseManager {
public:
    LicenseManager();

    // 生成当前机器的机器码（基于硬件ID，不含电脑名）
    static QString generateMachineCode();

    // 验证许可证是否有效
    bool validateLicense(const QString &licenseKey);

    // 从本地文件加载许可证
    bool loadLicenseFromFile();

    // 保存许可证到本地文件
    bool saveLicenseToFile(const QString &licenseKey);

    // 获取当前许可证信息
    LicenseInfo currentLicense() const { return m_license; }

    // 检查是否已授权（含试用）
    bool isAuthorized() const;

    // 检查是否在试用期内
    bool isTrialValid() const;

    // 获取剩余试用天数
    int trialDaysLeft() const;

    // 获取授权状态描述
    QString statusText() const;

    // 清除许可证（用于注销）
    bool clearLicense();

private:
    LicenseInfo m_license;

    // 计算硬件指纹（综合多个硬件ID）
    static QString computeHardwareFingerprint();

    // 获取MAC地址
    static QString getMacAddress();

    // 获取CPU信息
    static QString getCpuId();

    // 获取系统卷UUID
    static QString getVolumeUuid();

    // 获取主板序列号
    static QString getBoardSerial();

    // 验证许可证密钥格式和签名
    bool verifyLicenseKey(const QString &licenseKey, const QString &machineCode);

    // 解析许可证密钥
    bool parseLicenseKey(const QString &licenseKey, LicenseInfo &info);

    // 获取许可证文件路径
    static QString licenseFilePath();

    // 初始化试用（如果没有许可证且没有试用记录）
    void initTrialIfNeeded();
};

#endif // LICENSEMANAGER_H
