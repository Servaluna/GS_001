QT += core gui network widgets

CONFIG += c++17
TARGET = AC-1001
TEMPLATE = app

DEFINES += AC1001_PROJECT_DIR=\\\"$$PWD\\\"

SOURCES += \
    app/appcontroller.cpp \
    core/domain/aircraftsimulator.cpp \
    core/logging/aircraftlogger.cpp \
    core/network/aircraftclient.cpp \
    main.cpp \
    ui/mainwindow.cpp

HEADERS += \
    app/appcontroller.h \
    core/domain/aircraftsimulator.h \
    core/logging/aircraftlogger.h \
    core/models/aircraftdevice.h \
    core/network/aircraftclient.h \
    ui/mainwindow.h

FORMS += \
    ui/mainwindow.ui
