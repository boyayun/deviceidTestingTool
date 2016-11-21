#-------------------------------------------------
#
# Project created by QtCreator 2016-09-01T15:59:20
#
#-------------------------------------------------

QT       += core gui \
	  serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = deviceidTestingTool
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    UserUartLink.cpp

HEADERS  += mainwindow.h \
    UserUartLink.h

FORMS    += mainwindow.ui
