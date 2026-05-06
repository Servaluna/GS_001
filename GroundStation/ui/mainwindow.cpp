#include "mainwindow.h"
#include "ui_mainwindow.h"

#define DEBUG_LOCATION qDebug().nospace() << "[" << Q_FUNC_INFO << " @ " << QFileInfo(__FILE__).fileName() << ":" << __LINE__ << "]"


MainWindow::MainWindow( QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initUI();
}

MainWindow::MainWindow(QString token, const UserInfo& userInfo, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , m_token(token)
    , m_userInfo(userInfo)
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
    DEBUG_LOCATION;

    QString title = QString("地面站系统 - [%1]").arg(m_userInfo.role);
    setWindowTitle(title);

    QString welcome = QString("欢迎用户: %1 (%2)").arg(m_userInfo.username , m_userInfo.role);
    statusBar()->showMessage(welcome, 5000);

    initButtonsByRole();
    initTableTask();

    connect(ui->tableTask, &QTableWidget::itemClicked,
            this, &MainWindow::onTaskItemClicked);

}

void MainWindow::initButtonsByRole()
{
    // 鏍规嵁瑙掕壊璁剧疆鎸夐挳鍙鎬?
    switch(UserRole::roleFromString(m_userInfo.role)) {
    case UserRole::Admin:
        // Admin鍙互鐪嬪埌鎵€鏈夋寜閽?
        ui->btnExecute->setVisible(true);
        ui->btnObtain->setVisible(true);
        ui->btnLogs->setVisible(true);
        break;

    case UserRole::Engineer:
        // Engineer鍙兘鐪嬪埌Execute鍜孫btain
        ui->btnExecute->setVisible(true);
        ui->btnObtain->setVisible(true);
        ui->btnLogs->setVisible(false);
        break;

    case UserRole::Operator:
        // Operator鍙兘鐪嬪埌Execute
        ui->btnExecute->setVisible(true);
        ui->btnObtain->setVisible(false);
        ui->btnLogs->setVisible(false);
        break;

    default:
        break;
    }
}

void MainWindow::initTableTask()
{
    // 璁剧疆琛ㄥご
    QStringList headers = {"选择", "任务ID", "类型", "描述", "优先级", "状态",
                           "创建时间", "开始时间", "结束时间", "创建者"};
    ui->tableTask->setColumnCount(headers.size());
    ui->tableTask->setHorizontalHeaderLabels(headers);

    // 璁剧疆鍒楀
    ui->tableTask->setColumnWidth(0, 50);   // 閫夋嫨鍒?
    ui->tableTask->setColumnWidth(1, 120);  // 浠诲姟ID
    ui->tableTask->setColumnWidth(2, 80);   // 绫诲瀷
    ui->tableTask->setColumnWidth(3, 200);  // 鎻忚堪
    ui->tableTask->setColumnWidth(4, 60);   // 浼樺厛绾?
    ui->tableTask->setColumnWidth(5, 80);   // 鐘舵€?
    ui->tableTask->setColumnWidth(6, 140);  // 鍒涘缓鏃堕棿
    ui->tableTask->setColumnWidth(7, 140);  // 寮€濮嬫椂闂?
    ui->tableTask->setColumnWidth(8, 140);  // 缁撴潫鏃堕棿
    ui->tableTask->setColumnWidth(9, 80);   // 鍒涘缓鑰?

    // 璁╂渶鍚庝竴鍒楀～婊″墿浣欑┖闂?
    ui->tableTask->horizontalHeader()->setStretchLastSection(true);

    // 璁剧疆琛ㄦ牸鏍峰紡
    ui->tableTask->setAlternatingRowColors(true);
    ui->tableTask->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableTask->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableTask->setEditTriggers(QAbstractItemView::NoEditTriggers);  // 鍙

    // 璁剧疆琛岄珮
    ui->tableTask->verticalHeader()->setDefaultSectionSize(30);
}

void MainWindow::updateTaskList(const QList<TaskBasicInfo> &tasks)
{
    // 璁剧疆琛屾暟
    ui->tableTask->setRowCount(tasks.size());

    // 閫愯娣诲姞鏁版嵁
    for (int i = 0; i < tasks.size(); ++i) {
        addTaskToTable(tasks[i], i);
    }
}

void MainWindow::addTaskToTable(const TaskBasicInfo &task, int row)
{
    // 0: 閫夋嫨 (澶嶉€夋)
    QTableWidgetItem *checkItem = new QTableWidgetItem();
    checkItem->setCheckState(Qt::Unchecked);
    ui->tableTask->setItem(row, 0, checkItem);

    // 1: 浠诲姟ID
    ui->tableTask->setItem(row, 1, new QTableWidgetItem(task.task_id));

    // 2: 浠诲姟绫诲瀷
    ui->tableTask->setItem(row, 2, new QTableWidgetItem(TaskType::toString(task.task_type)));

    // 3: 鎻忚堪
    ui->tableTask->setItem(row, 3, new QTableWidgetItem(task.description));

    // 4: 浼樺厛绾?
    QTableWidgetItem *priorityItem = new QTableWidgetItem(QString::number(task.priority));

    if (task.priority >= 8) {
        priorityItem->setForeground(QBrush(Qt::red));
        priorityItem->setFont(QFont("", -1, QFont::Bold));
    }
    ui->tableTask->setItem(row, 4, priorityItem);

    // 5: 鐘舵€?
    QTableWidgetItem *statusItem = new QTableWidgetItem(getStatusText(task.status));
    statusItem->setForeground(QBrush(getStatusColor(task.status)));
    statusItem->setFont(QFont("", -1, QFont::Bold));
    ui->tableTask->setItem(row, 5, statusItem);

    // 6锛氬垱寤烘椂闂?
    ui->tableTask->setItem(row, 6, new QTableWidgetItem(task.create_time.toString("yyyy-MM-dd hh:mm:ss")));

    // 7锛氬紑濮嬫椂闂?
    ui->tableTask->setItem(row, 7, new QTableWidgetItem(task.start_time.isValid() ? task.start_time.toString("yyyy-MM-dd hh:mm:ss"): "-"));

    // 8锛氱粨鏉熸椂闂?
    ui->tableTask->setItem(row, 8, new QTableWidgetItem(task.start_time.isValid() ? task.end_time.toString("yyyy-MM-dd hh:mm:ss"): "-"));

    // 9锛氬垱寤鸿€?
    ui->tableTask->setItem(row, 9, new QTableWidgetItem(task.creator));

    // 瀛樺偍瀹屾暣鐨勪换鍔′俊鎭埌绗?鍒楋紙闅愯棌鏁版嵁锛?
    ui->tableTask->item(row, 0)->setData(Qt::UserRole,QVariant::fromValue(task));  // 闇€瑕佹敞鍐岃嚜瀹氫箟绫诲瀷
}

QString MainWindow::getStatusText(TaskStatus::Status status)
{
    switch(status) {
    case TaskStatus::Pending:   return "待执行";
    case TaskStatus::Running:   return "执行中";
    case TaskStatus::Completed: return "已完成";
    case TaskStatus::Failed:    return "失败";
    case TaskStatus::Cancelled: return "已取消";
    default: return "未知";
    }
}

QColor MainWindow::getStatusColor(TaskStatus::Status status)
{
    switch(status) {
    case TaskStatus::Pending:   return QColor(255, 165, 0);  // 姗欒壊
    case TaskStatus::Running:   return QColor(0, 120, 215);  // 钃濊壊
    case TaskStatus::Completed: return QColor(0, 150, 0);    // 缁胯壊
    case TaskStatus::Failed:    return QColor(220, 20, 60);  // 绾㈣壊
    case TaskStatus::Cancelled: return QColor(128, 128, 128); // 鐏拌壊
    default: return Qt::black;
    }
}

void MainWindow::onTaskItemClicked(QTableWidgetItem *item)
{
    if (!item) return;

    int row = item->row();

    // 鑾峰彇璇ヨ鐨勪换鍔D
    QString taskId = ui->tableTask->item(row, 0)->text();

    // 鎴栬€呰幏鍙栧畬鏁寸殑浠诲姟淇℃伅锛堝鏋滃瓨鍌ㄤ簡锛?
    // TaskBasicInfo task = ui->taskTable->item(row, 0)->data(Qt::UserRole).value<TaskBasicInfo>();

    qDebug() << "閫変腑浠诲姟:" << taskId;

    // 鍙互鎵撳紑浠诲姟璇︽儏瀵硅瘽妗?
    // showTaskDetailDialog(taskId);
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

    event->accept();    // 鎺ュ彈鍏抽棴浜嬩欢
    DEBUG_LOCATION << QString("用户 %1 退出程序").arg(m_userInfo.username);
}

void MainWindow::on_btnExecute_clicked()
{
   ui->stackedWidget ->setCurrentIndex(0);
}
void MainWindow::on_btnObtain_clicked()
{
    ui->stackedWidget ->setCurrentIndex(1);
}
void MainWindow::on_btnLogs_clicked()
{
    ui->stackedWidget ->setCurrentIndex(2);
}


void MainWindow::on_btnConnectToDevices_clicked()
{
    emit openDeviceConnector();
}


void MainWindow::on_btnLogout_clicked()
{
    emit logoutFromMainWindow();
}

