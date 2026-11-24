TEMPLATE = app
TARGET = realtime_tests
CONFIG += console c++17
CONFIG -= app_bundle qt
INCLUDEPATH += .
SOURCES += tests/realtime_tests.cpp
win32-msvc:QMAKE_CXXFLAGS += /utf-8
