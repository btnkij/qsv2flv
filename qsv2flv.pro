QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    converter.cpp \
    datamodel.cpp \
    inputfilemodel.cpp \
    main.cpp \
    mainwindow.cpp \
    qsvreader.cpp

HEADERS += \
    converter.h \
    datamodel.h \
    inputfilemodel.h \
    mainwindow.h \
    qsvreader.h

FORMS += \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

INCLUDEPATH += $$PWD/ffmpeg/include

LIBS += -L$$PWD/"ffmpeg/lib" -lavcodec \
        -L$$PWD/"ffmpeg/lib" -lavdevice \
        -L$$PWD/"ffmpeg/lib" -lavfilter \
        -L$$PWD/"ffmpeg/lib" -lavformat \
        -L$$PWD/"ffmpeg/lib" -lavutil \
        -L$$PWD/"ffmpeg/lib" -lpostproc \
        -L$$PWD/"ffmpeg/lib" -lswresample \
        -L$$PWD/"ffmpeg/lib" -lswscale
