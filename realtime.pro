TEMPLATE = app
TARGET = realtime_demo
CONFIG += console c++17
CONFIG -= app_bundle qt
INCLUDEPATH += .
SOURCES += examples/realtime_demo.cpp
win32-msvc:QMAKE_CXXFLAGS += /utf-8
