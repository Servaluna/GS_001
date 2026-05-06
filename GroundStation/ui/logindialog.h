#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "models.h"

#include <QDialog>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include <QLineEdit>

namespace Ui {
class LoginDialog;
}

class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(QWidget *parent = nullptr);
    ~LoginDialog();

    UserInfo getUserInfo() const { return m_userInfo; }

signals:
    // void loginSuccess(const UserInfo& userInfo);

private slots:
    // 添加槽函数声明
    void on_btnLogin_clicked();
    void on_btnCancel_clicked();

private:
    Ui::LoginDialog *ui;
    UserInfo m_userInfo;

    void init();
};

#endif // LOGINDIALOG_H
