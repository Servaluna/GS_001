#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../core/logging/logger.h"
#include "../core/services/taskservice.h"

#include <QHeaderView>
#include <QHBoxLayout>
#include <QSizePolicy>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QVBoxLayout>

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

    ui->btnAlwaysOnTop->setCheckable(true);
    ui->btnAlwaysOnTop->setChecked(false);
    ui->btnAlwaysOnTop->setText("置顶");
    ui->btnAlwaysOnTop->setToolTip("让地面站窗口保持在屏幕最前面");

    initButtonsByRole();
    initTableTask();
    showTaskStartControls();

    ui->stackedWidget->setCurrentIndex(0);

    connect(ui->btnAlwaysOnTop, &QPushButton::toggled,
            this, &MainWindow::on_btnAlwaysOnTop_toggled);
    connect(m_btnStartAircraftTask, &QPushButton::clicked,
            this, &MainWindow::startSelectedAircraftTask);

    loadExecutableTasks();
}

void MainWindow::on_btnAlwaysOnTop_toggled(bool checked)
{
    const Qt::WindowFlags flags = checked
        ? (windowFlags() | Qt::WindowStaysOnTopHint)
        : (windowFlags() & ~Qt::WindowStaysOnTopHint);

    setWindowFlags(flags);
    ui->btnAlwaysOnTop->setText(checked ? "取消置顶" : "置顶");
    show();
    raise();
    activateWindow();
}

void MainWindow::initButtonsByRole()
{
    switch (UserRole::roleFromId(m_userInfo.role_id)) {
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
        Logger::warn("TASK_TABLE_LOAD_REJECTED",
                     "任务服务为空，无法加载任务表",
                     {{"user_id", m_userInfo.user_id}, {"role_id", m_userInfo.role_id}});
        updateTaskList({});
        return;
    }

    Logger::info("TASK_TABLE_LOAD_START",
                 "开始加载执行页面任务表",
                 {{"user_id", m_userInfo.user_id}, {"role_id", m_userInfo.role_id}, {"username", m_userInfo.username}});
    const QList<AircraftTask> tasks = m_taskService->getExecutableAircraftTasksForUser(m_userInfo.user_id, m_userInfo.role_id);
    Logger::info("TASK_TABLE_LOAD_FINISHED",
                 "执行页面任务表数据加载完成",
                 {{"user_id", m_userInfo.user_id}, {"role_id", m_userInfo.role_id}, {"aircraft_task_count", tasks.size()}});
    updateTaskList(tasks);
}

void MainWindow::showTaskStartControls()
{
    ui->label_7->hide();
    ui->label_8->hide();
    ui->progressBar->hide();

    if (!m_btnStartAircraftTask) {
        m_btnStartAircraftTask = new QPushButton("执行升级任务", ui->widget_8);
        m_btnStartAircraftTask->setObjectName("btnStartAircraftTaskRuntime");
        m_btnStartAircraftTask->setMinimumWidth(120);
        ui->horizontalLayout_2->addWidget(m_btnStartAircraftTask);
    }

    if (!m_taskStartSpacer) {
        m_taskStartSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        ui->horizontalLayout_2->insertSpacerItem(0, m_taskStartSpacer);
    }

    m_btnStartAircraftTask->show();
}

void MainWindow::showTaskProgressControls()
{
    if (m_btnStartAircraftTask) {
        m_btnStartAircraftTask->hide();
    }
    if (m_taskStartSpacer) {
        ui->horizontalLayout_2->removeItem(m_taskStartSpacer);
        delete m_taskStartSpacer;
        m_taskStartSpacer = nullptr;
    }
    ui->label_7->show();
    ui->label_8->show();
    ui->progressBar->show();
}

void MainWindow::initTableTask()
{
    QStringList headers = {"选择", "飞机任务ID", "批次ID", "飞机编号", "状态",
                           "进度", "当前阶段"};
    ui->tableTask->setColumnCount(headers.size());
    ui->tableTask->setHorizontalHeaderLabels(headers);

    ui->tableTask->setColumnWidth(0, 50);
    ui->tableTask->setColumnWidth(1, 100);
    ui->tableTask->setColumnWidth(2, 80);
    ui->tableTask->setColumnWidth(3, 120);
    ui->tableTask->setColumnWidth(4, 80);
    ui->tableTask->setColumnWidth(5, 80);
    ui->tableTask->setColumnWidth(6, 140);

    ui->tableTask->horizontalHeader()->setStretchLastSection(true);
    ui->tableTask->horizontalHeader()->setHighlightSections(false);
    ui->tableTask->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    ui->tableTask->setAlternatingRowColors(false);
    ui->tableTask->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableTask->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableTask->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableTask->setShowGrid(false);
    ui->tableTask->setFocusPolicy(Qt::NoFocus);
    ui->tableTask->verticalHeader()->setDefaultSectionSize(38);
    ui->tableTask->setStyleSheet(
        "QTableWidget {"
        "border: 1px solid #d7dce2;"
        "background: #ffffff;"
        "alternate-background-color: #ffffff;"
        "}"
        "QHeaderView::section {"
        "background: #f4f6f8;"
        "border: none;"
        "border-bottom: 1px solid #d7dce2;"
        "padding: 6px 8px;"
        "font-weight: 600;"
        "}"
        "QTableWidget::item {"
        "border-bottom: 1px solid #eef1f4;"
        "padding: 6px 8px;"
        "}"
        "QTableWidget::item:selected {"
        "background: #ffffff;"
        "color: palette(text);"
        "}"
        "QTableWidget::item:focus {"
        "background: #ffffff;"
        "color: palette(text);"
        "}"
    );
}

void MainWindow::updateTaskList(const QList<AircraftTask>& tasks)
{
    Logger::info("TASK_TABLE_RENDER",
                 "刷新执行页面任务表",
                 {{"aircraft_task_count", tasks.size()}});
    const QSignalBlocker blocker(ui->tableTask);
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
    QWidget* checkWidget = new QWidget(ui->tableTask);
    QHBoxLayout* checkLayout = new QHBoxLayout(checkWidget);
    checkLayout->setContentsMargins(0, 0, 0, 0);
    checkLayout->setAlignment(Qt::AlignCenter);

    QCheckBox* checkBox = new QCheckBox(checkWidget);
    checkBox->setProperty("aircraft_task_id", task.aircraft_task_id);
    checkBox->setCursor(Qt::PointingHandCursor);
    checkLayout->addWidget(checkBox);
    ui->tableTask->setCellWidget(row, 0, checkWidget);
    connect(checkBox, &QCheckBox::toggled,
            this, [this, checkBox](bool checked) {
                onTaskCheckChanged(checkBox, checked);
            });

    auto makeReadOnlyItem = [](const QString& text) {
        QTableWidgetItem* item = new QTableWidgetItem(text);
        item->setFlags((item->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable) & ~Qt::ItemIsEditable);
        item->setTextAlignment(Qt::AlignCenter);
        return item;
    };

    ui->tableTask->setItem(row, 1, makeReadOnlyItem(QString::number(task.aircraft_task_id)));
    ui->tableTask->setItem(row, 2, makeReadOnlyItem(QString::number(task.batch_id)));
    ui->tableTask->setItem(row, 3, makeReadOnlyItem(task.aircraft_code));

    QTableWidgetItem *statusItem = makeReadOnlyItem(getStatusText(task.status));
    statusItem->setForeground(QBrush(getStatusColor(task.status)));
    statusItem->setFont(QFont("", -1, QFont::Bold));
    ui->tableTask->setItem(row, 4, statusItem);

    ui->tableTask->setItem(row, 5, makeReadOnlyItem(QString("%1%").arg(task.progress, 0, 'f', 1)));
    ui->tableTask->setItem(row, 6, makeReadOnlyItem(task.current_phase));
}

QCheckBox* MainWindow::taskCheckBoxAt(int row) const
{
    QWidget* cellWidget = ui->tableTask->cellWidget(row, 0);
    return cellWidget ? cellWidget->findChild<QCheckBox*>() : nullptr;
}

void MainWindow::onTaskCheckChanged(QCheckBox* source, bool checked)
{
    if (!source) {
        return;
    }

    const int taskId = source->property("aircraft_task_id").toInt();
    if (checked) {
        const QSignalBlocker tableBlocker(ui->tableTask);
        for (int row = 0; row < ui->tableTask->rowCount(); ++row) {
            QCheckBox* box = taskCheckBoxAt(row);
            if (box && box != source) {
                const QSignalBlocker boxBlocker(box);
                box->setChecked(false);
            }
        }
    }

    LogContext context;
    context.operator_user_id = m_userInfo.user_id;
    context.aircraft_task_id = taskId;
    Logger::debug(checked ? "TASK_CHECKED" : "TASK_UNCHECKED",
                  QString("%1飞机升级任务 %2").arg(checked ? "勾选" : "取消勾选").arg(taskId),
                  context,
                  {{"checked", checked}});
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
    for (int row = 0; row < ui->tableTask->rowCount(); ++row) {
        QCheckBox* box = taskCheckBoxAt(row);
        if (box && box->isChecked()) {
            return QString::number(box->property("aircraft_task_id").toInt());
        }
    }
    return QString();
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

void MainWindow::startSelectedAircraftTask()
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
    showTaskProgressControls();
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
