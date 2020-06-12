TEMPLATE = lib
QT -= gui core

CONFIG += warn_on plugin c++11
CONFIG -= thread qt rtti debug_and_release_target

VERSION = 1.0.0

DEFINES += XPLM200
DEFINES += XPLM210
DEFINES += XPLM300
DEFINES += XPLM301

debug {
    DEFINES += DEBUG
} else {
    CONFIG += release
}

INCLUDEPATH += "F:/Programming/X-Plane/SDK/CHeaders/XPLM"
INCLUDEPATH += "F:/Programming/X-Plane/SDK/CHeaders/Wrappers"
INCLUDEPATH += "F:/Programming/X-Plane/SDK/CHeaders/Widgets"
INCLUDEPATH += "F:/Programming/X-Plane/boost_1_73_0"

win32 {
    LIBS += -L"F:/Programming/X-Plane/SDK/Libraries/Win"
    LIBS += -lXPLM_64 -lXPWidgets_64
    LIBS += -lws2_32
    DEFINES += APL=0 IBM=1 LIN=0
    TARGET = win
    TARGET_EXT = .xpl
}

unix:!macx {
    CONFIG += no_plugin_name_prefix
    DEFINES += APL=0 IBM=0 LIN=1
    QMAKE_EXTENSION_SHLIB = xpl
    QMAKE_CXXFLAGS += -fvisibility=hidden -shared -rdynamic -nodefaultlibs -undefined_warning
}

macx {
    TARGET = mac
    TARGET_EXT = .xpl
    DEFINES += APL=1 IBM=0 LIN=0
    QMAKE_LFLAGS += -undefined suppress
    CONFIG += x64 ppc
}

SOURCES +=     plugin.cpp
