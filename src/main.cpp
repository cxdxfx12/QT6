#include <QApplication>
#include <QIcon>
#include <QFile>
#include <QTextStream>
#include "MainWindow.h"
#include "Utils/Logger.h"
#include "Utils/ConfigManager.h"
#include "Auth/LoginDialog.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 应用信息
    QApplication::setApplicationName("悟空运费结算");
    QApplication::setApplicationVersion("1.0.0");
    QApplication::setOrganizationName("杭州喵喵至家网络有限公司");

    // 设置应用程序图标
    QApplication::setWindowIcon(QIcon(":/app_icon.png"));

    // 加载全局样式表
    QFile styleFile(":/style.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QTextStream stream(&styleFile);
        app.setStyleSheet(stream.readAll());
        styleFile.close();
    }

    // 初始化日志
    Logger::instance().setLogFile("freight_calculator.log");
    Logger::instance().info("Application started");

    // 显示授权登录对话框
    LoginDialog loginDlg;
    if (loginDlg.exec() != QDialog::Accepted || !loginDlg.isAuthorized()) {
        Logger::instance().info("Authorization failed or cancelled");
        return 0;
    }

    Logger::instance().info("Authorization passed");

    // 创建主窗口
    MainWindow window;
    window.setWindowTitle("悟空运费结算");
    window.resize(1280, 800);
    window.show();

    int ret = app.exec();

    Logger::instance().info("Application exited");
    return ret;
}
