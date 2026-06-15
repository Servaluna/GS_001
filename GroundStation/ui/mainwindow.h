#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "models.h"
#include "../core/models/aircrafttask.h"

#include <QCloseEvent>
#include <QCheckBox>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QTableWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TaskService;
class QSpacerItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    explicit MainWindow(QString token, const UserInfo& userInfo, TaskService* taskService, QWidget *parent = nullptr);
    ~MainWindow();

    void updateTaskList(const QList<AircraftTask>& tasks);
    void showExecutePageAndReload();
    void setNetworkConnectionStatus(bool connected);
    void setAircraftConnectionStatus(bool connected);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void on_btnAlwaysOnTop_toggled(bool checked);

    void on_btnExecute_clicked();
    void on_btnObtain_clicked();
    void on_btnLogs_clicked();

    void on_btnLogout_clicked();

signals:
    void logoutFromMainWindow();

private:
    void initUI();
    void initButtonsByRole();
    void loadExecutableTasks();
    void showTaskStartControls();
    void showTaskProgressControls();
    void startSelectedAircraftTask();

    void initTableTask();
    void addTaskToTable(const AircraftTask& task, int row);
    QCheckBox* taskCheckBoxAt(int row) const;
    void onTaskCheckChanged(QCheckBox* source, bool checked);
    QString selectedAircraftTaskId() const;

private:
    Ui::MainWindow *ui;
    QString m_token;
    UserInfo m_userInfo;
    QPointer<TaskService> m_taskService;
    QPointer<QPushButton> m_btnStartAircraftTask;
    QSpacerItem* m_taskStartSpacer = nullptr;
};

#endif // MAINWINDOW_H
