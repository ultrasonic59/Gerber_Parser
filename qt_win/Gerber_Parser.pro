QT       += core gui widgets
CONFIG   += c++17
TARGET   = Gerber_Parser
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    Gerber_Parser.cpp \
    gerberview.cpp

HEADERS += \
    mainwindow.h \
    Gerber_Parser.h \
    gerbertypes.h \
    gerberview.h
