#include "appcontroller.h"

#include "../core/domain/aircraftsimulator.h"
#include "../core/logging/aircraftlogger.h"
#include "../core/network/aircraftclient.h"
#include "../ui/mainwindow.h"

namespace {
constexpr quint16 GROUND_STATION_PORT = 9001;
const char* GROUND_STATION_HOST = "127.0.0.1";
}

AppController::AppController(QObject *parent)
    : QObject{parent}
    , m_simulator(new AircraftSimulator(this))
    , m_client(new AircraftClient(m_simulator, this))
    , m_mainWindow(nullptr)
{}

int AppController::start()
{
    AircraftLogger::info("AC-1001 启动");

    m_mainWindow = new MainWindow();
    connect(m_mainWindow, &QObject::destroyed, this, [this]() {
        m_mainWindow = nullptr;
    });

    connect(m_client, &AircraftClient::connectionChanged,
            m_mainWindow, &MainWindow::setConnectionStatus);
    connect(m_mainWindow, &MainWindow::toggleGroundStationConnectionRequested,
            this, &AppController::onToggleGroundStationConnectionRequested);

    m_mainWindow->show();
    m_mainWindow->setConnectionStatus(false, "ADG 未连接地面站");

    return 0;
}

void AppController::onToggleGroundStationConnectionRequested()
{
    if (m_client->isConnected()) {
        AircraftLogger::info("用户请求断开 GroundStation 连接");
        m_client->disconnectFromGroundStation();
        return;
    }

    AircraftLogger::info("用户请求连接 GroundStation");
    m_client->connectToGroundStation(GROUND_STATION_HOST, GROUND_STATION_PORT);
}
