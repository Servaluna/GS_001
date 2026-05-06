#include "deviceconnectorwindow.h"
#include "ui_deviceconnectorwindow.h"
#include "../core/network/deviceconnector.h"

deviceconnectorwindow::deviceconnectorwindow(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::deviceconnectorwindow)
    , m_connector(nullptr)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    this->setWindowTitle("连接设备");
}

deviceconnectorwindow::deviceconnectorwindow(DeviceConnector* connector, QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::deviceconnectorwindow)
    , m_connector(connector)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    this->setWindowTitle("连接设备");

    if (m_connector) {
        connect(m_connector, &DeviceConnector::cmcConnectionChanged,
                this, [this](bool connected, const QString& errorMessage) {
                    if (connected) {
                        QMessageBox::information(this, "连接提示", "连接设备成功");
                    } else {
                        QMessageBox::warning(this, "连接提示", errorMessage.isEmpty() ? "网络异常连接失败" : errorMessage);
                    }
                });
    }
}

deviceconnectorwindow::~deviceconnectorwindow()
{
    delete ui;
}

void deviceconnectorwindow::on_btnConnect_clicked()
{
    QString IP = ui ->lineEdit_IP ->text();
    QString Port = ui ->lineEdit_Port ->text();

    if (!m_connector) {
        QMessageBox::warning(this, "连接提示", "设备连接器未初始化");
        return;
    }

    m_connector->connectToCMC(IP, Port.toUShort());
}


void deviceconnectorwindow::on_btnCancel_clicked()
{
    this -> close();
}

