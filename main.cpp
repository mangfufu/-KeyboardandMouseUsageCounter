#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    a.setApplicationName("键鼠使用统计");
    MainWindow w;
    w.show();
    return a.exec();
}
