QT       += core gui sql network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

DEFINES += GROUNDSTATION_PROJECT_DIR=\\\"$$PWD\\\"

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += $$PWD/../Common
INCLUDEPATH += ../Common

SOURCES += \
    app/appcontroller.cpp \
    core/dao/aircrafttaskdao.cpp \
    core/dao/devicetaskdao.cpp \
    core/dao/downloadcheckpointdao.cpp \
    core/dao/transfersessiondao.cpp \
    core/localdatabase/localdatabase.cpp \
    core/logging/logger.cpp \
    core/network/deviceconnector.cpp \
    core/repository/taskrepository.cpp \
    core/services/taskservice.cpp \
    core/session/sessionmanager.cpp \
    core/domain/download/downloadmanager.cpp \
    core/domain/scheduler/taskscheduler.cpp \
    core/domain/state/taskstatemachine.cpp \
    core/domain/transfer/transfermanager.cpp \
    main.cpp \
    ui/logindialog.cpp \
    ui/mainwindow.cpp \
    core/network/serverconnector.cpp

HEADERS += \
    ../Common/taskstatus.h \
    app/appcontroller.h \
    core/dao/aircrafttaskdao.h \
    core/dao/devicetaskdao.h \
    core/dao/downloadcheckpointdao.h \
    core/localdatabase/localdatabase.h \
    core/logging/logger.h \
    core/models/aircrafttask.h \
    core/models/devicetask.h \
    core/models/downloadtask.h \
    core/models/transfersession.h \
    core/network/deviceconnector.h \
    core/repository/taskrepository.h \
    core/services/taskservice.h \
    core/session/sessionmanager.h \
    core/domain/download/downloadmanager.h \
    core/domain/scheduler/taskscheduler.h \
    core/domain/state/taskstatemachine.h \
    core/domain/transfer/transfermanager.h \
    ui/logindialog.h \
    ui/mainwindow.h \
    core/network/serverconnector.h

FORMS += \
    ui/logindialog.ui \
    ui/mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
