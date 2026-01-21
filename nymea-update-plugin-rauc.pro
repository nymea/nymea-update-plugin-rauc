QT += network dbus
QT -= gui

TARGET = $$qtLibraryTarget(nymea_updatepluginrauc)
TEMPLATE = lib

greaterThan(QT_MAJOR_VERSION, 5) {
    message("Building using Qt6 support")
    CONFIG *= c++17
    QMAKE_LFLAGS *= -std=c++17
    QMAKE_CXXFLAGS *= -std=c++17
} else {
    message("Building using Qt5 support")
    CONFIG *= c++11
    QMAKE_LFLAGS *= -std=c++11
    QMAKE_CXXFLAGS *= -std=c++11
    DEFINES += QT_DISABLE_DEPRECATED_UP_TO=0x050F00
}

QMAKE_CXXFLAGS += -g -Werror

CONFIG += plugin link_pkgconfig
PKGCONFIG += nymea

SOURCES += \
    raucinstallerinterface.cpp \
    raucdbusinterface.cpp \
    selfhostedrepository.cpp \
    updatecontrollerrauc.cpp

HEADERS += \
    raucdbustypes.h \
    raucinstallerinterface.h \
    raucdbusinterface.h \
    selfhostedrepository.h \
    updatecontrollerrauc.h

OTHER_FILES += \
    dbus/rauc-introspection.xml

target.path = $$[QT_INSTALL_LIBS]/nymea/platform/
INSTALLS += target
