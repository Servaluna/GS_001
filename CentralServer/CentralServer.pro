QT       += core gui network sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

INCLUDEPATH += $$PWD/../Common

SOURCES += \
    core/database/models/user.cpp \
    core/network/clienthandler.cpp \
    main.cpp \
    core/database/databasemanager.cpp \
    core/network/server.cpp

HEADERS += \
    ../Common/fileprotocol.h \
    ../Common/models.h \
    ../Common/protocol.h \
    core/database/databasemanager.h \
    core/database/models/user.h \
    core/network/clienthandler.h \
    core/network/server.h

FORMS += \
    core/network/server.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
