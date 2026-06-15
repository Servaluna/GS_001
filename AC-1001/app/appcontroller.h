#ifndef APPCONTROLLER_H
#define APPCONTROLLER_H

#include <QObject>
#include <QString>

class AircraftClient;
class AircraftSimulator;
class MainWindow;

class AppController : public QObject
{
    Q_OBJECT

public:
    explicit AppController(QObject *parent = nullptr);
    int start();

private slots:
    void onToggleGroundStationConnectionRequested();

private:
    AircraftSimulator* m_simulator;
    AircraftClient* m_client;
    MainWindow* m_mainWindow;
};

#endif // APPCONTROLLER_H
