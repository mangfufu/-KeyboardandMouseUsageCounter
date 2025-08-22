QT       += core gui

# 设置项目名称
TARGET = KeyboardandMouseUsage

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets network

CONFIG += c++17
RC_FILE += icon.rc
RESOURCES += resources.qrc
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    globalhookmanager.cpp

HEADERS += \
    mainwindow.h \
    globalhookmanager.h

FORMS += \
mainwindow.ui

# 添加Windows库支持
win32: LIBS += -luser32

# 配置QuaZip库 (如果已安装)
# 手动指定QuaZip库的路径和链接选项
# win32: LIBS += -L$$PWD/quazip/lib -lquazip
# INCLUDEPATH += $$PWD/quazip/include
# DEPENDPATH += $$PWD/quazip/include

# 对于未安装QuaZip的情况，默认使用手动更新方式
DEFINES += NO_QUAZIP

# 说明：如果需要启用ZIP文件自动解压缩功能，请先下载并编译QuaZip库，
# 然后取消上面的注释并根据实际安装路径调整库和头文件的路径。

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    icon.rc
