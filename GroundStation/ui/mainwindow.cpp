#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../core/logging/logger.h"
#include "../core/services/taskservice.h"

#include <QCheckBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QSignalBlocker>
#include <QStatusBar>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {

QString roleDisplayName(int roleId, const QString& fallbackRole)
{
    const UserRole::Role role = UserRole::roleFromId(roleId);
    if (role != UserRole::Unknown) {
        return UserRole::roleToString(role);
    }

    return fallbackRole.isEmpty() ? QStringLiteral("Unknown") : fallbackRole;
}

}

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
    const QString roleName = roleDisplayName(m_userInfo.role_id, m_userInfo.role);
    ui->lblUsername->setText(QStringLiteral("用户名：%1").arg(m_userInfo.username));
    ui->lblRole->setText(QStringLiteral("身份：%1").arg(roleName));

    setWindowTitle(QStringLiteral("地面站系统 - [%1]").arg(roleName));
    statusBar()->showMessage(QStringLiteral("欢迎用户：%1 (%2)").arg(m_userInfo.username, roleName), 5000);

    ui->btnAlwaysOnTop->setCheckable(true);
    ui->btnAlwaysOnTop->setChecked(false);
    ui->btnAlwaysOnTop->setToolTip(QStringLiteral("让地面站窗口保持在屏幕最前面"));

    setNetworkConnectionStatus(true);
    setAircraftConnectionStatus(false);

    initButtonsByRole();
    initTableTask();
    initLogsPage();
    resetTaskProgressPanel();
    showTaskStartControls();

    ui->stkMainPages->setCurrentIndex(0);

    connect(ui->btnAlwaysOnTop, &QPushButton::toggled,
            this, &MainWindow::on_btnAlwaysOnTop_toggled);
    connect(ui->btnStartAircraftTask, &QPushButton::clicked,
            this, &MainWindow::startSelectedAircraftTask);
    connect(&Logger::instance(), &Logger::logGenerated,
            this, &MainWindow::appendLogEntryToTable);
    if (m_taskService) {
        connect(m_taskService, &TaskService::taskStarted,
                this, &MainWindow::onTaskStarted);
        connect(m_taskService, &TaskService::taskProgressUpdated,
                this, &MainWindow::onTaskProgressUpdated);
        connect(m_taskService, &TaskService::taskFinished,
                this, &MainWindow::onTaskFinished);
    }

    const QList<LogEntry> existingLogs = Logger::instance().recentEntries();
    for (const LogEntry& entry : existingLogs) {
        appendLogEntryToTable(entry);
    }

    loadExecutableTasks();
}

void MainWindow::on_btnAlwaysOnTop_toggled(bool checked)
{
    const Qt::WindowFlags flags = checked
        ? (windowFlags() | Qt::WindowStaysOnTopHint)
        : (windowFlags() & ~Qt::WindowStaysOnTopHint);

    setWindowFlags(flags);
    ui->btnAlwaysOnTop->setText(checked ? QStringLiteral("取消置顶") : QStringLiteral("置顶"));
    show();
    raise();
    activateWindow();
}

void MainWindow::initButtonsByRole()
{
    ui->btnObtain->setVisible(false);
    ui->btnCreateBatch->setVisible(false);

    switch (UserRole::roleFromId(m_userInfo.role_id)) {
    case UserRole::Admin:
        ui->btnExecute->setVisible(true);
        ui->btnLogs->setVisible(true);
        break;
    case UserRole::Engineer:
    case UserRole::Operator:
        ui->btnExecute->setVisible(true);
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
                     QStringLiteral("任务服务为空，无法加载任务表"),
                     {{"user_id", m_userInfo.user_id}, {"role_id", m_userInfo.role_id}});
        updateTaskList({});
        return;
    }

    Logger::info("TASK_TABLE_LOAD_START",
                 QStringLiteral("开始加载执行页面任务表"),
                 {{"user_id", m_userInfo.user_id}, {"role_id", m_userInfo.role_id}, {"username", m_userInfo.username}});
    const QList<AircraftTask> tasks = m_taskService->getExecutableAircraftTasksForUser(m_userInfo.user_id, m_userInfo.role_id);
    Logger::info("TASK_TABLE_LOAD_FINISHED",
                 QStringLiteral("执行页面任务表数据加载完成"),
                 {{"user_id", m_userInfo.user_id}, {"role_id", m_userInfo.role_id}, {"aircraft_task_count", tasks.size()}});
    updateTaskList(tasks);
}

void MainWindow::showTaskStartControls()
{
    ui->stkTaskActions->setCurrentWidget(ui->pageTaskActionStart);
}

void MainWindow::showTaskProgressControls()
{
    ui->stkTaskActions->setCurrentWidget(ui->pageTaskActionProgress);
}

void MainWindow::resetTaskProgressPanel()
{
    m_activeAircraftTaskId.clear();
    ui->lblCurrentFile->setText(QStringLiteral("当前任务：未执行"));
    ui->lblCurrentPhase->setText(QStringLiteral("当前阶段：等待开始"));
    ui->progressTask->setRange(0, 100);
    ui->progressTask->setValue(0);
    ui->progressTask->setFormat(QStringLiteral("%p%"));
}

void MainWindow::initTableTask()
{
    const QStringList headers = {
        QStringLiteral("选择"),
        QStringLiteral("飞机任务ID"),
        QStringLiteral("批次ID"),
        QStringLiteral("飞机编号"),
        QStringLiteral("当前阶段")
    };

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

void MainWindow::initLogsPage()
{
    ui->lblLogsTitle->setStyleSheet("font-size: 18px; font-weight: 600;");
    ui->lblLogsSummary->setStyleSheet("color: #5f6b7a; line-height: 1.4;");

    m_lblLogSummary = ui->lblLogsSummary;
    m_tblLogs = ui->tblLogs;
    m_tblLogs->setColumnCount(5);
    m_tblLogs->setHorizontalHeaderLabels({
        QStringLiteral("时间"),
        QStringLiteral("级别"),
        QStringLiteral("事件"),
        QStringLiteral("内容"),
        QStringLiteral("详情")
    });
    m_tblLogs->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tblLogs->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tblLogs->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tblLogs->setAlternatingRowColors(false);
    m_tblLogs->setShowGrid(false);
    m_tblLogs->setWordWrap(false);
    m_tblLogs->setFocusPolicy(Qt::NoFocus);
    m_tblLogs->verticalHeader()->setVisible(false);
    m_tblLogs->verticalHeader()->setDefaultSectionSize(34);
    m_tblLogs->horizontalHeader()->setDefaultAlignment(Qt::AlignCenter);
    m_tblLogs->horizontalHeader()->setHighlightSections(false);
    m_tblLogs->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_tblLogs->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tblLogs->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tblLogs->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_tblLogs->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    m_tblLogs->setStyleSheet(
        "QTableWidget {"
        "border: 1px solid #d7dce2;"
        "background: #ffffff;"
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
    );
}

void MainWindow::updateTaskList(const QList<AircraftTask>& tasks)
{
    Logger::info("TASK_TABLE_RENDER",
                 QStringLiteral("刷新执行页面任务表"),
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

void MainWindow::onTaskStarted(const QString& taskId, const QString& taskName)
{
    m_activeAircraftTaskId = taskId;
    ui->lblCurrentFile->setText(QStringLiteral("当前任务：%1").arg(taskName));
    ui->lblCurrentPhase->setText(QStringLiteral("当前阶段：准备执行"));
    ui->progressTask->setValue(0);
    ui->progressTask->setFormat(QStringLiteral("%p%"));
    showTaskProgressControls();
    if (ui->stkMainPages->currentWidget() == ui->pageExecute) {
        statusBar()->showMessage(QStringLiteral("升级任务 %1 开始执行").arg(taskId), 5000);
    }
}

void MainWindow::onTaskProgressUpdated(const QString& taskId, const QString& step, int progress, qint64 speed)
{
    if (!m_activeAircraftTaskId.isEmpty() && taskId != m_activeAircraftTaskId) {
        return;
    }

    m_activeAircraftTaskId = taskId;
    ui->lblCurrentPhase->setText(QStringLiteral("当前阶段：%1").arg(step));
    ui->progressTask->setValue(qBound(0, progress, 100));
    if (speed > 0) {
        ui->progressTask->setFormat(QStringLiteral("%1% (%2 KB/s)")
                                        .arg(qBound(0, progress, 100))
                                        .arg(speed / 1024));
    } else {
        ui->progressTask->setFormat(QStringLiteral("%p%"));
    }
    showTaskProgressControls();
}

void MainWindow::onTaskFinished(const QString& taskId, bool success, const QString& message)
{
    if (!m_activeAircraftTaskId.isEmpty() && taskId != m_activeAircraftTaskId) {
        return;
    }

    ui->lblCurrentPhase->setText(
        QStringLiteral("当前阶段：%1").arg(success ? QStringLiteral("执行完成") : QStringLiteral("执行失败"))
    );
    ui->progressTask->setValue(success ? 100 : ui->progressTask->value());
    ui->progressTask->setFormat(QStringLiteral("%p%"));
    statusBar()->showMessage(message, 5000);
    loadExecutableTasks();
    resetTaskProgressPanel();
    showTaskStartControls();
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

void MainWindow::appendLogEntryToTable(const LogEntry& entry)
{
    if (!m_tblLogs) {
        return;
    }

    const int row = m_tblLogs->rowCount();
    m_tblLogs->insertRow(row);

    auto makeItem = [](const QString& text, Qt::Alignment alignment = Qt::AlignCenter) {
        auto* item = new QTableWidgetItem(text);
        item->setFlags((item->flags() | Qt::ItemIsEnabled | Qt::ItemIsSelectable) & ~Qt::ItemIsEditable);
        item->setTextAlignment(alignment);
        item->setToolTip(text);
        return item;
    };

    const QString detailText = QString::fromUtf8(
        QJsonDocument(entry.event_detail).toJson(QJsonDocument::Compact)
    );

    m_tblLogs->setItem(row, 0, makeItem(entry.created_at.toString("hh:mm:ss.zzz")));
    m_tblLogs->setItem(row, 1, makeItem(entry.event_level));
    m_tblLogs->setItem(row, 2, makeItem(entry.event_type));
    m_tblLogs->setItem(row, 3, makeItem(entry.event_message, Qt::AlignVCenter | Qt::AlignLeft));
    m_tblLogs->setItem(row, 4, makeItem(detailText, Qt::AlignVCenter | Qt::AlignLeft));

    m_tblLogs->scrollToBottom();

    if (m_lblLogSummary) {
        m_lblLogSummary->setText(
            QStringLiteral("显示当前 GroundStation 客户端运行期间产生的日志\n当前共 %1 条")
                .arg(m_tblLogs->rowCount())
        );
    }
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
                  QStringLiteral("%1飞机升级任务 %2")
                      .arg(checked ? QStringLiteral("勾选") : QStringLiteral("取消勾选"))
                      .arg(taskId),
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
        ? QStringLiteral("网络连接状态：已连接CentralServer")
        : QStringLiteral("网络连接状态：未连接CentralServer"));
}

void MainWindow::setAircraftConnectionStatus(bool connected, const QString& aircraftCode)
{
    m_aircraftConnected = connected;
    m_connectedAircraftCode = connected ? aircraftCode.trimmed() : QString();

    if (!connected) {
        ui->lblDeviceConnection->setText(QStringLiteral("飞机连接状态：未连接飞机"));
        return;
    }

    if (m_connectedAircraftCode.isEmpty()) {
        ui->lblDeviceConnection->setText(QStringLiteral("飞机连接状态：已连接，等待飞机编号"));
        return;
    }

    ui->lblDeviceConnection->setText(QStringLiteral("飞机连接状态：已连接%1").arg(m_connectedAircraftCode));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_userInfo.username.isEmpty()) {
        const int ret = QMessageBox::question(
            this,
            QStringLiteral("退出确认"),
            QStringLiteral("用户 %1 正在使用程序，是否退出账户并关闭程序？").arg(m_userInfo.username),
            QMessageBox::Yes | QMessageBox::No
        );
        if (ret == QMessageBox::No) {
            event->ignore();
            return;
        }
    }

    event->accept();
    Logger::info("APP_EXIT",
                 QStringLiteral("用户 %1 退出程序").arg(m_userInfo.username),
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
    if (!m_aircraftConnected) {
        LogContext context;
        context.operator_user_id = m_userInfo.user_id;
        Logger::warn("TASK_START_REJECTED",
                     QStringLiteral("未连接飞机，禁止执行升级任务"),
                     context);
        QMessageBox::warning(this,
                             QStringLiteral("执行任务"),
                             QStringLiteral("当前未连接任何飞机，无法执行升级任务。"));
        return;
    }

    const QString taskId = selectedAircraftTaskId();
    if (taskId.isEmpty()) {
        QMessageBox::information(this,
                                 QStringLiteral("执行任务"),
                                 QStringLiteral("请先勾选一个需要执行的飞机升级任务。"));
        return;
    }

    if (!m_taskService || !m_taskService->startTask(taskId)) {
        LogContext context;
        context.operator_user_id = m_userInfo.user_id;
        context.aircraft_task_id = taskId.toInt();
        Logger::warn("TASK_START_REJECTED",
                     QStringLiteral("飞机升级任务启动失败: %1").arg(taskId),
                     context);
        QMessageBox::warning(this,
                             QStringLiteral("执行任务"),
                             QStringLiteral("启动升级任务失败，请检查任务状态或查看日志页面。"));
        return;
    }

    LogContext context;
    context.operator_user_id = m_userInfo.user_id;
    context.aircraft_task_id = taskId.toInt();
    Logger::info("TASK_START_SUBMITTED",
                 QStringLiteral("飞机升级任务已提交执行 %1").arg(taskId),
                 context);
    m_activeAircraftTaskId = taskId;
    ui->lblCurrentFile->setText(QStringLiteral("当前任务：飞机任务 %1").arg(taskId));
    ui->lblCurrentPhase->setText(QStringLiteral("当前阶段：等待调度启动"));
    ui->progressTask->setValue(0);
    ui->progressTask->setFormat(QStringLiteral("%p%"));
    showTaskProgressControls();
    statusBar()->showMessage(QStringLiteral("已提交升级任务 %1").arg(taskId), 5000);
    loadExecutableTasks();
}

void MainWindow::on_btnLogout_clicked()
{
    emit logoutFromMainWindow();
}
