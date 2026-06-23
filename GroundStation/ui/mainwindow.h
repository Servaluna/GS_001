#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "models.h"
#include "../core/logging/logger.h"
#include "../core/models/aircrafttask.h"

#include <QCloseEvent>
#include <QCheckBox>
#include <QMainWindow>
#include <QMessageBox>
#include <QPointer>
#include <QTableWidget>
#include <QTableWidgetItem>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class TaskService;
class QLabel;

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
    void setAircraftConnectionStatus(bool connected, const QString& aircraftCode = QString());

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
    void resetTaskProgressPanel();
    void onTaskStarted(const QString& taskId, const QString& taskName);
    void onTaskProgressUpdated(const QString& taskId, const QString& step, int progress, qint64 speed);
    void onTaskFinished(const QString& taskId, bool success, const QString& message);
    void startSelectedAircraftTask();
    void initLogsPage();
    void appendLogEntryToTable(const LogEntry& entry);

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
    bool m_aircraftConnected = false;
    QString m_connectedAircraftCode;
    QString m_activeAircraftTaskId;
    QPointer<QTableWidget> m_tblLogs;
    QPointer<QLabel> m_lblLogSummary;
};

#endif // MAINWINDOW_H
