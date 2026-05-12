#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../core/logging/logger.h"
#include "../core/services/taskservice.h"

#include <QHeaderView>
#include <QStatusBar>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initUI();
}

MainWindow::MainWindow(QString token, const UserInfo& userInfo, TaskService* taskService, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_token(token)
    , m_userInfo(userInfo)
    , m_taskService(taskService)
{
    ui->setupUi(this);
    initUI();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::initUI()
{
    QString title = QString("地面站系统 - [%1]").arg(m_userInfo.role);
    setWindowTitle(title);

    QString welcome = QString("欢迎用户: %1 (%2)").arg(m_userInfo.username, m_userInfo.role);
    statusBar()->showMessage(welcome, 5000);

    initButtonsByRole();
    initTableTask();

    ui->stackedWidget->setCurrentIndex(0);

    connect(ui->tableTask, &QTableWidget::itemClicked,
            this, &MainWindow::onTaskItemClicked);

    loadExecutableTasks();
}

void MainWindow::initButtonsByRole()
{
    switch (UserRole::roleFromString(m_userInfo.role)) {
    case UserRole::Admin:
        ui->btnExecute->setVisible(true);
        ui->btnObtain->setVisible(true);
        ui->btnLogs->setVisible(true);
        break;
    case UserRole::Engineer:
        ui->btnExecute->setVisible(true);
        ui->btnObtain->setVisible(true);
        ui->btnLogs->setVisible(false);
        break;
    case UserRole::Operator:
        ui->btnExecute->setVisible(true);
        ui->btnObtain->setVisible(false);
        ui->btnLogs->setVisible(false);
        break;
    default:
        break;
    }
}

void MainWindow::loadExecutableTasks()
{
    if (!m_taskService) {
        updateTaskList({});
        return;
    }

    updateTaskList(m_taskService->getExecutableAircraftTasksForUser(m_userInfo.user_id, m_userInfo.role));
}

void MainWindow::initTableTask()
{
    QStringList headers = {"选择", "飞机任务ID", "批次ID", "飞机编号", "状态",
                           "进度", "当前阶段", "开始时间", "完成时间", "错误信息"};
    ui->tableTask->setColumnCount(headers.size());
    ui->tableTask->setHorizontalHeaderLabels(headers);

    ui->tableTask->setColumnWidth(0, 50);
    ui->tableTask->setColumnWidth(1, 100);
    ui->tableTask->setColumnWidth(2, 80);
    ui->tableTask->setColumnWidth(3, 120);
    ui->tableTask->setColumnWidth(4, 80);
    ui->tableTask->setColumnWidth(5, 80);
    ui->tableTask->setColumnWidth(6, 140);
    ui->tableTask->setColumnWidth(7, 150);
    ui->tableTask->setColumnWidth(8, 150);
    ui->tableTask->setColumnWidth(9, 180);

    ui->tableTask->horizontalHeader()->setStretchLastSection(true);
    ui->tableTask->setAlternatingRowColors(true);
    ui->tableTask->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableTask->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableTask->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableTask->verticalHeader()->setDefaultSectionSize(30);
}

void MainWindow::updateTaskList(const QList<AircraftTask>& tasks)
{
    ui->tableTask->setRowCount(tasks.size());
    for (int i = 0; i < tasks.size(); ++i) {
        addTaskToTable(tasks[i], i);
    }
}

void MainWindow::showExecutePageAndReload()
{
    ui->stackedWidget->setCurrentIndex(0);
    loadExecutableTasks();
}

void MainWindow::addTaskToTable(const AircraftTask& task, int row)
{
    QTableWidgetItem *checkItem = new QTableWidgetItem();
    checkItem->setCheckState(Qt::Unchecked);
    checkItem->setData(Qt::UserRole, task.aircraft_task_id);
    ui->tableTask->setItem(row, 0, checkItem);

    ui->tableTask->setItem(row, 1, new QTableWidgetItem(QString::number(task.aircraft_task_id)));
    ui->tableTask->setItem(row, 2, new QTableWidgetItem(QString::number(task.batch_id)));
    ui->tableTask->setItem(row, 3, new QTableWidgetItem(task.aircraft_code));

    QTableWidgetItem *statusItem = new QTableWidgetItem(getStatusText(task.status));
    statusItem->setForeground(QBrush(getStatusColor(task.status)));
    statusItem->setFont(QFont("", -1, QFont::Bold));
    ui->tableTask->setItem(row, 4, statusItem);

    ui->tableTask->setItem(row, 5, new QTableWidgetItem(QString("%1%").arg(task.progress, 0, 'f', 1)));
    ui->tableTask->setItem(row, 6, new QTableWidgetItem(task.current_phase));
    ui->tableTask->setItem(row, 7, new QTableWidgetItem(task.start_time.isValid() ? task.start_time.toString("yyyy-MM-dd hh:mm:ss") : "-"));
    ui->tableTask->setItem(row, 8, new QTableWidgetItem(task.finish_time.isValid() ? task.finish_time.toString("yyyy-MM-dd hh:mm:ss") : "-"));
    ui->tableTask->setItem(row, 9, new QTableWidgetItem(task.last_error));
}

QString MainWindow::getStatusText(DeviceTaskStatus status)
{
    return TaskStatusText::deviceDisplayName(status);
}

QColor MainWindow::getStatusColor(DeviceTaskStatus status)
{
    switch (status) {
    case DeviceTaskStatus::Waiting: return QColor(255, 165, 0);
    case DeviceTaskStatus::Downloading: return QColor(0, 120, 215);
    case DeviceTaskStatus::Transferring: return QColor(0, 120, 215);
    case DeviceTaskStatus::Installing: return QColor(128, 90, 213);
    case DeviceTaskStatus::Verifying: return QColor(0, 150, 160);
    case DeviceTaskStatus::Success: return QColor(0, 150, 0);
    case DeviceTaskStatus::Failed: return QColor(220, 20, 60);
    default: return Qt::black;
    }
}

QString MainWindow::selectedAircraftTaskId() const
{
    const QList<QTableWidgetItem*> selectedItems = ui->tableTask->selectedItems();
    if (selectedItems.isEmpty()) {
        return QString();
    }

    const int row = selectedItems.first()->row();
    QTableWidgetItem* idItem = ui->tableTask->item(row, 1);
    return idItem ? idItem->text() : QString();
}

void MainWindow::onTaskItemClicked(QTableWidgetItem *item)
{
    if (!item) {
        return;
    }

    const int row = item->row();
    const QString taskId = ui->tableTask->item(row, 1)->text();
    LogContext context;
    context.operator_user_id = m_userInfo.user_id;
    context.aircraft_task_id = taskId.toInt();
    Logger::debug("TASK_SELECTED", QString("选中飞机升级任务 %1").arg(taskId), context);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_userInfo.username.isEmpty()) {
        int ret = QMessageBox::question(this, "退出确认",
                                        QString("用户 %1 正在使用程序，是否退出账户并关闭程序？").arg(m_userInfo.username),
                                        QMessageBox::Yes | QMessageBox::No);
        if (ret == QMessageBox::No) {
            event->ignore();
            return;
        }
    }

    event->accept();
    Logger::info("APP_EXIT",
                 QString("用户 %1 退出程序").arg(m_userInfo.username),
                 {{"username", m_userInfo.username}, {"role", m_userInfo.role}});
}

void MainWindow::on_btnExecute_clicked()
{
    showExecutePageAndReload();
}

void MainWindow::on_btnObtain_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}

void MainWindow::on_btnLogs_clicked()
{
    ui->stackedWidget->setCurrentIndex(2);
}

void MainWindow::on_btnStartAircraftTask_clicked()
{
    const QString taskId = selectedAircraftTaskId();
    if (taskId.isEmpty()) {
        QMessageBox::information(this, "执行任务", "请先选择一个飞机升级任务。");
        return;
    }

    if (!m_taskService || !m_taskService->startTask(taskId)) {
        LogContext context;
        context.operator_user_id = m_userInfo.user_id;
        context.aircraft_task_id = taskId.toInt();
        Logger::warn("TASK_START_REJECTED", QString("启动飞机升级任务失败: %1").arg(taskId), context);
        QMessageBox::warning(this, "执行任务", "启动飞机升级任务失败，请检查任务状态和设备子任务。");
        return;
    }

    LogContext context;
    context.operator_user_id = m_userInfo.user_id;
    context.aircraft_task_id = taskId.toInt();
    Logger::info("TASK_START_SUBMITTED", QString("用户启动飞机升级任务: %1").arg(taskId), context);
    statusBar()->showMessage(QString("已启动飞机升级任务: %1").arg(taskId), 5000);
    loadExecutableTasks();
}

void MainWindow::on_btnConnectToDevices_clicked()
{
    emit openDeviceConnector();
}

void MainWindow::on_btnLogout_clicked()
{
    emit logoutFromMainWindow();
}
