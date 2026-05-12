#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include "models.h"

#include <QDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QTimer>

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

private slots:
    void on_btnLogin_clicked();
    void on_btnCancel_clicked();

private:
    void init();

    Ui::LoginDialog *ui;
    UserInfo m_userInfo;
};

#endif // LOGINDIALOG_H
