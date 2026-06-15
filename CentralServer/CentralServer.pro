QT       += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += $$PWD/../Common

SOURCES += \
    core/dao/taskdao.cpp \
    core/dao/userdao.cpp \
    core/services/taskservice.cpp \
    core/services/userservice.cpp \
    core/network/clienthandler.cpp \
    main.cpp \
    core/database/databasemanager.cpp \
    core/logging/serverlogger.cpp \
    core/network/server.cpp \
    ui/serverwindow.cpp

HEADERS += \
    ../Common/fileprotocol.h \
    ../Common/models.h \
    ../Common/protocol.h \
    ../Common/taskstatus.h \
    core/dao/taskdao.h \
    core/dao/userdao.h \
    core/database/databasemanager.h \
    core/logging/serverlogger.h \
    core/services/taskservice.h \
    core/services/userservice.h \
    core/network/clienthandler.h \
    core/network/server.h \
    ui/serverwindow.h

FORMS += \
    ui/serverwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
