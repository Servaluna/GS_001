#ifndef DEVICECONNECTORWINDOW_H
#define DEVICECONNECTORWINDOW_H

#include <QWidget>
#include <QMessageBox>

class DeviceConnector;

namespace Ui {
class deviceconnectorwindow;
}

class deviceconnectorwindow : public QWidget
{
    Q_OBJECT

public:
    explicit deviceconnectorwindow(QWidget *parent = nullptr);
    explicit deviceconnectorwindow(DeviceConnector* connector, QWidget *parent = nullptr);
    ~deviceconnectorwindow();

private slots:
    void on_btnConnect_clicked();
    void on_btnCancel_clicked();

private:
    Ui::deviceconnectorwindow *ui;
    DeviceConnector* m_connector;
};

#endif // DEVICECONNECTORWINDOW_H
