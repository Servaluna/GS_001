#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileInfo>
#include <QStatusBar>
#include <QCloseEvent>
#include <QMessageBox>
#include <qtablewidget>

#include "models.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    explicit MainWindow(QString token, const UserInfo& userInfo,QWidget *parent = nullptr);
    ~MainWindow();

    void updateTaskList(const QList<TaskBasicInfo> &tasks);

protected:
    void closeEvent(QCloseEvent *event) override;    //重写关闭事件以进行退出确认

private slots:
    void onTaskItemClicked(QTableWidgetItem *item);     //任务列表点击事件

    void on_btnExecute_clicked();
    void on_btnObtain_clicked();
    void on_btnLogs_clicked();

    void on_btnConnectToDevices_clicked();
    void on_btnLogout_clicked();

signals:
    void openDeviceConnector();
    void logoutFromMainWindow();

private:
    void initUI();
    void initButtonsByRole();

    void initTableTask();
    void addTaskToTable(const TaskBasicInfo &task, int row);
    QString getStatusText(TaskStatus::Status status);
    QColor getStatusColor(TaskStatus::Status status);

private:
    Ui::MainWindow *ui;
    QString m_token;
    UserInfo m_userInfo;
};

#endif // MAINWINDOW_H
