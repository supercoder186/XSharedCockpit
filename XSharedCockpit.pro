TEMPLATE = lib
QT -= gui core

CONFIG += warn_on plugin c++11
CONFIG -= thread qt rtti debug_and_release_target

VERSION = 1.3.0

DEFINES += XPLM200
DEFINES += XPLM210
DEFINES += XPLM300
DEFINES += XPLM301

debug {
    DEFINES += DEBUG
} else {
    CONFIG += release
}

INCLUDEPATH += "lib/SDK/CHeaders/XPLM"
INCLUDEPATH += "lib/SDK/CHeaders/Wrappers"
INCLUDEPATH += "lib/SDK/CHeaders/Widgets"
INCLUDEPATH += "lib/boost_1_73_0"

win32 {
    LIBS += -L"F:/Programming/X-Plane/XSharedCockpit/lib/SDK/Libraries/Win"
    LIBS += -lXPLM_64
    LIBS += -lXPWidgets_64
    LIBS += -lws2_32
    DEFINES += APL=0 IBM=1 LIN=0
    TARGET = win
    TARGET_EXT = .xpl
}

unix:!macx {
    CONFIG += no_plugin_name_prefix
    DEFINES += APL=0 IBM=0 LIN=1
    TARGET = lin
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
