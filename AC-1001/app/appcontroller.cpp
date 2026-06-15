#include "appcontroller.h"

#include "../core/domain/aircraftsimulator.h"
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
        m_client->disconnectFromGroundStation();
        return;
    }

    m_client->connectToGroundStation(GROUND_STATION_HOST, GROUND_STATION_PORT);
}
