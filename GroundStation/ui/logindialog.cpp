#include "logindialog.h"
#include "ui_logindialog.h"

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
        QMessageBox::critical(this, "错误", msg);
        ui->btnLogin->setEnabled(true);
        ui->btnLogin->setText("登录");
    });
}

void LoginDialog::on_btnLogin_clicked()
{
    QString username = ui->editUsername->text().trimmed();
    QString password = ui->editPassword->text();

    if (username.isEmpty() || password.isEmpty()) {
        QMessageBox::warning(this, "提示", "请输入用户名和密码");
        return;
    }

    ServerConnector::instance().loginRequest(username, password);
    DEBUG_LOCATION << "发送登录请求" << username;

    ui->btnLogin->setEnabled(false);
    ui->btnLogin->setText("登录中...");
}

void LoginDialog::on_btnCancel_clicked()
{
    reject();
}
