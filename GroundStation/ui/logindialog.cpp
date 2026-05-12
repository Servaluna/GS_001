#include "logindialog.h"
#include "ui_logindialog.h"

#include "../core/logging/logger.h"
#include "../core/network/serverconnector.h"

LoginDialog::LoginDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::LoginDialog)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    init();
}

LoginDialog::~LoginDialog()
{
    delete ui;
}

void LoginDialog::init()
{
    setWindowTitle("用户登录");
    ui->btnLogin->setDefault(true);
    ui->editUsername->setFocus();
    ui->editPassword->setEchoMode(QLineEdit::Password);

    connect(&ServerConnector::instance(), &ServerConnector::errorOccurred, this, [this](const QString& msg) {
        Logger::warn("AUTH_LOGIN_ERROR", msg);
        QMessageBox::critical(this, "错误", msg);
        ui->btnLogin->setEnabled(true);
        ui->btnLogin->setText("登录");
    });
}

void LoginDialog::on_btnLogin_clicked()
{
    const QString username = ui->editUsername->text().trimmed();
    const QString password = ui->editPassword->text();

    if (username.isEmpty() || password.isEmpty()) {
        Logger::warn("AUTH_LOGIN_INPUT_INVALID", "登录输入为空");
        QMessageBox::warning(this, "提示", "请输入用户名和密码。");
        return;
    }

    Logger::info("AUTH_LOGIN_SUBMIT", "用户提交登录", {{"username", username}});
    ServerConnector::instance().loginRequest(username, password);

    ui->btnLogin->setEnabled(false);
    ui->btnLogin->setText("登录中...");
}

void LoginDialog::on_btnCancel_clicked()
{
    Logger::info("AUTH_LOGIN_CANCELLED", "用户取消登录");
    reject();
}
