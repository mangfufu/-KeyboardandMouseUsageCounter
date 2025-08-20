#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QShowEvent>
#include <QCloseEvent>
#include <QSettings>
#include <QTimer>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QDate>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QStyle>
#include <QTime>
#include <QDialog>
#include <QScrollArea>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QVector>
#include <QFrame>
#include <QtGlobal>
#include <Windows.h>

// 定义UI标签指针
QLabel *dateLabel;
QLabel *keyCountLabel;
QLabel *mouseCountLabel;
QLabel *leftClickCountLabel;
QLabel *rightClickCountLabel;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , keyPressCount(0)
    , mouseClickCount(0)
    , leftClickCount(0)
    , rightClickCount(0)
    , today(QDate::currentDate())
    , settings(new QSettings("MyTool", "KeyMouseCounter"))
    , updateTimer(new QTimer(this))
    , hookManager(nullptr)
    , trayIcon(nullptr)
    , trayMenu(nullptr)
    , detailsDialog(nullptr)
{
    ui->setupUi(this);
    initUI();
    initHooks();
    initTray();
    loadStatistics();

    // 添加工具提示测试 [已注释]
    // testToolTip();

    // 设置更新计时器，每秒更新一次显示和托盘提示
    // 设置定时器在应用程序不活动时也能触发
    updateTimer->setTimerType(Qt::CoarseTimer);
    connect(updateTimer, &QTimer::timeout, this, [this]() {
        checkDateChange();
        updateDisplay();
        updateTrayTooltip();
        // 定期保存数据
        saveStatistics();
    });
    updateTimer->start(1000);

    // 设置窗口大小
    setFixedSize(300, 200);
    // 移除最大化按钮
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setWindowTitle("键鼠使用统计");
}

// 根据数值范围返回颜色样式
QString MainWindow::getColorStyle(int value)
{
    QString color;
    if (value <= 1000) {
        color = "#008000"; // 绿色
    } else if (value <= 3000) {
        color = "#0000FF"; // 蓝色
    } else if (value <= 5000) {
        color = "#FFFF00"; // 黄色
    } else if (value <= 10000) {
        color = "#FFA500"; // 橙色
    } else {
        color = "#FF0000"; // 红色
    }
    return QString("color: %1; font-weight: bold;").arg(color);
}

// 根据按键次数返回按键颜色（红色渐变，随次数增加透明度提高，颜色变深）
QString MainWindow::getKeyName(int keyCode)
{
    // 特殊按键映射
    QMap<int, QString> specialKeys;
    specialKeys[VK_ESCAPE] = "Esc";
    specialKeys[VK_F1] = "F1"; specialKeys[VK_F2] = "F2"; specialKeys[VK_F3] = "F3"; specialKeys[VK_F4] = "F4";
    specialKeys[VK_F5] = "F5"; specialKeys[VK_F6] = "F6"; specialKeys[VK_F7] = "F7"; specialKeys[VK_F8] = "F8";
    specialKeys[VK_F9] = "F9"; specialKeys[VK_F10] = "F10"; specialKeys[VK_F11] = "F11"; specialKeys[VK_F12] = "F12";
    specialKeys[VK_BACK] = "Backspace";
    specialKeys[VK_TAB] = "Tab";
    specialKeys[VK_RETURN] = "Enter";
    specialKeys[VK_LSHIFT] = "LShift";    specialKeys[VK_RSHIFT] = "RShift";
    specialKeys[VK_LCONTROL] = "LCtrl";
    specialKeys[VK_RCONTROL] = "RCtrl";
    specialKeys[VK_LMENU] = "LAlt";
    specialKeys[VK_RMENU] = "RAlt";
    specialKeys[VK_SPACE] = "Space";
    specialKeys[VK_CAPITAL] = "CapsLock";
    specialKeys[VK_NUMLOCK] = "NumLock";
    specialKeys[VK_SCROLL] = "ScrollLock";
    specialKeys[VK_INSERT] = "Insert";
    specialKeys[VK_DELETE] = "Delete";
    specialKeys[VK_HOME] = "Home";
    specialKeys[VK_END] = "End";
    specialKeys[VK_PRIOR] = "PageUp";
    specialKeys[VK_NEXT] = "PageDown";
    specialKeys[VK_UP] = "↑";
    specialKeys[VK_DOWN] = "↓";
    specialKeys[VK_LEFT] = "←";
    specialKeys[VK_RIGHT] = "→";
    specialKeys[VK_OEM_3] = "·";    specialKeys[VK_OEM_MINUS] = "-";    specialKeys[VK_OEM_PLUS] = "=";    specialKeys[VK_OEM_4] = "[";    specialKeys[0x5B] = "[";    specialKeys[VK_OEM_6] = "]";    specialKeys[VK_OEM_5] = "\\";    specialKeys[VK_OEM_1] = ";";    specialKeys[VK_OEM_7] = "'";    specialKeys[VK_OEM_COMMA] = ",";    specialKeys[VK_OEM_PERIOD] = ".";    specialKeys[VK_OEM_2] = "/";

    if (specialKeys.contains(keyCode)) {
        return specialKeys[keyCode];
    } else if (keyCode >= '0' && keyCode <= '9') {
        return QString(QChar(keyCode));
    } else if (keyCode >= 'A' && keyCode <= 'Z') {
        return QString(QChar(keyCode));
    } else if (keyCode >= VK_NUMPAD0 && keyCode <= VK_NUMPAD9) {
        return QString("Num") + QChar('0' + (keyCode - VK_NUMPAD0));
    } else if (keyCode >= VK_MULTIPLY && keyCode <= VK_DIVIDE) {
        switch (keyCode) {
        case VK_MULTIPLY: return QString("*");
        case VK_ADD: return QString("+");
        case VK_SUBTRACT: return QString("-");
        case VK_DIVIDE: return QString("/");
        default: return QString("0x") + QString::number(keyCode, 16).toUpper();
        }
    } else {
        return QString("0x") + QString::number(keyCode, 16).toUpper();
    }
}

    QString MainWindow::getKeyColor(int count){
    // 计算透明度 (0-255)，次数越多透明度越高
    int alpha = qMin(count * 255 / 10000, 255);

    // 计算红色分量，次数越多颜色越深（从红色到黑色）
    int red = 255 - qMin(count * 255 / 10000, 255);

    // 确保至少有一点红色
    red = qMax(red, 50);

    // 格式化为RGBA颜色
    return QString("rgba(%1, 0, 0, %2)").arg(red).arg(alpha / 255.0);
}

MainWindow::~MainWindow()
{
    saveStatistics();
    cleanupHooks();
    delete updateTimer;
    delete settings;
    delete ui;
    // 删除手动创建的UI组件
    delete dateLabel;
    delete keyCountLabel;
    delete mouseCountLabel;
    delete leftClickCountLabel;
    delete rightClickCountLabel;
    // 清理托盘资源
    delete trayMenu;
    delete trayIcon;
}

void MainWindow::showEvent(QShowEvent *event)
{
    // 检查是否跨天
    checkDateChange();
    // 加载统计数据
    loadStatistics();
    // 更新显示和托盘提示
    updateDisplay();
    updateTrayTooltip();
    // 确保钩子已安装
    if (!hookManager || !hookManager->installHooks()) {
        initHooks();
    }
    // 调用基类事件处理
    QMainWindow::showEvent(event);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // 创建退出选项对话框
    QMessageBox msgBox;
    msgBox.setWindowTitle("退出选项");
    msgBox.setText("请选择操作:");
    msgBox.setIcon(QMessageBox::Question);

    // 添加退出按钮
    QPushButton *exitButton = msgBox.addButton("退出程序", QMessageBox::ActionRole);
    // 添加最小化到托盘按钮
    QPushButton *minimizeButton = msgBox.addButton("最小化到托盘", QMessageBox::ActionRole);
    // 显示对话框并等待用户选择
    msgBox.exec();

    // 根据用户选择执行相应操作
    if (msgBox.clickedButton() == exitButton) {
        // 保存统计数据
        saveStatistics();
        // 清理钩子
        cleanupHooks();
        // 调用基类事件处理
        QMainWindow::closeEvent(event);
    } else if (msgBox.clickedButton() == minimizeButton) {
        // 最小化到托盘
        saveStatistics();
        hide();
        event->ignore();
    } else {
        // 取消关闭
        event->ignore();
    }
}

void MainWindow::updateDisplay()
{
    // 检查是否跨天
    checkDateChange();

    // 更新显示
    keyCountLabel->setText(QString("键盘按键次数: %1").arg(keyPressCount));
    mouseCountLabel->setText(QString("鼠标总点击次数: %1").arg(mouseClickCount));
    leftClickCountLabel->setText(QString("左键点击次数: %1").arg(leftClickCount));
    rightClickCountLabel->setText(QString("右键点击次数: %1").arg(rightClickCount));
    dateLabel->setText(QString("日期: %1").arg(today.toString("yyyy-MM-dd")));
}

void MainWindow::resetStatistics()
{
    // 确认重置
    if (QMessageBox::question(this, "确认重置", "确定要重置今日统计数据吗?") == QMessageBox::Yes) {
        // 重置总计数
        keyPressCount = 0;
        mouseClickCount = 0;
        leftClickCount = 0;
        rightClickCount = 0;

        // 重置每小时统计数据
        QString dateKey = today.toString("yyyy-MM-dd");
        for (int hour = 0; hour < 24; hour++) {
            settings->setValue(dateKey + "/hourlyKeyCount/" + QString::number(hour), 0);
            settings->setValue(dateKey + "/hourlyMouseCount/" + QString::number(hour), 0);
            settings->setValue(dateKey + "/hourlyLeftClickCount/" + QString::number(hour), 0);
            settings->setValue(dateKey + "/hourlyRightClickCount/" + QString::number(hour), 0);
        }

        // 保存重置后的数据
    saveStatistics();
    // 更新显示和托盘提示
    updateDisplay();
    updateTrayTooltip();
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, "关于", "键盘鼠标统计工具\n版本 1.5\nhttps://github.com/mangfufu/-KeyboardandMouseUsageCounter");

}

void MainWindow::onDetailsDialogClosed()
{
    // 清理详情对话框指针
    detailsDialog = nullptr;
}

void MainWindow::showDetailsDialog()
{
    // 检查详情对话框是否已存在
    if (detailsDialog != nullptr) {
        detailsDialog->raise();
        detailsDialog->activateWindow();
        return;
    }

    // 创建详情对话框
    detailsDialog = new QDialog(this);
    detailsDialog->setWindowTitle("数据详情");
    detailsDialog->resize(600, 800);

    // 连接对话框关闭信号到槽函数
    connect(detailsDialog, &QDialog::finished, this, &MainWindow::onDetailsDialogClosed);

    // 创建布局
    QVBoxLayout *mainLayout = new QVBoxLayout(detailsDialog);

    // 创建滚动区域
    QScrollArea *scrollArea = new QScrollArea(detailsDialog);
    scrollArea->setWidgetResizable(true);
    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

    // 创建一个容器来存储所有数据标签和行widget，方便后续更新
    QVector<QLabel*> keyLabels;
    QVector<QLabel*> mouseLabels;
    QVector<QLabel*> leftClickLabels;
    QVector<QLabel*> rightClickLabels;
    QVector<QWidget*> hourWidgets; // 存储每小时行的widget

    // 实时加载最新的每小时数据
    QString dateKey = today.toString("yyyy-MM-dd");
    QVector<int> hourlyKeyCounts(24, 0);
    QVector<int> hourlyMouseCounts(24, 0);
    QVector<int> hourlyLeftClickCounts(24, 0);
    QVector<int> hourlyRightClickCounts(24, 0);

    for (int hour = 0; hour < 24; hour++) {
        int keyCount = settings->value(dateKey + "/hourlyKeyCount/" + QString::number(hour), 0).toInt();
        int mouseCount = settings->value(dateKey + "/hourlyMouseCount/" + QString::number(hour), 0).toInt();
        int leftClickCount = settings->value(dateKey + "/hourlyLeftClickCount/" + QString::number(hour), 0).toInt();
        int rightClickCount = settings->value(dateKey + "/hourlyRightClickCount/" + QString::number(hour), 0).toInt();
        hourlyKeyCounts[hour] = keyCount;
        hourlyMouseCounts[hour] = mouseCount;
        hourlyLeftClickCounts[hour] = leftClickCount;
        hourlyRightClickCounts[hour] = rightClickCount;
    }

    // 创建标题标签
    QLabel *titleLabel = new QLabel("数据详情");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: black;");
    scrollLayout->addWidget(titleLabel);

    // 创建列标题
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *timeHeader = new QLabel("时间");
    timeHeader->setFixedWidth(100);
    timeHeader->setStyleSheet("font-weight: bold;");
    headerLayout->addWidget(timeHeader);

    QLabel *keyHeader = new QLabel("键盘");
    keyHeader->setFixedWidth(100);
    keyHeader->setStyleSheet("font-weight: bold;");
    headerLayout->addWidget(keyHeader);

    QLabel *mouseHeader = new QLabel("鼠标");
    mouseHeader->setFixedWidth(100);
    mouseHeader->setStyleSheet("font-weight: bold;");
    headerLayout->addWidget(mouseHeader);

    QLabel *leftClickHeader = new QLabel("左键");
    leftClickHeader->setFixedWidth(100);
    leftClickHeader->setStyleSheet("font-weight: bold;");
    headerLayout->addWidget(leftClickHeader);

    QLabel *rightClickHeader = new QLabel("右键");
    rightClickHeader->setFixedWidth(100);
    rightClickHeader->setStyleSheet("font-weight: bold;");
    headerLayout->addWidget(rightClickHeader);

    scrollLayout->addLayout(headerLayout);

    // 创建分隔线
    QFrame *line = new QFrame();
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    scrollLayout->addWidget(line);

    // 显示每小时数据
    int currentHour = QTime::currentTime().hour(); // 获取当前小时
    for (int hour = 0; hour < 24; hour++) {
        // 创建水平布局
        QHBoxLayout *hourLayout = new QHBoxLayout();

        // 为当前小时的行添加背景色
        QWidget *hourWidget = new QWidget();
        hourWidget->setLayout(hourLayout);
        hourWidgets.append(hourWidget); // 添加到widget容器
        if (hour == currentHour) {
            hourWidget->setStyleSheet("background-color:rgba(204, 237, 234, 0.82);"); // 淡绿色背景（行高亮）
        }

        // 时间标签
        QLabel *timeLabel = new QLabel(QString("%1:00-%1:59").arg(hour, 2, 10, QChar('0')));
        timeLabel->setFixedWidth(100);
        timeLabel->setStyleSheet("font-weight: bold;");
        hourLayout->addWidget(timeLabel);

        // 键盘数据标签
        QLabel *keyLabel = new QLabel();
        keyLabel->setFixedWidth(100);
        keyLabels.append(keyLabel);

        // 设置键盘数据颜色 (仅数字部分)
        QString keyText = QString("键盘: <span style='%1'>%2</span>次").arg(getColorStyle(hourlyKeyCounts[hour])).arg(hourlyKeyCounts[hour]);
        keyLabel->setText(keyText);
        keyLabel->setTextFormat(Qt::RichText);
        keyLabel->setStyleSheet("color: black;");
        hourLayout->addWidget(keyLabel);

        // 鼠标数据标签
        QLabel *mouseLabel = new QLabel();
        mouseLabel->setFixedWidth(100);
        mouseLabels.append(mouseLabel);

        // 设置鼠标总点击数据颜色 (仅数字部分)
        QString mouseText = QString("鼠标: <span style='%1'>%2</span>次").arg(getColorStyle(hourlyMouseCounts[hour])).arg(hourlyMouseCounts[hour]);
        mouseLabel->setText(mouseText);
        mouseLabel->setTextFormat(Qt::RichText);
        mouseLabel->setStyleSheet("color: black;");
        hourLayout->addWidget(mouseLabel);

        // 左键数据标签
        QLabel *leftClickLabel = new QLabel();
        leftClickLabel->setFixedWidth(100);
        leftClickLabels.append(leftClickLabel);

        // 设置左键点击数据颜色 (仅数字部分)
        QString leftClickText = QString("左键: <span style='%1'>%2</span>次").arg(getColorStyle(hourlyLeftClickCounts[hour])).arg(hourlyLeftClickCounts[hour]);
        leftClickLabel->setText(leftClickText);
        leftClickLabel->setTextFormat(Qt::RichText);
        leftClickLabel->setStyleSheet("color: black;");
        hourLayout->addWidget(leftClickLabel);

        // 右键数据标签
        QLabel *rightClickLabel = new QLabel();
        rightClickLabel->setFixedWidth(100);
        rightClickLabels.append(rightClickLabel);

        // 设置右键点击数据颜色 (仅数字部分)
        QString rightClickText = QString("右键: <span style='%1'>%2</span>次").arg(getColorStyle(hourlyRightClickCounts[hour])).arg(hourlyRightClickCounts[hour]);
        rightClickLabel->setText(rightClickText);
        rightClickLabel->setTextFormat(Qt::RichText);
        rightClickLabel->setStyleSheet("color: black;");
        hourLayout->addWidget(rightClickLabel);

        // 添加到滚动布局
        scrollLayout->addWidget(hourWidget);
    }

    // 设置滚动区域
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // 添加键盘布局界面
    QLabel *keyboardLayoutTitle = new QLabel("键盘按键统计");
    keyboardLayoutTitle->setAlignment(Qt::AlignCenter);
    keyboardLayoutTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: black;");
    mainLayout->addWidget(keyboardLayoutTitle);

    // 创建键盘布局容器
    QWidget *keyboardWidget = new QWidget();
    keyboardWidget->setStyleSheet("background-color: #f0f0f0; border-radius: 5px; padding: 10px;");
    QVBoxLayout *keyboardLayout = new QVBoxLayout(keyboardWidget);

    // 初始化按键代码到按钮的映射
    keyCodeToButton.clear();

    // 创建键盘行
    QVector<QHBoxLayout*> keyboardRows;
    for (int i = 0; i < 7; i++) {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(5);
        row->setAlignment(Qt::AlignCenter); // 使每行按键居中对齐
        keyboardRows.append(row);
        keyboardLayout->addLayout(row);
    }



    // 第一行：功能键
    QVector<int> row1Keys = {VK_ESCAPE, VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12};

    // 添加左侧空白间隔
    keyboardRows[0]->addSpacing(40);

    for (int keyCode : row1Keys) {
        QString keyName = this->getKeyName(keyCode);
        QPushButton *keyButton = new QPushButton(keyName, this);
        keyButton->setFixedSize(60, 30); // 功能键稍小
        keyButton->setFont(QFont("Arial", 8));
        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 3px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }")
                                 .arg(bgColor));
        // 应用悬停提示
        keyButton->setToolTip(toolTipText);

        keyboardRows[0]->addWidget(keyButton);
        keyCodeToButton[keyCode] = keyButton;
    }

    // 第二行：数字键
    QVector<int> row2Keys = {VK_OEM_3, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', VK_OEM_MINUS, VK_OEM_PLUS, VK_BACK};
    for (int keyCode : row2Keys) {
        QString keyName = this->getKeyName(keyCode);
        QPushButton *keyButton = new QPushButton(keyName, this);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));
        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }").arg(bgColor));
        // 应用悬停提示
        keyButton->setToolTip(toolTipText);

        keyboardRows[1]->addWidget(keyButton);
        keyCodeToButton[keyCode] = keyButton;
    }

    // 第三行：字母键 Q-W-E-R-T-Y
    QVector<int> row3Keys = {VK_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', VK_OEM_4, VK_OEM_6, VK_OEM_5};
    for (int keyCode : row3Keys) {
        QString keyName = this->getKeyName(keyCode);
        QPushButton *keyButton = new QPushButton(keyName, this);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));
        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }")
                                 .arg(bgColor));
        // 应用悬停提示
        keyButton->setToolTip(toolTipText);

        keyboardRows[2]->addWidget(keyButton);
        keyCodeToButton[keyCode] = keyButton;
    }

    // 第四行：字母键 A-S-D-F-G
    QVector<int> row4Keys = {VK_CAPITAL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', VK_OEM_1, VK_OEM_7, VK_RETURN};
    for (int keyCode : row4Keys) {
        QString keyName = this->getKeyName(keyCode);
        QPushButton *keyButton = new QPushButton(keyName, this);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));
        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }")
                                 .arg(bgColor));
        // 应用悬停提示
        keyButton->setToolTip(toolTipText);

        keyboardRows[3]->addWidget(keyButton);
        keyCodeToButton[keyCode] = keyButton;
    }

    // 第五行：字母键 Z-X-C-V-B
    QVector<int> row5Keys = {VK_LSHIFT, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_2, VK_RSHIFT};
    for (int keyCode : row5Keys) {
        QString keyName = this->getKeyName(keyCode);
        QPushButton *keyButton = new QPushButton(keyName, this);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));
        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }")
                                 .arg(bgColor));
        // 应用悬停提示
        keyButton->setToolTip(toolTipText);

        keyboardRows[4]->addWidget(keyButton);
        keyCodeToButton[keyCode] = keyButton;
    }

    // 第六行：空行
    keyboardRows[5]->addSpacing(35); // 保持行高一致

    // 第七行：特殊键和空格键
    QVector<int> row7Keys = {VK_LCONTROL, VK_LMENU, VK_SPACE, VK_RMENU, VK_RCONTROL};

    // 添加左侧空白间隔
    keyboardRows[6]->addSpacing(40);

    for (int i = 0; i < row7Keys.size(); i++) {
        int keyCode = row7Keys[i];
        QString keyName = this->getKeyName(keyCode);
        QPushButton *keyButton = new QPushButton(keyName, this);
        keyButton->setFont(QFont("Arial", 9));

        if (keyCode == VK_SPACE) {
            keyButton->setFixedSize(300, 35);  // 增大空格键
        } else if (keyCode == VK_CONTROL || keyCode == VK_MENU) {
            if (i == 0 || i == row7Keys.size() - 1) {
                keyButton->setFixedSize(100, 35);  // 左右两侧的Ctrl键稍大
            } else {
                keyButton->setFixedSize(85, 35);   // Alt键稍大
            }
        } else {
            keyButton->setFixedSize(65, 35);
        }

        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }")
                                 .arg(bgColor));
        // 应用悬停提示
        keyButton->setToolTip(toolTipText);


        keyboardRows[6]->addWidget(keyButton);

        // 在RControl后添加方向键
        if (i == row7Keys.size() - 1) {
            // 添加间隔
            keyboardRows[6]->addSpacing(15);

            // 创建方向键的垂直布局
            QVBoxLayout *directionLayout = new QVBoxLayout();
            directionLayout->setSpacing(5);
            directionLayout->setAlignment(Qt::AlignCenter);

            // 创建上键的水平布局（用于对齐下键）
            QHBoxLayout *upLayout = new QHBoxLayout();
            upLayout->setSpacing(5);
            upLayout->setAlignment(Qt::AlignCenter);

            // 上键
            QPushButton *upButton = new QPushButton(this->getKeyName(VK_UP), this);
            upButton->setFixedSize(65, 35);
            upButton->setFont(QFont("Arial", 9));
            int upCount = keyCounts.value(VK_UP, 0);
            upButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }").arg(getKeyColor(upCount)));
            upButton->setToolTip(QString("%1: %2次").arg(this->getKeyName(VK_UP)).arg(upCount));
            upLayout->addWidget(upButton);
            keyCodeToButton[VK_UP] = upButton;

            // 添加上键布局到垂直布局
            directionLayout->addLayout(upLayout);

            // 创建下键和左右键的水平布局
            QHBoxLayout *downAndArrowsLayout = new QHBoxLayout();
            downAndArrowsLayout->setSpacing(5);
            downAndArrowsLayout->setAlignment(Qt::AlignCenter);

            // 左键
            QPushButton *leftButton = new QPushButton(this->getKeyName(VK_LEFT), this);
            leftButton->setFixedSize(65, 35);
            leftButton->setFont(QFont("Arial", 9));
            int leftCount = keyCounts.value(VK_LEFT, 0);
            leftButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }").arg(getKeyColor(leftCount)));
            leftButton->setToolTip(QString("%1: %2次").arg(this->getKeyName(VK_LEFT)).arg(leftCount));
            downAndArrowsLayout->addWidget(leftButton);
            keyCodeToButton[VK_LEFT] = leftButton;

            // 下键
            QPushButton *downButton = new QPushButton(this->getKeyName(VK_DOWN), this);
            downButton->setFixedSize(65, 35);
            downButton->setFont(QFont("Arial", 9));
            int downCount = keyCounts.value(VK_DOWN, 0);
            downButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }").arg(getKeyColor(downCount)));
            downButton->setToolTip(QString("%1: %2次").arg(this->getKeyName(VK_DOWN)).arg(downCount));
            downAndArrowsLayout->addWidget(downButton);
            keyCodeToButton[VK_DOWN] = downButton;

            // 右键
            QPushButton *rightButton = new QPushButton(this->getKeyName(VK_RIGHT), this);
            rightButton->setFixedSize(65, 35);
            rightButton->setFont(QFont("Arial", 9));
            int rightCount = keyCounts.value(VK_RIGHT, 0);
            rightButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }").arg(getKeyColor(rightCount)));
            rightButton->setToolTip(QString("%1: %2次").arg(this->getKeyName(VK_RIGHT)).arg(rightCount));
            downAndArrowsLayout->addWidget(rightButton);
            keyCodeToButton[VK_RIGHT] = rightButton;

            // 添加水平布局到垂直布局
            directionLayout->addLayout(downAndArrowsLayout);

            // 将方向键布局添加到键盘行
            keyboardRows[6]->addLayout(directionLayout);
        }

        // 在中间的Alt键和空格键之间添加间隔
        if (i == 1) {
            keyboardRows[5]->addSpacing(10);
        }

        // 在空格键和右侧的Alt键之间添加间隔
        if (i == 2) {
            keyboardRows[5]->addSpacing(10);
        }

        keyCodeToButton[keyCode] = keyButton;
    }

    // 添加右侧空白间隔以使键盘居中
    keyboardRows[6]->addSpacing(40);

    // 添加键盘布局到主布局
    mainLayout->addWidget(keyboardWidget);

    // 创建关闭按钮
    QPushButton *closeButton = new QPushButton("关闭", detailsDialog);
    connect(closeButton, &QPushButton::clicked, detailsDialog, &QDialog::accept);
    mainLayout->addWidget(closeButton);

    // 获取数字颜色样式的辅助函数
    auto getNumberColorStyle = [](int count) {
        if (count >= 10001) {
            return "color: red;";
        } else if (count >= 5001) {
            return "color: orange;";
        } else if (count >= 3001) {
            return "color: yellow;";
        } else if (count >= 1001) {
            return "color: blue;";
        } else {
            return "color: green;";
        }
    };

    // 创建更新数据的函数，捕获this指针和需要的变量（使用值捕获而非引用捕获）
    auto updateDetailsData = [this, keyLabels, mouseLabels, leftClickLabels, rightClickLabels, hourWidgets, getNumberColorStyle]() {
        QString dateKey = QDate::currentDate().toString("yyyy-MM-dd");

        for (int hour = 0; hour < 24; hour++) {
            int keyCount = settings->value(dateKey + "/hourlyKeyCount/" + QString::number(hour), 0).toInt();
            int mouseCount = settings->value(dateKey + "/hourlyMouseCount/" + QString::number(hour), 0).toInt();
            int leftClickCount = settings->value(dateKey + "/hourlyLeftClickCount/" + QString::number(hour), 0).toInt();
            int rightClickCount = settings->value(dateKey + "/hourlyRightClickCount/" + QString::number(hour), 0).toInt();

            // 更新键盘数据标签
            QString keyText = QString::number(keyCount);
            QString keyColorStyle = getNumberColorStyle(keyCount);
            keyLabels[hour]->setText(QString("键盘: <span style='%1'>%2</span>次").arg(keyColorStyle).arg(keyText));
            keyLabels[hour]->setTextFormat(Qt::RichText);
            keyLabels[hour]->setStyleSheet("color: black;");

            // 更新当前小时行的背景色
            int currentHour = QTime::currentTime().hour(); // 获取当前小时
            if (hour == currentHour) {
                hourWidgets[hour]->setStyleSheet("background-color:rgba(204, 237, 234, 0.82);"); // 淡绿色背景（行高亮）
            } else {
                hourWidgets[hour]->setStyleSheet("background-color: transparent;");
            }


            // 更新鼠标总点击数据标签
            QString mouseText = QString::number(mouseCount);
            QString mouseColorStyle = getNumberColorStyle(mouseCount);
            mouseLabels[hour]->setText(QString("鼠标: <span style='%1'>%2</span>次").arg(mouseColorStyle).arg(mouseText));
            mouseLabels[hour]->setTextFormat(Qt::RichText);
            mouseLabels[hour]->setStyleSheet("color: black;");


            // 更新左键点击数据标签
            QString leftClickText = QString::number(leftClickCount);
            QString leftClickColorStyle = getNumberColorStyle(leftClickCount);
            leftClickLabels[hour]->setText(QString("左键: <span style='%1'>%2</span>次").arg(leftClickColorStyle).arg(leftClickText));
            leftClickLabels[hour]->setTextFormat(Qt::RichText);
            leftClickLabels[hour]->setStyleSheet("color: black;");


            // 更新右键点击数据标签
            QString rightClickText = QString::number(rightClickCount);
            QString rightClickColorStyle = getNumberColorStyle(rightClickCount);
            rightClickLabels[hour]->setText(QString("右键: <span style='%1'>%2</span>次").arg(rightClickColorStyle).arg(rightClickText));
            rightClickLabels[hour]->setTextFormat(Qt::RichText);
            rightClickLabels[hour]->setStyleSheet("color: black;");

        }

        // 更新键盘按键颜色和悬停提示
        for (auto it = keyCodeToButton.begin(); it != keyCodeToButton.end(); ++it) {
            int keyCode = it.key();
            QPushButton *button = it.value();
            int count = keyCounts.value(keyCode, 0);
            QString keyName = this->getKeyName(keyCode);
            QString bgColor = getKeyColor(count);
            // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
            button->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }")
                                 .arg(bgColor));

            // 设置悬停提示文本
            QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
            // 应用悬停提示
            button->setToolTip(toolTipText);
            // 验证提示文本是否被正确设置
            // qDebug() << "更新按键悬停提示 - 键名: " << keyName << ", 计数: " << count << ", 提示文本: " << toolTipText;
            // qDebug() << "按钮实际提示文本: " << button->toolTip();
            // // 调试信息：显示更新的悬停提示
            // qDebug() << "更新按键悬停提示 - 键名: " << keyName << ", 计数: " << count << ", 提示文本: " << toolTipText;
            // 确保按钮文本也使用正确的键名
            button->setText(keyName);
        }
    };

    // 初始加载数据
    updateDetailsData();

    // 添加调试信息
    // qDebug() << "keyCounts大小: " << keyCounts.size();
    // for (auto it = keyCounts.begin(); it != keyCounts.end(); ++it) {
    //     qDebug() << "按键代码: " << it.key() << "，按键名称: " << getKeyName(it.key()) << "，计数: " << it.value();
    // }

    // 创建定时器，每1秒更新一次数据
    QTimer *timer = new QTimer(detailsDialog);
    QObject::connect(timer, &QTimer::timeout, timer, [updateDetailsData]() {
        updateDetailsData();
    });
    timer->start(1000); // 5000毫秒 = 5秒

    // 设置对话框为非模态
    detailsDialog->setModal(false);
    // 显示对话框
    detailsDialog->show();

    // 连接对话框关闭信号到清理槽函数
    connect(detailsDialog, &QDialog::finished, this, [timer, this](int result) {
        Q_UNUSED(result);
        // 停止定时器并清理
        timer->stop();
        delete timer;
        // detailsDialog会在onDetailsDialogClosed槽函数中设置为nullptr
    });
}

void MainWindow::handleGlobalKeyPress()
{
    // 增加按键计数
    keyPressCount++;

    // 记录每小时数据
    int currentHour = QTime::currentTime().hour();
    QString dateKey = today.toString("yyyy-MM-dd");
    int hourlyKeyCount = settings->value(dateKey + "/hourlyKeyCount/" + QString::number(currentHour), 0).toInt();
    settings->setValue(dateKey + "/hourlyKeyCount/" + QString::number(currentHour), hourlyKeyCount + 1);
    // 更新显示和托盘提示
    updateDisplay();
    updateTrayTooltip();
    // 保存数据
    saveStatistics();
}

void MainWindow::handleSpecificKeyPress(int keyCode)
{
    // 增加具体按键计数
    keyCounts[keyCode]++;

    // 记录每小时按键数据
    int currentHour = QTime::currentTime().hour();
    QString dateKey = today.toString("yyyy-MM-dd");
    QString keyCodeStr = QString::number(keyCode);
    int hourlyKeyCount = settings->value(dateKey + "/hourlyKeyCounts/" + keyCodeStr + "/" + QString::number(currentHour), 0).toInt();
    settings->setValue(dateKey + "/hourlyKeyCounts/" + keyCodeStr + "/" + QString::number(currentHour), hourlyKeyCount + 1);

    // 更新对应按键的悬停提示
    if (keyCodeToButton.contains(keyCode)) {
        QPushButton *button = keyCodeToButton[keyCode];
        int count = keyCounts[keyCode];
        QString keyName = this->getKeyName(keyCode);
        button->setStyleSheet(QString("background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px;")
                             .arg(getKeyColor(count)));
        button->setToolTip(QString("%1: %2次")
                          .arg(keyName)
                          .arg(count));
        // 确保按钮文本也使用正确的键名
        button->setText(keyName);
    }

    // 保存数据
    saveStatistics();
}

void MainWindow::handleLeftButtonClick()
{
    // 增加左键点击计数
    leftClickCount++;
    // 增加总鼠标点击计数
    mouseClickCount++;

    // 记录每小时数据
    int currentHour = QTime::currentTime().hour();
    QString dateKey = today.toString("yyyy-MM-dd");
    int hourlyMouseCount = settings->value(dateKey + "/hourlyMouseCount/" + QString::number(currentHour), 0).toInt();
    settings->setValue(dateKey + "/hourlyMouseCount/" + QString::number(currentHour), hourlyMouseCount + 1);

    // 记录每小时左键数据
    int hourlyLeftClickCount = settings->value(dateKey + "/hourlyLeftClickCount/" + QString::number(currentHour), 0).toInt();
    settings->setValue(dateKey + "/hourlyLeftClickCount/" + QString::number(currentHour), hourlyLeftClickCount + 1);
    // 更新显示和托盘提示
    updateDisplay();
    updateTrayTooltip();
    // 保存数据
    saveStatistics();
}

void MainWindow::handleRightButtonClick()
{
    // 增加右键点击计数
    rightClickCount++;
    // 增加总鼠标点击计数
    mouseClickCount++;

    // 记录每小时数据
    int currentHour = QTime::currentTime().hour();
    QString dateKey = today.toString("yyyy-MM-dd");
    int hourlyMouseCount = settings->value(dateKey + "/hourlyMouseCount/" + QString::number(currentHour), 0).toInt();
    settings->setValue(dateKey + "/hourlyMouseCount/" + QString::number(currentHour), hourlyMouseCount + 1);

    // 记录每小时右键数据
    int hourlyRightClickCount = settings->value(dateKey + "/hourlyRightClickCount/" + QString::number(currentHour), 0).toInt();
    settings->setValue(dateKey + "/hourlyRightClickCount/" + QString::number(currentHour), hourlyRightClickCount + 1);
    settings->sync();

    // 更新显示和托盘提示
    updateDisplay();
    updateTrayTooltip();
    // 保存数据
    saveStatistics();
}

void MainWindow::loadStatistics()
{
    // 加载今日统计数据
    QString dateKey = today.toString("yyyy-MM-dd");
    keyPressCount = settings->value(dateKey + "/keyPressCount", 0).toInt();
    mouseClickCount = settings->value(dateKey + "/mouseClickCount", 0).toInt();
    leftClickCount = settings->value(dateKey + "/leftClickCount", 0).toInt();
    rightClickCount = settings->value(dateKey + "/rightClickCount", 0).toInt();

    // 调试信息：显示加载的总计数
    qDebug() << "加载统计数据 - 日期: " << dateKey;
    qDebug() << "按键总数: " << keyPressCount;
    qDebug() << "鼠标点击总数: " << mouseClickCount;

    // 加载具体按键计数
    keyCounts.clear();
    QString keyCountsGroup = dateKey + "/keyCounts/";
    QStringList allKeys = settings->allKeys();
    qDebug() << "设置中的键数量: " << allKeys.size();
    for (const QString &key : allKeys) {
        if (key.startsWith(keyCountsGroup)) {
            QString keyCodeStr = key.mid(keyCountsGroup.length());
            int keyCode = keyCodeStr.toInt();
            int count = settings->value(key, 0).toInt();
            keyCounts[keyCode] = count;
            qDebug() << "加载按键: " << keyCode << " (" << getKeyName(keyCode) << "), 计数: " << count;
        }
    }
    qDebug() << "keyCounts大小: " << keyCounts.size();
}

void MainWindow::saveStatistics()
{
    // 保存今日统计数据
    QString dateKey = today.toString("yyyy-MM-dd");
    settings->setValue(dateKey + "/keyPressCount", keyPressCount);
    settings->setValue(dateKey + "/mouseClickCount", mouseClickCount);
    settings->setValue(dateKey + "/leftClickCount", leftClickCount);
    settings->setValue(dateKey + "/rightClickCount", rightClickCount);

    // 保存具体按键计数
    QString keyCountsGroup = dateKey + "/keyCounts/";
    // 先删除旧数据
    QStringList allKeys = settings->allKeys();
    for (const QString &key : allKeys) {
        if (key.startsWith(keyCountsGroup)) {
            settings->remove(key);
        }
    }
    // 保存新数据
    for (auto it = keyCounts.begin(); it != keyCounts.end(); ++it) {
        int keyCode = it.key();
        int count = it.value();
        settings->setValue(keyCountsGroup + QString::number(keyCode), count);
    }

    settings->sync();
}

void MainWindow::checkDateChange()
{
    QDate currentDate = QDate::currentDate();
    if (currentDate != today) {
        // 导出昨日数据（不显示对话框）
        exportStatistics(false);
        // 保存昨日数据
        saveStatistics();
        // 更新日期
        today = currentDate;
        // 重置今日计数
        keyPressCount = 0;
        mouseClickCount = 0;
        leftClickCount = 0;
        rightClickCount = 0;
        // 加载今日数据（如果有）
        loadStatistics();
    }
}

void MainWindow::initUI()
{
    // 创建中心部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 创建日期标签
    dateLabel = new QLabel(this);
    mainLayout->addWidget(dateLabel);

    // 创建键盘计数标签
    keyCountLabel = new QLabel(this);
    mainLayout->addWidget(keyCountLabel);

    // 创建鼠标计数标签
    mouseCountLabel = new QLabel(this);
    mainLayout->addWidget(mouseCountLabel);

    // 创建左键计数标签
    leftClickCountLabel = new QLabel(this);
    mainLayout->addWidget(leftClickCountLabel);

    // 创建右键计数标签
    rightClickCountLabel = new QLabel(this);
    mainLayout->addWidget(rightClickCountLabel);

    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    // 创建重置按钮
    QPushButton *resetButton = new QPushButton("重置", this);
    connect(resetButton, &QPushButton::clicked, this, &MainWindow::resetStatistics);
    buttonLayout->addWidget(resetButton);

    // 创建关于按钮
    QPushButton *aboutButton = new QPushButton("关于", this);
    connect(aboutButton, &QPushButton::clicked, this, &MainWindow::showAboutDialog);
    buttonLayout->addWidget(aboutButton);

    // 创建详情按钮
    QPushButton *detailsButton = new QPushButton("详情", this);
    connect(detailsButton, &QPushButton::clicked, this, &MainWindow::showDetailsDialog);
    buttonLayout->addWidget(detailsButton);

    // 创建导出按钮
    QPushButton *exportButton = new QPushButton("导出", this);
    connect(exportButton, &QPushButton::clicked, this, [this]() { exportStatistics(true); });
    buttonLayout->addWidget(exportButton);

    // 添加按钮布局到主布局
    mainLayout->addLayout(buttonLayout);

    // 设置样式
    QString styleSheet = ""
        "QLabel { font-size: 14px; color: #333; }"
        "QPushButton { background-color: rgb(172, 209, 245); color: black; border-radius: 3px; padding: 5px; font-weight: default; border: 1px solid rgb(172, 209, 245); }"
        "QPushButton:hover { background-color:rgb(186, 232, 255); }"
        "QWidget { background-color: #f5f5f5; }"
        "QToolTip { background-color: white; color: black; border: 1px solid #ccc; padding: 3px; font-size: 12px; }"
        // 拟态化滚动条样式
        "QScrollBar:vertical {"
            "width: 12px;"
            "margin: 5px 0 5px 0;"
            "background: rgba(204, 237, 234, 0.82);"
            "border-radius: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
            "background: rgba(204, 237, 234, 0.82);"
            "border-radius: 0px;"
            "min-height: 20px;"
        "}"
        "QScrollBar::handle:vertical:hover {"
            "background:rgba(0, 200, 255, 0.27);"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
            "background: transparent;"
            "height: 3px;"
        "}"
        "QScrollBar:horizontal {"
            "height: 12px;"
            "margin: 0 5px 0 5px;"
            "background:rgba(204, 237, 234, 0.82);"
            "border-radius: 0px;"
        "}"
        "QScrollBar::handle:horizontal {"
            "background:rgba(204, 237, 234, 0.82);"
            "border-radius: 0px;"
            "min-width: 20px;"
        "}"
        "QScrollBar::handle:horizontal:hover {"
            "background:rgba(0, 200, 255, 0.27);"
        "}"
        "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
            "background: transparent;"
            "width: 3px;"
        "}";
    setStyleSheet(styleSheet);
}

void MainWindow::initHooks()
{
    // 清理已有的钩子
    cleanupHooks();

    // 创建钩子管理器
    hookManager = new GlobalHookManager(this);

    // 连接信号和槽
    connect(hookManager, &GlobalHookManager::keyPressed, this, &MainWindow::handleGlobalKeyPress);
    connect(hookManager, &GlobalHookManager::specificKeyPressed, this, &MainWindow::handleSpecificKeyPress);
    connect(hookManager, &GlobalHookManager::leftButtonClicked, this, &MainWindow::handleLeftButtonClick);
    connect(hookManager, &GlobalHookManager::rightButtonClicked, this, &MainWindow::handleRightButtonClick);

    // 安装钩子
    if (!hookManager->installHooks()) {
        QMessageBox::warning(this, "警告", "无法安装全局钩子，程序将只能统计窗口内的操作。");
        delete hookManager;
        hookManager = nullptr;
    }
}

void MainWindow::cleanupHooks()
{
    if (hookManager) {
        hookManager->uninstallHooks();
        delete hookManager;
        hookManager = nullptr;
    }
}

void MainWindow::initTray()
{
    // 检查系统是否支持托盘
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        QMessageBox::information(this, "提示", "系统不支持托盘功能");
        return;
    }

    // 创建托盘图标
    trayIcon = new QSystemTrayIcon(this);
    // 设置自定义图标（使用Qt资源系统）
    QIcon icon(":/myicon.ico");
    if (icon.isNull()) {
        trayIcon->setIcon(QApplication::style()->standardIcon(QStyle::SP_TitleBarMenuButton));
    } else {
        trayIcon->setIcon(icon);
    }

    // 创建托盘菜单
    trayMenu = new QMenu(this);

    // 创建"打开"菜单项
    QAction *openAction = new QAction("打开窗口", this);
    connect(openAction, &QAction::triggered, this, &MainWindow::showMainWindow);
    trayMenu->addAction(openAction);

    // 创建"退出"菜单项
    QAction *exitAction = new QAction("退出程序", this);
    connect(exitAction, &QAction::triggered, qApp, &QApplication::quit);
    trayMenu->addAction(exitAction);

    // 设置托盘菜单
    trayIcon->setContextMenu(trayMenu);

    // 连接双击事件
    connect(trayIcon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) {
            showMainWindow();
        }
    });

    // 显示托盘图标
    trayIcon->show();

    // 初始更新托盘提示
    updateTrayTooltip();
}

void MainWindow::updateTrayTooltip()
{
    // 更新托盘提示信息
    QString tooltip = QString("键盘鼠标统计\n日期: %1\n键盘按键: %2次\n鼠标点击: %3次")
                      .arg(today.toString("yyyy-MM-dd"))
                      .arg(keyPressCount)
                      .arg(mouseClickCount);
    trayIcon->setToolTip(tooltip);
}

// 测试QToolTip功能的辅助函数 [已注释]
/*
void MainWindow::testToolTip() {
    QPushButton *testButton = new QPushButton("测试提示", this);
    testButton->setGeometry(10, 10, 100, 30);
    testButton->setStyleSheet("QPushButton { background-color: yellow; } QToolTip { background-color: pink !important; color: green !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }");
    testButton->setToolTip("这是一个测试提示");
    testButton->show();
    qDebug() << "测试按钮提示文本: " << testButton->toolTip();
}*/


void MainWindow::showMainWindow()
{
    // 显示并激活主窗口
    show();
    activateWindow();

    // 检查日期变化
    checkDateChange();
    // 直接更新显示数据（使用内存中的最新数据）
    updateDisplay();
    updateTrayTooltip();
    // 保存数据
    saveStatistics();
}

void MainWindow::exportStatistics(bool showDialog)
{
    // 根据参数决定是否显示确认对话框
    if (showDialog) {
        if (QMessageBox::question(this, "确认导出", "确定要导出统计数据吗？") != QMessageBox::Yes) {
            return; // 用户选择取消，不执行导出
        }
    }

    // 获取当前日期时间用于创建文件夹和文件名
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString datetimeFormat = "yyyy-MM-dd-hh_mm_ss";
    QString datetimeStr = currentDateTime.toString(datetimeFormat);
    QString dateStr = currentDateTime.toString("yyyy-MM-dd");

    // 创建文件夹路径 - 使用QDir::separator()确保跨平台兼容性
    QString appPath = QCoreApplication::applicationDirPath();
    QString outputDirPath = appPath + QDir::separator() + "output" + QDir::separator() + dateStr + QDir::separator();
    QDir outputDir;
    if (!outputDir.mkpath(outputDirPath)) {
        if (showDialog) {
            QMessageBox::critical(this, "错误", "无法创建输出文件夹: " + outputDirPath + "\n请检查目录权限。");
        }
        qDebug() << "无法创建输出文件夹: " << outputDirPath;
        return;
    }

    // 创建文件路径
    QString fileName;
    if (showDialog) {
        fileName = datetimeStr + ".txt";
    } else {
        fileName = "Autosave_" + datetimeStr + ".txt";
    }
    QString filePath = outputDirPath + fileName;
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (showDialog) {
            QMessageBox::critical(this, "错误", "无法创建输出文件: " + filePath + "\n错误: " + file.errorString());
        }
        qDebug() << "无法创建输出文件: " << filePath << " 错误: " << file.errorString();
        return;
    }

    // 写入数据
    QTextStream out(&file);
    out << "=== 键盘鼠标统计数据 ===\n";
    out << "导出时间: " << currentDateTime.toString("yyyy-MM-dd-hh_mm_ss") << "\n";
    out << "统计日期: " << dateStr << "\n\n";

    // 写入总览数据
    out << "=== 总览数据 ===\n";
    out << "键盘按键次数: " << keyPressCount << "次\n";
    out << "鼠标总点击次数: " << mouseClickCount << "次\n";
    out << "左键点击次数: " << leftClickCount << "次\n";
    out << "右键点击次数: " << rightClickCount << "次\n\n";

    // 写入每小时数据
    out << "=== 每小时数据 ===\n";

    for (int hour = 0; hour < 24; hour++) {
        int keyCount = settings->value(dateStr + "/hourlyKeyCount/" + QString::number(hour), 0).toInt();
        int mouseCount = settings->value(dateStr + "/hourlyMouseCount/" + QString::number(hour), 0).toInt();
        int leftClickCount = settings->value(dateStr + "/hourlyLeftClickCount/" + QString::number(hour), 0).toInt();
        int rightClickCount = settings->value(dateStr + "/hourlyRightClickCount/" + QString::number(hour), 0).toInt();

        out << QString("%1:00-%1:59     \n键盘次数:    %2    次      \n鼠标次数:    %3    次      左: %4   次      右: %5 次  \n")
               .arg(hour, 2, 10, QChar('0'))
               .arg(keyCount)
               .arg(mouseCount)
               .arg(leftClickCount)
               .arg(rightClickCount) << "\n";
    }

    // 写入键盘按键详细数据
    out << "\n=== 键盘按键详细数据 ===\n";

    // 对按键数据进行排序（按按键次数降序）
    QList<QPair<int, int>> sortedKeys;
    for (auto it = keyCounts.begin(); it != keyCounts.end(); ++it) {
        sortedKeys.append(qMakePair(it.key(), it.value()));
    }

    std::sort(sortedKeys.begin(), sortedKeys.end(), [](const QPair<int, int>& a, const QPair<int, int>& b) {
        return a.second > b.second; // 按次数降序排序
    });

    // 写入排序后的按键数据
    for (const auto& pair : sortedKeys) {
        int keyCode = pair.first;
        int count = pair.second;
        QString keyName = getKeyName(keyCode);

        out << QString("\n按键 %1   :   %2    次")
               .arg(keyName)
               .arg(count) << "\n";
    }

    // 关闭文件
    file.close();

    // 显示成功消息
    QMessageBox::information(this, "成功", "数据已成功导出到:\n" + filePath);
}
