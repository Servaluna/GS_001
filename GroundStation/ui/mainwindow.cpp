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
    setNetworkConnectionStatus(true);
    setAircraftConnectionStatus(false);

    initButtonsByRole();
    initTableTask();
    showTaskStartControls();

    ui->stkMainPages->setCurrentIndex(0);

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
    ui->lblCurrentFile->hide();
    ui->lblCurrentPhase->hide();
    ui->progressTask->hide();

    if (!m_btnStartAircraftTask) {
        m_btnStartAircraftTask = new QPushButton("执行升级任务", ui->pnlTaskActions);
        m_btnStartAircraftTask->setObjectName("btnStartAircraftTaskRuntime");
        m_btnStartAircraftTask->setMinimumWidth(120);
        ui->layTaskActions->addWidget(m_btnStartAircraftTask);
    }

    if (!m_taskStartSpacer) {
        m_taskStartSpacer = new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum);
        ui->layTaskActions->insertSpacerItem(0, m_taskStartSpacer);
    }

    m_btnStartAircraftTask->show();
}

void MainWindow::showTaskProgressControls()
{
    if (m_btnStartAircraftTask) {
        m_btnStartAircraftTask->hide();
    }
    if (m_taskStartSpacer) {
        ui->layTaskActions->removeItem(m_taskStartSpacer);
        delete m_taskStartSpacer;
        m_taskStartSpacer = nullptr;
    }
    ui->lblCurrentFile->show();
    ui->lblCurrentPhase->show();
    ui->progressTask->show();
}

void MainWindow::initTableTask()
{
    QStringList headers = {"选择", "飞机任务ID", "批次ID", "飞机编号", "当前阶段"};
    ui->tblTasks->setColumnCount(headers.size());
    ui->tblTasks->setHorizontalHeaderLabels(headers);

    ui->tblTasks->setColumnWidth(0, 50);
    ui->tblTasks->setColumnWidth(1, 100);
    ui->tblTasks->setColumnWidth(2, 80);
    ui->tblTasks->setColumnWidth(3, 150);
    ui->tblTasks->setColumnWidth(4, 160);

    ui->tblTasks->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Fixed);
    ui->tblTasks->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tblTasks->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    ui->tblTasks->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    ui->tblTasks->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    ui->tblTasks->horizontalHeader()->setStretchLastSection(false);
    ui->tblTasks->horizontalHeader()->setHighlightSections(false);
    ui->tblTasks->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    ui->tblTasks->setAlternatingRowColors(false);
    ui->tblTasks->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tblTasks->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tblTasks->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tblTasks->setShowGrid(false);
    ui->tblTasks->setFocusPolicy(Qt::NoFocus);
    ui->tblTasks->verticalHeader()->setDefaultSectionSize(38);
    ui->tblTasks->setStyleSheet(
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
    const QSignalBlocker blocker(ui->tblTasks);
    ui->tblTasks->setRowCount(tasks.size());
    for (int i = 0; i < tasks.size(); ++i) {
        addTaskToTable(tasks[i], i);
    }
}

void MainWindow::showExecutePageAndReload()
{
    ui->stkMainPages->setCurrentIndex(0);
    loadExecutableTasks();
}

void MainWindow::addTaskToTable(const AircraftTask& task, int row)
{
    QWidget* checkWidget = new QWidget(ui->tblTasks);
    QHBoxLayout* checkLayout = new QHBoxLayout(checkWidget);
    checkLayout->setContentsMargins(0, 0, 0, 0);
    checkLayout->setAlignment(Qt::AlignCenter);

    QCheckBox* checkBox = new QCheckBox(checkWidget);
    checkBox->setProperty("aircraft_task_id", task.aircraft_task_id);
    checkBox->setCursor(Qt::PointingHandCursor);
    checkLayout->addWidget(checkBox);
    ui->tblTasks->setCellWidget(row, 0, checkWidget);
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

    ui->tblTasks->setItem(row, 1, makeReadOnlyItem(QString::number(task.aircraft_task_id)));
    ui->tblTasks->setItem(row, 2, makeReadOnlyItem(QString::number(task.batch_id)));
    ui->tblTasks->setItem(row, 3, makeReadOnlyItem(task.aircraft_code));
    ui->tblTasks->setItem(row, 4, makeReadOnlyItem(TaskStatusText::devicePhaseDisplayName(task.current_phase, task.status)));
}

QCheckBox* MainWindow::taskCheckBoxAt(int row) const
{
    QWidget* cellWidget = ui->tblTasks->cellWidget(row, 0);
    return cellWidget ? cellWidget->findChild<QCheckBox*>() : nullptr;
}

void MainWindow::onTaskCheckChanged(QCheckBox* source, bool checked)
{
    if (!source) {
        return;
    }

    const int taskId = source->property("aircraft_task_id").toInt();
    if (checked) {
        const QSignalBlocker tableBlocker(ui->tblTasks);
        for (int row = 0; row < ui->tblTasks->rowCount(); ++row) {
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

QString MainWindow::selectedAircraftTaskId() const
{
    for (int row = 0; row < ui->tblTasks->rowCount(); ++row) {
        QCheckBox* box = taskCheckBoxAt(row);
        if (box && box->isChecked()) {
            return QString::number(box->property("aircraft_task_id").toInt());
        }
    }
    return QString();
}

void MainWindow::setNetworkConnectionStatus(bool connected)
{
    ui->lblNetworkConnection->setText(connected
        ? "网络连接状态：已连接CentralServer"
        : "网络连接状态：未连接CentralServer");
}

void MainWindow::setAircraftConnectionStatus(bool connected)
{
    ui->lblDeviceConnection->setText(connected
        ? "飞机连接状态：已连接AC-1001"
        : "飞机连接状态：未连接AC-1001");
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
    ui->stkMainPages->setCurrentIndex(1);
}

void MainWindow::on_btnLogs_clicked()
{
    ui->stkMainPages->setCurrentIndex(2);
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

void MainWindow::on_btnLogout_clicked()
{
    emit logoutFromMainWindow();
}
