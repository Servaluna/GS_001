#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "models.h"
#include "../core/models/aircrafttask.h"

#include <QCloseEvent>
#include <QColor>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QTableWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TaskService;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    explicit MainWindow(QString token, const UserInfo& userInfo, TaskService* taskService, QWidget *parent = nullptr);
    ~MainWindow();

    void updateTaskList(const QList<AircraftTask>& tasks);
    void showExecutePageAndReload();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onTaskItemClicked(QTableWidgetItem *item);

    void on_btnExecute_clicked();
    void on_btnObtain_clicked();
    void on_btnLogs_clicked();
    void on_btnStartAircraftTask_clicked();

    void on_btnConnectToDevices_clicked();
    void on_btnLogout_clicked();

signals:
    void openDeviceConnector();
    void logoutFromMainWindow();

private:
    void initUI();
    void initButtonsByRole();
    void loadExecutableTasks();

    void initTableTask();
    void addTaskToTable(const AircraftTask& task, int row);
    QString getStatusText(DeviceTaskStatus status);
    QColor getStatusColor(DeviceTaskStatus status);
    QString selectedAircraftTaskId() const;

private:
    Ui::MainWindow *ui;
    QString m_token;
    UserInfo m_userInfo;
    QPointer<TaskService> m_taskService;
};

#endif // MAINWINDOW_H
