#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void toggleGroundStationConnectionRequested();

public slots:
    void setConnectionStatus(bool connected, const QString& message);

private slots:
    void on_btnToggleGsConnection_clicked();

private:
    Ui::MainWindow *ui;
    bool m_connected = false;
};

#endif // MAINWINDOW_H
