#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QShowEvent>
#include <QCloseEvent>
#include <QSettings>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QDate>
#include <QApplication>
#include <QCoreApplication>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QProgressDialog>
#include <QProcess>
#include <QCryptographicHash>
#include <QProcess>
#include <QProgressDialog>
#include <QIcon>
#include <QStyle>
#include <QTime>
#include <QDialog>
#include <QScrollArea>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QFileDialog>
#include <QGroupBox>
#include <QVector>
#include <QFrame>
#include <QButtonGroup>
#include <QRadioButton>
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
    , autoSaveTimer(new QTimer(this))
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

    // 设置自动保存计时器，每天23:59:59触发
    autoSaveTimer->setTimerType(Qt::CoarseTimer);
    connect(autoSaveTimer, &QTimer::timeout, this, &MainWindow::autoSaveData);

    // 计算当前时间到23:59:59的毫秒数
    QTime now = QTime::currentTime();
    int msecsToMidnight = now.msecsTo(QTime(23, 59, 59));
    if (msecsToMidnight <= 0) {
        // 如果当前时间已过23:59:59，则设置明天触发
        msecsToMidnight += 24 * 60 * 60 * 1000;
    }
    autoSaveTimer->start(msecsToMidnight);

    // 设置窗口大小
    setFixedSize(300, 200);
    // 移除最大化按钮
    setWindowFlags(windowFlags() & ~Qt::WindowMaximizeButtonHint);
    setWindowTitle("键鼠使用统计");
}

// 自动保存数据
void MainWindow::autoSaveData()
{
    // 静默导出键盘布局图片
    exportKeyboardLayoutAsImage(false);

    // 静默导出统计数据
    exportStatistics(false);

    // 重新设置计时器，明天同一时间触发
    int msecsToTomorrow = 24 * 60 * 60 * 1000;
    autoSaveTimer->start(msecsToTomorrow);
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
    // 设置标准按钮为取消按钮
    msgBox.setStandardButtons(QMessageBox::Cancel);
    // 设置取消按钮文本
    msgBox.button(QMessageBox::Cancel)->setText("取消");

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
        // 点击×按钮或其他情况，只关闭对话框
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
    QDialog dialog(this);
    dialog.setWindowTitle("关于");
    dialog.setFixedSize(300, 200);

    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *titleLabel = new QLabel("键盘鼠标统计工具");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");

    QLabel *versionLabel = new QLabel(QString("版本 %1").arg(APP_VERSION));
    versionLabel->setAlignment(Qt::AlignCenter);

    QLabel *urlLabel = new QLabel("<a href='https://github.com/mangfufu/-KeyboardandMouseUsageCounter'>GitHub 仓库</a>");
    urlLabel->setAlignment(Qt::AlignCenter);
    urlLabel->setOpenExternalLinks(true);

    QPushButton *checkUpdateButton = new QPushButton("检查更新");
    checkUpdateButton->setFixedWidth(100);
    connect(checkUpdateButton, &QPushButton::clicked, this, &MainWindow::checkForUpdates);

    layout->addSpacing(20);
    layout->addWidget(titleLabel);
    layout->addWidget(versionLabel);
    layout->addSpacing(10);
    layout->addWidget(urlLabel);
    layout->addStretch();
    layout->addWidget(checkUpdateButton, 0, Qt::AlignCenter);
    layout->addSpacing(20);

    dialog.exec();
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

    // 清空成员变量容器
    timeLabels.clear();
    keyLabels.clear();
    mouseLabels.clear();
    leftClickLabels.clear();
    rightClickLabels.clear();
    hourWidgets.clear();

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
    detailsTitleLabel = new QLabel("数据详情");
    detailsTitleLabel->setAlignment(Qt::AlignCenter);
    if (isDarkMode) {
        detailsTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: lightgray;");
    } else {
        detailsTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: black;");
    }
    scrollLayout->addWidget(detailsTitleLabel);

    // 创建列标题
    QHBoxLayout *headerLayout = new QHBoxLayout();
    QLabel *timeHeader = new QLabel("时间");
    timeHeader->setFixedWidth(100);
    if (isDarkMode) {
        timeHeader->setStyleSheet("font-weight: bold; color: lightgray;");
    } else {
        timeHeader->setStyleSheet("font-weight: bold; color: black;");
    }
    headerLayout->addWidget(timeHeader);

    QLabel *keyHeader = new QLabel("键盘");
    keyHeader->setFixedWidth(100);
    if (isDarkMode) {
        keyHeader->setStyleSheet("font-weight: bold; color: lightgray;");
    } else {
        keyHeader->setStyleSheet("font-weight: bold; color: black;");
    }
    headerLayout->addWidget(keyHeader);

    QLabel *mouseHeader = new QLabel("鼠标");
    mouseHeader->setFixedWidth(100);
    if (isDarkMode) {
        mouseHeader->setStyleSheet("font-weight: bold; color: lightgray;");
    } else {
        mouseHeader->setStyleSheet("font-weight: bold; color: black;");
    }
    headerLayout->addWidget(mouseHeader);

    QLabel *leftClickHeader = new QLabel("左键");
    leftClickHeader->setFixedWidth(100);
    if (isDarkMode) {
        leftClickHeader->setStyleSheet("font-weight: bold; color: lightgray;");
    } else {
        leftClickHeader->setStyleSheet("font-weight: bold; color: black;");
    }
    headerLayout->addWidget(leftClickHeader);

    QLabel *rightClickHeader = new QLabel("右键");
    rightClickHeader->setFixedWidth(100);
    if (isDarkMode) {
        rightClickHeader->setStyleSheet("font-weight: bold; color: lightgray;");
    } else {
        rightClickHeader->setStyleSheet("font-weight: bold; color: black;");
    }
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
            if (isDarkMode) {
                hourWidget->setStyleSheet("background-color:rgba(80, 80, 80, 1);"); // 亮灰色背景（行高亮）
            } else {
                hourWidget->setStyleSheet("background-color:rgba(204, 237, 234, 0.82);"); // 淡绿色背景（行高亮）
            }
        }

        // 时间标签
        QLabel *timeLabel = new QLabel(QString("%1:00-%1:59").arg(hour, 2, 10, QChar('0')));
        timeLabel->setFixedWidth(100);
        if (isDarkMode) {
            timeLabel->setStyleSheet("font-weight: bold; color: lightgray;");
        } else {
            timeLabel->setStyleSheet("font-weight: bold; color: black;");
        }
        hourLayout->addWidget(timeLabel);
        timeLabels.append(timeLabel);

        // 键盘数据标签
        QLabel *keyLabel = new QLabel();
        keyLabel->setFixedWidth(100);
        this->keyLabels.append(keyLabel);

        // 设置键盘数据颜色 (仅数字部分)
        QString keyText = QString("键盘: <span style='%1'>%2</span>次").arg(getColorStyle(hourlyKeyCounts[hour])).arg(hourlyKeyCounts[hour]);
        keyLabel->setText(keyText);
        keyLabel->setTextFormat(Qt::RichText);
        if (isDarkMode) {
            keyLabel->setStyleSheet("color: white;");
        } else {
            keyLabel->setStyleSheet("color: black;");
        }
        hourLayout->addWidget(keyLabel);

        // 鼠标数据标签
        QLabel *mouseLabel = new QLabel();
        mouseLabel->setFixedWidth(100);
        this->mouseLabels.append(mouseLabel);

        // 设置鼠标总点击数据颜色 (仅数字部分)
        QString mouseText = QString("鼠标: <span style='%1'>%2</span>次").arg(getColorStyle(hourlyMouseCounts[hour])).arg(hourlyMouseCounts[hour]);
        mouseLabel->setText(mouseText);
        mouseLabel->setTextFormat(Qt::RichText);
        if (isDarkMode) {
            mouseLabel->setStyleSheet("color: lightgray;");
        } else {
            mouseLabel->setStyleSheet("color: black;");
        }
        hourLayout->addWidget(mouseLabel);

        // 左键数据标签
        QLabel *leftClickLabel = new QLabel();
        leftClickLabel->setFixedWidth(100);
        this->leftClickLabels.append(leftClickLabel);

        // 设置左键点击数据颜色 (仅数字部分)
        QString leftClickText = QString("左键: <span style='%1'>%2</span>次").arg(getColorStyle(hourlyLeftClickCounts[hour])).arg(hourlyLeftClickCounts[hour]);
        leftClickLabel->setText(leftClickText);
        leftClickLabel->setTextFormat(Qt::RichText);
        if (isDarkMode) {
            leftClickLabel->setStyleSheet("color: lightgray;");
        } else {
            leftClickLabel->setStyleSheet("color: black;");
        }
        hourLayout->addWidget(leftClickLabel);

        // 右键数据标签
        QLabel *rightClickLabel = new QLabel();
        rightClickLabel->setFixedWidth(100);
        this->rightClickLabels.append(rightClickLabel);

        // 设置右键点击数据颜色 (仅数字部分)
        QString rightClickText = QString("右键: <span style='%1'>%2</span>次").arg(getColorStyle(hourlyRightClickCounts[hour])).arg(hourlyRightClickCounts[hour]);
        rightClickLabel->setText(rightClickText);
        rightClickLabel->setTextFormat(Qt::RichText);
        if (isDarkMode) {
            rightClickLabel->setStyleSheet("color: lightgray;");
        } else {
            rightClickLabel->setStyleSheet("color: black;");
        }
        hourLayout->addWidget(rightClickLabel);

        // 添加到滚动布局
        scrollLayout->addWidget(hourWidget);
    }

    // 设置滚动区域
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

    // 添加键盘布局界面
    keyboardLayoutTitleLabel = new QLabel("键盘按键统计");
    keyboardLayoutTitleLabel->setAlignment(Qt::AlignCenter);
    if (isDarkMode) {
        keyboardLayoutTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: lightgray;");
    } else {
        keyboardLayoutTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: black;");
    }
    mainLayout->addWidget(keyboardLayoutTitleLabel);

    // 创建键盘布局容器
    keyboardWidget = new QWidget();
    if (isDarkMode) {
        keyboardWidget->setStyleSheet("background-color: #333333; border-radius: 5px; padding: 10px;");
    } else {
        keyboardWidget->setStyleSheet("background-color: #f0f0f0; border-radius: 5px; padding: 10px;");
    }
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
        QPushButton *keyButton = new QPushButton(keyName, detailsDialog);
        keyButton->setFixedSize(60, 30); // 功能键稍小
        keyButton->setFont(QFont("Arial", 8));
        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        if (isDarkMode) {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 3px; } "
                    "QPushButton:hover { background-color: #555; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            } else {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 3px; } "
                    "QPushButton:hover { background-color: #e0e0e0; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            }
        // 应用悬停提示
        keyButton->setToolTip(toolTipText);

        keyboardRows[0]->addWidget(keyButton);
        keyCodeToButton[keyCode] = keyButton;
    }

    // 第二行：数字键
    QVector<int> row2Keys = {VK_OEM_3, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', VK_OEM_MINUS, VK_OEM_PLUS, VK_BACK};
    for (int keyCode : row2Keys) {
        QString keyName = this->getKeyName(keyCode);
        QPushButton *keyButton = new QPushButton(keyName, detailsDialog);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));
        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        if (isDarkMode) {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; padding: 5px; } "
                    "QPushButton:hover { background-color: #555; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            } else {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black; border-radius: 3px; padding: 5px; } "
                    "QPushButton:hover { background-color: #e0e0e0; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            }
        // 应用悬停提示
        keyButton->setToolTip(toolTipText);

        keyboardRows[1]->addWidget(keyButton);
        keyCodeToButton[keyCode] = keyButton;
    }

    // 第三行：字母键 Q-W-E-R-T-Y
    QVector<int> row3Keys = {VK_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', VK_OEM_4, VK_OEM_6, VK_OEM_5};
    for (int keyCode : row3Keys) {
        QString keyName = this->getKeyName(keyCode);
        QPushButton *keyButton = new QPushButton(keyName, detailsDialog);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));
        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        // 根据isDarkMode设置按键样式
        if (isDarkMode) {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } "
                    "QPushButton:hover { background-color: #555; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            } else {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } "
                    "QPushButton:hover { background-color: #e0e0e0; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            }
        
        // 应用悬停提示
        keyButton->setToolTip(toolTipText);

        keyboardRows[2]->addWidget(keyButton);
        keyCodeToButton[keyCode] = keyButton;
    }

    // 第四行：字母键 A-S-D-F-G
    QVector<int> row4Keys = {VK_CAPITAL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', VK_OEM_1, VK_OEM_7, VK_RETURN};
    for (int keyCode : row4Keys) {
        QString keyName = this->getKeyName(keyCode);
        QPushButton *keyButton = new QPushButton(keyName, detailsDialog);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));
        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        // 根据isDarkMode设置按键样式
        if (isDarkMode) {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } "
                    "QPushButton:hover { background-color: #555; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            } else {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } "
                    "QPushButton:hover { background-color: #e0e0e0; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            }
        // 应用悬停提示
        keyButton->setToolTip(toolTipText);

        keyboardRows[3]->addWidget(keyButton);
        keyCodeToButton[keyCode] = keyButton;
    }

    // 第五行：字母键 Z-X-C-V-B
    QVector<int> row5Keys = {VK_LSHIFT, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_2, VK_RSHIFT};
    for (int keyCode : row5Keys) {
        QString keyName = this->getKeyName(keyCode);
        QPushButton *keyButton = new QPushButton(keyName, detailsDialog);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));
        // 设置按键颜色和悬停提示
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        // 设置悬停提示文本
        QString toolTipText = QString("%1: %2次").arg(keyName).arg(count);
        // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
        // 根据isDarkMode设置按键样式
        if (isDarkMode) {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } "
                    "QPushButton:hover { background-color: #555; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            } else {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } "
                    "QPushButton:hover { background-color: #e0e0e0; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            }
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
        QPushButton *keyButton = new QPushButton(keyName, detailsDialog);
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
        // 根据isDarkMode设置按键样式
        if (isDarkMode) {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } "
                    "QPushButton:hover { background-color: #555; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            } else {
                keyButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } "
                    "QPushButton:hover { background-color: #e0e0e0; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            }
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
            QPushButton *upButton = new QPushButton(this->getKeyName(VK_UP), detailsDialog);
            upButton->setFixedSize(65, 35);
            upButton->setFont(QFont("Arial", 9));
            int upCount = keyCounts.value(VK_UP, 0);
            // 根据isDarkMode设置按键样式
            if (isDarkMode) {
                upButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } "
                    "QPushButton:hover { background-color: #555; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(getKeyColor(upCount)));
            } else {
                upButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } "
                    "QPushButton:hover { background-color: #e0e0e0; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(getKeyColor(upCount)));
            }
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
            QPushButton *leftButton = new QPushButton(this->getKeyName(VK_LEFT), detailsDialog);
            leftButton->setFixedSize(65, 35);
            leftButton->setFont(QFont("Arial", 9));
            int leftCount = keyCounts.value(VK_LEFT, 0);
            // 根据isDarkMode设置按键样式
            if (isDarkMode) {
                leftButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } "
                    "QPushButton:hover { background-color: #555; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(getKeyColor(leftCount)));
            } else {
                leftButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } "
                    "QPushButton:hover { background-color: #e0e0e0; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(getKeyColor(leftCount)));
            }
            leftButton->setToolTip(QString("%1: %2次").arg(this->getKeyName(VK_LEFT)).arg(leftCount));
            downAndArrowsLayout->addWidget(leftButton);
            keyCodeToButton[VK_LEFT] = leftButton;

            // 下键
            QPushButton *downButton = new QPushButton(this->getKeyName(VK_DOWN), detailsDialog);
            downButton->setFixedSize(65, 35);
            downButton->setFont(QFont("Arial", 9));
            int downCount = keyCounts.value(VK_DOWN, 0);
            // 根据isDarkMode设置按键样式
            if (isDarkMode) {
                downButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } "
                    "QPushButton:hover { background-color: #555; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(getKeyColor(downCount)));
            } else {
                downButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } "
                    "QPushButton:hover { background-color: #e0e0e0; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(getKeyColor(downCount)));
            }
            downButton->setToolTip(QString("%1: %2次").arg(this->getKeyName(VK_DOWN)).arg(downCount));
            downAndArrowsLayout->addWidget(downButton);
            keyCodeToButton[VK_DOWN] = downButton;

            // 右键
            QPushButton *rightButton = new QPushButton(this->getKeyName(VK_RIGHT), detailsDialog);
            rightButton->setFixedSize(65, 35);
            rightButton->setFont(QFont("Arial", 9));
            int rightCount = keyCounts.value(VK_RIGHT, 0);
            // 根据isDarkMode设置按键样式
            if (isDarkMode) {
                rightButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } "
                    "QPushButton:hover { background-color: #555; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(getKeyColor(rightCount)));
            } else {
                rightButton->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } "
                    "QPushButton:hover { background-color: #e0e0e0; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(getKeyColor(rightCount)));
            }
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

    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    // 创建关闭按钮
    QPushButton *closeButton = new QPushButton("关闭", detailsDialog);
    connect(closeButton, &QPushButton::clicked, detailsDialog, &QDialog::accept);
    buttonLayout->addWidget(closeButton);

    // 删除导出为图片按钮，已在主界面统一导出功能
    // QPushButton *exportImageButton = new QPushButton("导出为图片", detailsDialog);
    // connect(exportImageButton, &QPushButton::clicked, this, [this]() {
    //     this->exportDetailsAsImage(this->detailsDialog);
    // });
    // buttonLayout->addWidget(exportImageButton);

    mainLayout->addLayout(buttonLayout);

    // 获取数字颜色样式的辅助函数
    auto getNumberColorStyle = [](int count) {
        if (count >= 10001) {
            return "color: red;";
        } else if (count >= 5001) {
            return "color: orange;";
        } else if (count >= 3001) {
            return "color: purple;";
        } else if (count >= 1001) {
            return "color: blue;";
        } else {
            return "color: green;";
        }
    };

    // 创建更新数据的函数，捕获this指针和需要的变量（使用值捕获而非引用捕获）
    auto updateDetailsData = [this, getNumberColorStyle]() {
        QString dateKey = QDate::currentDate().toString("yyyy-MM-dd");

        for (int hour = 0; hour < 24; hour++) {
            int keyCount = settings->value(dateKey + "/hourlyKeyCount/" + QString::number(hour), 0).toInt();
            int mouseCount = settings->value(dateKey + "/hourlyMouseCount/" + QString::number(hour), 0).toInt();
            int leftClickCount = settings->value(dateKey + "/hourlyLeftClickCount/" + QString::number(hour), 0).toInt();
            int rightClickCount = settings->value(dateKey + "/hourlyRightClickCount/" + QString::number(hour), 0).toInt();

            // 更新键盘数据标签
            QString keyText = QString::number(keyCount);
            QString keyColorStyle = getNumberColorStyle(keyCount);
            this->keyLabels[hour]->setText(QString("键盘: <span style='%1'>%2</span>次").arg(keyColorStyle).arg(keyText));
            this->keyLabels[hour]->setTextFormat(Qt::RichText);
            // 根据主题模式设置文字颜色
            this->keyLabels[hour]->setStyleSheet(this->isDarkMode ? "color: lightgray;" : "color: black;");

            // 更新当前小时行的背景色
            int currentHour = QTime::currentTime().hour(); // 获取当前小时
            if (hour == currentHour) {
                if (this->isDarkMode)
                {
                    this->hourWidgets[hour]->setStyleSheet("background-color:rgba(80, 80, 80, 1);"); // 亮灰色背景（行高亮）
                }
                else
                {
                    this->hourWidgets[hour]->setStyleSheet("background-color:rgba(204, 237, 234, 0.82);"); // 淡绿色背景（行高亮）
                }
            } else {
                this->hourWidgets[hour]->setStyleSheet("background-color: transparent;");
            }


            // 更新鼠标总点击数据标签
            QString mouseText = QString::number(mouseCount);
            QString mouseColorStyle = getNumberColorStyle(mouseCount);
            this->mouseLabels[hour]->setText(QString("鼠标: <span style='%1'>%2</span>次").arg(mouseColorStyle).arg(mouseText));
            this->mouseLabels[hour]->setTextFormat(Qt::RichText);
            // 根据主题模式设置文字颜色
            this->mouseLabels[hour]->setStyleSheet(this->isDarkMode ? "color: lightgray;" : "color: black;");


            // 更新左键点击数据标签
            QString leftClickText = QString::number(leftClickCount);
            QString leftClickColorStyle = getNumberColorStyle(leftClickCount);
            this->leftClickLabels[hour]->setText(QString("左键: <span style='%1'>%2</span>次").arg(leftClickColorStyle).arg(leftClickText));
            this->leftClickLabels[hour]->setTextFormat(Qt::RichText);
            // 根据主题模式设置文字颜色
            this->leftClickLabels[hour]->setStyleSheet(this->isDarkMode ? "color: lightgray;" : "color: black;");


            // 更新右键点击数据标签
            QString rightClickText = QString::number(rightClickCount);
            QString rightClickColorStyle = getNumberColorStyle(rightClickCount);
            this->rightClickLabels[hour]->setText(QString("右键: <span style='%1'>%2</span>次").arg(rightClickColorStyle).arg(rightClickText));
            this->rightClickLabels[hour]->setTextFormat(Qt::RichText);
            // 根据主题模式设置文字颜色
            this->rightClickLabels[hour]->setStyleSheet(this->isDarkMode ? "color: lightgray;" : "color: black;");

        }

        // 更新键盘按键颜色和悬停提示
        for (auto it = keyCodeToButton.begin(); it != keyCodeToButton.end(); ++it) {
            int keyCode = it.key();
            QPushButton *button = it.value();
            int count = keyCounts.value(keyCode, 0);
            QString keyName = this->getKeyName(keyCode);
            QString bgColor = getKeyColor(count);
            // 统一使用测试按钮的实现方式：同时设置按钮样式和QToolTip样式
            // 根据主题模式设置按键样式
            if (isDarkMode) {
                button->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } "
                    "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            } else {
                button->setStyleSheet(QString(
                    "QPushButton { background-color: %1; color: black !important; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } "
                    "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
                ).arg(bgColor));
            }

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

    // 设置对话框为模态
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
        // 导出昨日数据为图片（自动导出，不显示对话框）
        exportKeyboardLayoutAsImage(false);
        // 保存昨日数据
        saveStatistics();
        // 更新日期
        today = currentDate;
        // 重置今日计数
        keyPressCount = 0;
        mouseClickCount = 0;
        leftClickCount = 0;
        rightClickCount = 0;
        keyCounts.clear();
        // 加载今日数据（如果有）
        loadStatistics();
    }
}

void MainWindow::initUI()
{
    // 初始化深色模式标志
    isDarkMode = false;

    // 创建中心部件
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 创建主布局
    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // 创建顶部布局（用于放置日期标签和主题切换按钮）
    QHBoxLayout *topLayout = new QHBoxLayout();
    mainLayout->addLayout(topLayout);

    // 创建日期标签
    dateLabel = new QLabel(this);
    topLayout->addWidget(dateLabel);

    // 添加伸缩项，将按钮推到右侧
    topLayout->addStretch();

    // 创建主题切换按钮（圆形）
    themeToggleButton = new QPushButton(this);
    themeToggleButton->setFixedSize(30, 30);
    themeToggleButton->setStyleSheet(
        "QPushButton { background-color: black; border-radius: 15px; border: none; }"
        "QPushButton:hover { background-color: #333; }"
    );
    topLayout->addWidget(themeToggleButton);

    // 连接主题切换按钮的点击信号
    connect(themeToggleButton, &QPushButton::clicked, this, [this]() {
        toggleDarkMode();
    });

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
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportData);
    buttonLayout->addWidget(exportButton);

    // 添加按钮布局到主布局
    mainLayout->addLayout(buttonLayout);

    // 初始化主题
    updateTheme();
}

void MainWindow::toggleDarkMode()
{
    // 切换模式标志
    isDarkMode = !isDarkMode;

    // 更新主题
    updateTheme();
}

void MainWindow::updateTheme()
{
    QString styleSheet;

    if (isDarkMode) {
        // 深色模式样式
        styleSheet = ""
            "QLabel { font-size: 14px; color: #eee; }"
            "QPushButton { background-color: rgb(50, 50, 50); color: white; border-radius: 3px; padding: 5px; font-weight: default; border: 1px solid rgb(70, 70, 70); }"
            "QPushButton:hover { background-color:rgb(70, 70, 70); }"
            "QWidget { background-color: #222; }"
            "QToolTip { background-color: #333; color: white; border: 1px solid #555; padding: 3px; font-size: 12px; }"
            // 拟态化滚动条样式
            "QScrollBar:vertical {"
                "width: 12px;"
                "margin: 5px 0 5px 0;"
                "background: #333;"
                "border-radius: 0px;"
            "}"
            "QScrollBar::handle:vertical {"
                "background: #555;"
                "border-radius: 0px;"
                "min-height: 20px;"
            "}"
            "QScrollBar::handle:vertical:hover {"
                "background:#777;"
            "}"
            "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                "background: transparent;"
                "height: 3px;"
            "}"
            "QScrollBar:horizontal {"
                "height: 12px;"
                "margin: 0 5px 0 5px;"
                "background:#333;"
                "border-radius: 0px;"
            "}"
            "QScrollBar::handle:horizontal {"
                "background:#555;"
                "border-radius: 0px;"
                "min-width: 20px;"
            "}"
            "QScrollBar::handle:horizontal:hover {"
                "background:#777;"
            "}"
            "QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
                "background: transparent;"
                "width: 3px;"
            "}";

        // 更新主题切换按钮颜色为白色
        themeToggleButton->setStyleSheet(
            "QPushButton { background-color: white; border-radius: 15px; border: none; }"
            "QPushButton:hover { background-color: #ddd; }"
        );
    } else {
        // 日间模式样式
        styleSheet = ""
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

        // 更新主题切换按钮颜色为黑色
        themeToggleButton->setStyleSheet(
            "QPushButton { background-color: black; border-radius: 15px; border: none; }"
            "QPushButton:hover { background-color: #333; }"
        );
    }

    // 应用样式
    setStyleSheet(styleSheet);

    // 如果详情对话框已打开，也更新其样式
    if (detailsDialog) {
        detailsDialog->setStyleSheet(styleSheet);
        // 更新键盘容器背景色
        if (keyboardWidget) {
            if (isDarkMode) {
                keyboardWidget->setStyleSheet("background-color: #333333; border-radius: 5px; padding: 10px;");
            } else {
                keyboardWidget->setStyleSheet("background-color: #f0f0f0; border-radius: 5px; padding: 10px;");
            }
        }
        
        // 更新数据详情标题
        if (detailsTitleLabel) {
            if (isDarkMode) {
                detailsTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: lightgray;");
            } else {
                detailsTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: black;");
            }
        }
        
        // 更新键盘按键统计标题
        if (keyboardLayoutTitleLabel) {
            if (isDarkMode) {
                keyboardLayoutTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: lightgray;");
            } else {
                keyboardLayoutTitleLabel->setStyleSheet("font-weight: bold; font-size: 14px; color: black;");
            }
        }
        
        // 更新时间范围标签
        for (QLabel *label : timeLabels) {
            if (isDarkMode) {
                label->setStyleSheet("font-weight: bold; color: lightgray;");
            } else {
                label->setStyleSheet("font-weight: bold; color: black;");
            }
        }
        
        // 更新数据标签
        for (QLabel *label : keyLabels) {
            if (isDarkMode) {
                label->setStyleSheet("color: white;");
            } else {
                label->setStyleSheet("color: black;");
            }
        }
        
        for (QLabel *label : mouseLabels) {
            if (isDarkMode) {
                label->setStyleSheet("color: lightgray;");
            } else {
                label->setStyleSheet("color: black;");
            }
        }
        
        for (QLabel *label : leftClickLabels) {
            if (isDarkMode) {
                label->setStyleSheet("color: lightgray;");
            } else {
                label->setStyleSheet("color: black;");
            }
        }
        
        for (QLabel *label : rightClickLabels) {
            if (isDarkMode) {
                label->setStyleSheet("color: lightgray;");
            } else {
                label->setStyleSheet("color: black;");
            }
        }
    }

    // 更新所有键盘按键的样式（包括主窗口和详情对话框）
    for (auto it = keyCodeToButton.begin(); it != keyCodeToButton.end(); ++it) {
        QPushButton *keyButton = it.value();
        int keyCode = it.key();
        int count = keyCounts.value(keyCode, 0);
        QString bgColor = getKeyColor(count);
        
        if (isDarkMode) {
            keyButton->setStyleSheet(QString(
                "QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } "
                "QPushButton:hover { background-color: #555; } "
                "QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }"
            ).arg(bgColor));
        } else {
            keyButton->setStyleSheet(QString(
                "QPushButton { background-color: %1; color: black !important; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } "
                "QPushButton:hover { background-color: #e0e0e0; } "
                "QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }"
            ).arg(bgColor));
        }
    }

    // 额外确保详情对话框中的按键样式正确更新
    if (detailsDialog) {
        // 直接遍历keyCodeToButton映射，确保所有按键样式正确
        for (auto it = keyCodeToButton.begin(); it != keyCodeToButton.end(); ++it) {
            QPushButton *keyButton = it.value();
            // 检查按钮是否属于详情对话框
            if (keyButton->parent() == detailsDialog) {
                int keyCode = it.key();
                int count = keyCounts.value(keyCode, 0);
                QString bgColor = getKeyColor(count);
                  
                if (isDarkMode) {
                    keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white !important; border-radius: 3px; border: 1px solid #555; padding: 5px; } QToolTip { background-color: #444 !important; color: white !important; border: 1px solid #666 !important; padding: 3px !important; font-size: 12px !important; }").arg(bgColor));
                } else {
                    keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black !important; border-radius: 3px; border: 1px solid #ccc; padding: 5px; } QToolTip { background-color: white !important; color: black !important; border: 1px solid #ccc !important; padding: 3px !important; font-size: 12px !important; }").arg(bgColor));
                }
            }
        }
    }
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

void MainWindow::exitApplication()
{
    // 保存统计数据
    saveStatistics();
    // 清理钩子
    cleanupHooks();
    // 退出应用
    qApp->quit();
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
    connect(exitAction, &QAction::triggered, this, &MainWindow::exitApplication);
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

void MainWindow::exportKeyboardLayoutAsImage(bool showDialog)
{
    // 创建一个专门用于导出的对话框
    QDialog* exportDialog = nullptr;
    if (showDialog) {
        exportDialog = new QDialog(this);
        exportDialog->setWindowTitle("导出预览");
        exportDialog->resize(900, 700);
    } else {
        // 非交互模式下，创建一个临时对话框用于渲染
        exportDialog = new QDialog(this, Qt::Widget);
        exportDialog->resize(900, 700);
    }

    // 根据当前主题设置对话框样式
    if (isDarkMode) {
        exportDialog->setStyleSheet(
            "QDialog { background-color: #222; }"
            "QLabel { color: #eee; }"
            "QPushButton { background-color: rgb(50, 50, 50); color: white; border-radius: 3px; padding: 5px; border: 1px solid rgb(70, 70, 70); }"
            "QPushButton:hover { background-color:rgb(70, 70, 70); }"
        );
    } else {
        exportDialog->setStyleSheet(
            "QDialog { background-color: #f5f5f5; }"
            "QLabel { color: #333; }"
            "QPushButton { background-color: rgb(172, 209, 245); color: black; border-radius: 3px; padding: 5px; border: 1px solid rgb(172, 209, 245); }"
            "QPushButton:hover { background-color:rgb(186, 232, 255); }"
        );
    }

    // 创建主布局
    QHBoxLayout *mainLayout = new QHBoxLayout(exportDialog);

    // 左侧：键盘布局
    QWidget *keyboardWidget = new QWidget();
    if (isDarkMode) {
        keyboardWidget->setStyleSheet("background-color: #333; border-radius: 5px; padding: 10px;");
    } else {
        keyboardWidget->setStyleSheet("background-color: #f0f0f0; border-radius: 5px; padding: 10px;");
    }
    QVBoxLayout *keyboardLayout = new QVBoxLayout(keyboardWidget);

    // 键盘标题
    QLabel *keyboardTitle = new QLabel("键盘使用数据");
    keyboardTitle->setAlignment(Qt::AlignCenter);
    if (isDarkMode) {
        keyboardTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: white;");
    } else {
        keyboardTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: black;");
    }
    keyboardLayout->addWidget(keyboardTitle);

    // 创建键盘行
    QVector<QHBoxLayout*> keyboardRows;
    for (int i = 0; i < 7; i++) {
        QHBoxLayout *row = new QHBoxLayout();
        row->setSpacing(5);
        row->setAlignment(Qt::AlignCenter);
        keyboardRows.append(row);
        keyboardLayout->addLayout(row);
    }

    // 按键代码到按钮的映射
    QMap<int, QPushButton*> tempKeyCodeToButton;

    // 第一行：功能键
    QVector<int> row1Keys = {VK_ESCAPE, VK_F1, VK_F2, VK_F3, VK_F4, VK_F5, VK_F6, VK_F7, VK_F8, VK_F9, VK_F10, VK_F11, VK_F12};
    keyboardRows[0]->addSpacing(40);
    for (int keyCode : row1Keys) {
        QString keyName = getKeyName(keyCode);
        int count = keyCounts.value(keyCode, 0);

        // 创建按键容器
        QVBoxLayout *keyContainer = new QVBoxLayout();
        QPushButton *keyButton = new QPushButton(keyName);
        keyButton->setFixedSize(60, 30);
        keyButton->setFont(QFont("Arial", 8));

        // 设置按键颜色
        QString bgColor = getKeyColor(count);
        if (isDarkMode) {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 3px; border: 1px solid #555; padding: 3px; }").arg(bgColor));
        } else {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 3px; }").arg(bgColor));
        }

        // 添加次数标签
        QLabel *countLabel = new QLabel(QString::number(count));
        countLabel->setAlignment(Qt::AlignCenter);
        countLabel->setFont(QFont("Arial", 7));

        // 添加到容器
        keyContainer->addWidget(keyButton);
        keyContainer->addWidget(countLabel);
        keyboardRows[0]->addLayout(keyContainer);

        tempKeyCodeToButton[keyCode] = keyButton;
    }

    // 第二行：数字键
    QVector<int> row2Keys = {VK_OEM_3, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', VK_OEM_MINUS, VK_OEM_PLUS, VK_BACK};
    for (int keyCode : row2Keys) {
        QString keyName = getKeyName(keyCode);
        int count = keyCounts.value(keyCode, 0);

        QVBoxLayout *keyContainer = new QVBoxLayout();
        QPushButton *keyButton = new QPushButton(keyName);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));

        QString bgColor = getKeyColor(count);
        if (isDarkMode) {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 3px; border: 1px solid #555; padding: 5px; }").arg(bgColor));
        } else {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; }").arg(bgColor));
        }

        QLabel *countLabel = new QLabel(QString::number(count));
        countLabel->setAlignment(Qt::AlignCenter);
        countLabel->setFont(QFont("Arial", 7));
        if (isDarkMode) {
            countLabel->setStyleSheet("color: white;");
        } else {
            countLabel->setStyleSheet("color: black;");
        }

        keyContainer->addWidget(keyButton);
        keyContainer->addWidget(countLabel);
        keyboardRows[1]->addLayout(keyContainer);

        tempKeyCodeToButton[keyCode] = keyButton;
    }

    // 第三行：字母键 Q-W-E-R-T-Y
    QVector<int> row3Keys = {VK_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', VK_OEM_4, VK_OEM_6, VK_OEM_5};
    for (int keyCode : row3Keys) {
        QString keyName = getKeyName(keyCode);
        int count = keyCounts.value(keyCode, 0);

        QVBoxLayout *keyContainer = new QVBoxLayout();
        QPushButton *keyButton = new QPushButton(keyName);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));

        QString bgColor = getKeyColor(count);
        if (isDarkMode) {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 3px; border: 1px solid #555; padding: 5px; }").arg(bgColor));
        } else {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; }").arg(bgColor));
        }

        QLabel *countLabel = new QLabel(QString::number(count));
        countLabel->setAlignment(Qt::AlignCenter);
        countLabel->setFont(QFont("Arial", 7));

        keyContainer->addWidget(keyButton);
        keyContainer->addWidget(countLabel);
        keyboardRows[2]->addLayout(keyContainer);

        tempKeyCodeToButton[keyCode] = keyButton;
    }

    // 第四行：字母键 A-S-D-F-G
    QVector<int> row4Keys = {VK_CAPITAL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', VK_OEM_1, VK_OEM_7, VK_RETURN};
    for (int keyCode : row4Keys) {
        QString keyName = getKeyName(keyCode);
        int count = keyCounts.value(keyCode, 0);

        QVBoxLayout *keyContainer = new QVBoxLayout();
        QPushButton *keyButton = new QPushButton(keyName);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));

        QString bgColor = getKeyColor(count);
        if (isDarkMode) {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 3px; border: 1px solid #555; padding: 5px; }").arg(bgColor));
        } else {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; }").arg(bgColor));
        }

        QLabel *countLabel = new QLabel(QString::number(count));
        countLabel->setAlignment(Qt::AlignCenter);
        countLabel->setFont(QFont("Arial", 7));

        keyContainer->addWidget(keyButton);
        keyContainer->addWidget(countLabel);
        keyboardRows[3]->addLayout(keyContainer);

        tempKeyCodeToButton[keyCode] = keyButton;
    }

    // 第五行：字母键 Z-X-C-V-B
    QVector<int> row5Keys = {VK_LSHIFT, 'Z', 'X', 'C', 'V', 'B', 'N', 'M', VK_OEM_COMMA, VK_OEM_PERIOD, VK_OEM_2, VK_RSHIFT};
    for (int keyCode : row5Keys) {
        QString keyName = getKeyName(keyCode);
        int count = keyCounts.value(keyCode, 0);

        QVBoxLayout *keyContainer = new QVBoxLayout();
        QPushButton *keyButton = new QPushButton(keyName);
        keyButton->setFixedSize(65, 35);
        keyButton->setFont(QFont("Arial", 9));

        QString bgColor = getKeyColor(count);
        if (isDarkMode) {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 3px; border: 1px solid #555; padding: 5px; }").arg(bgColor));
        } else {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; }").arg(bgColor));
        }

        QLabel *countLabel = new QLabel(QString::number(count));
        countLabel->setAlignment(Qt::AlignCenter);
        countLabel->setFont(QFont("Arial", 7));

        keyContainer->addWidget(keyButton);
        keyContainer->addWidget(countLabel);
        keyboardRows[4]->addLayout(keyContainer);

        tempKeyCodeToButton[keyCode] = keyButton;
    }

    // 第六行：空行
    keyboardRows[5]->addSpacing(35);

    // 第七行：特殊键和空格键
    QVector<int> row7Keys = {VK_LCONTROL, VK_LMENU, VK_SPACE, VK_RMENU, VK_RCONTROL};
    keyboardRows[6]->addSpacing(40);

    for (int i = 0; i < row7Keys.size(); i++) {
        int keyCode = row7Keys[i];
        QString keyName = getKeyName(keyCode);
        int count = keyCounts.value(keyCode, 0);

        QVBoxLayout *keyContainer = new QVBoxLayout();
        QPushButton *keyButton = new QPushButton(keyName);
        keyButton->setFont(QFont("Arial", 9));

        if (keyCode == VK_SPACE) {
            keyButton->setFixedSize(300, 35);
        } else if (keyCode == VK_CONTROL || keyCode == VK_MENU) {
            if (i == 0 || i == row7Keys.size() - 1) {
                keyButton->setFixedSize(100, 35);
            } else {
                keyButton->setFixedSize(85, 35);
            }
        } else {
            keyButton->setFixedSize(65, 35);
        }

        QString bgColor = getKeyColor(count);
        if (isDarkMode) {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 3px; border: 1px solid #555; padding: 5px; }").arg(bgColor));
        } else {
            keyButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; }").arg(bgColor));
        }

        QLabel *countLabel = new QLabel(QString::number(count));
        countLabel->setAlignment(Qt::AlignCenter);
        countLabel->setFont(QFont("Arial", 7));

        keyContainer->addWidget(keyButton);
        keyContainer->addWidget(countLabel);
        keyboardRows[6]->addLayout(keyContainer);

        // 在RControl后添加方向键
        if (i == row7Keys.size() - 1) {
            keyboardRows[6]->addSpacing(15);

            QVBoxLayout *directionLayout = new QVBoxLayout();
            directionLayout->setSpacing(5);
            directionLayout->setAlignment(Qt::AlignCenter);

            QHBoxLayout *upLayout = new QHBoxLayout();
            upLayout->setSpacing(5);
            upLayout->setAlignment(Qt::AlignCenter);

            // 上键
            int upKeyCode = VK_UP;
            QString upKeyName = getKeyName(upKeyCode);
            int upCount = keyCounts.value(upKeyCode, 0);
            QVBoxLayout *upContainer = new QVBoxLayout();
            QPushButton *upButton = new QPushButton(upKeyName);
            upButton->setFixedSize(65, 35);
            upButton->setFont(QFont("Arial", 9));
            if (isDarkMode) {
                upButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 3px; border: 1px solid #555; padding: 5px; }").arg(getKeyColor(upCount)));
            } else {
                upButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; }").arg(getKeyColor(upCount)));
            }
            QLabel *upCountLabel = new QLabel(QString::number(upCount));
            upCountLabel->setAlignment(Qt::AlignCenter);
            upCountLabel->setFont(QFont("Arial", 7));
            if (isDarkMode) {
                upCountLabel->setStyleSheet("color: white;");
            } else {
                upCountLabel->setStyleSheet("color: black;");
            }
            upContainer->addWidget(upButton);
            upContainer->addWidget(upCountLabel);
            upLayout->addLayout(upContainer);
            directionLayout->addLayout(upLayout);

            QHBoxLayout *downAndArrowsLayout = new QHBoxLayout();
            downAndArrowsLayout->setSpacing(5);
            downAndArrowsLayout->setAlignment(Qt::AlignCenter);

            // 左键
            int leftKeyCode = VK_LEFT;
            QString leftKeyName = getKeyName(leftKeyCode);
            int leftCount = keyCounts.value(leftKeyCode, 0);
            QVBoxLayout *leftContainer = new QVBoxLayout();
            QPushButton *leftButton = new QPushButton(leftKeyName);
            leftButton->setFixedSize(65, 35);
            leftButton->setFont(QFont("Arial", 9));
            if (isDarkMode) {
                leftButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 3px; border: 1px solid #555; padding: 5px; }").arg(getKeyColor(leftCount)));
            } else {
                leftButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; }").arg(getKeyColor(leftCount)));
            }
            QLabel *leftCountLabel = new QLabel(QString::number(leftCount));
            leftCountLabel->setAlignment(Qt::AlignCenter);
            leftCountLabel->setFont(QFont("Arial", 7));
            if (isDarkMode) {
                leftCountLabel->setStyleSheet("color: white;");
            } else {
                leftCountLabel->setStyleSheet("color: black;");
            }
            leftContainer->addWidget(leftButton);
            leftContainer->addWidget(leftCountLabel);
            downAndArrowsLayout->addLayout(leftContainer);

            // 下键
            int downKeyCode = VK_DOWN;
            QString downKeyName = getKeyName(downKeyCode);
            int downCount = keyCounts.value(downKeyCode, 0);
            QVBoxLayout *downContainer = new QVBoxLayout();
            QPushButton *downButton = new QPushButton(downKeyName);
            downButton->setFixedSize(65, 35);
            downButton->setFont(QFont("Arial", 9));
            if (isDarkMode) {
                downButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 3px; border: 1px solid #555; padding: 5px; }").arg(getKeyColor(downCount)));
            } else {
                downButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; }").arg(getKeyColor(downCount)));
            }
            QLabel *downCountLabel = new QLabel(QString::number(downCount));
            downCountLabel->setAlignment(Qt::AlignCenter);
            downCountLabel->setFont(QFont("Arial", 7));
            if (isDarkMode) {
                downCountLabel->setStyleSheet("color: white;");
            } else {
                downCountLabel->setStyleSheet("color: black;");
            }
            downContainer->addWidget(downButton);
            downContainer->addWidget(downCountLabel);
            downAndArrowsLayout->addLayout(downContainer);

            // 右键
            int rightKeyCode = VK_RIGHT;
            QString rightKeyName = getKeyName(rightKeyCode);
            int rightCount = keyCounts.value(rightKeyCode, 0);
            QVBoxLayout *rightContainer = new QVBoxLayout();
            QPushButton *rightButton = new QPushButton(rightKeyName);
            rightButton->setFixedSize(65, 35);
            rightButton->setFont(QFont("Arial", 9));
            if (isDarkMode) {
                rightButton->setStyleSheet(QString("QPushButton { background-color: %1; color: white; border-radius: 3px; border: 1px solid #555; padding: 5px; }").arg(getKeyColor(rightCount)));
            } else {
                rightButton->setStyleSheet(QString("QPushButton { background-color: %1; color: black; border-radius: 3px; border: 1px solid #ccc; padding: 5px; }").arg(getKeyColor(rightCount)));
            }
            QLabel *rightCountLabel = new QLabel(QString::number(rightCount));
            rightCountLabel->setAlignment(Qt::AlignCenter);
            rightCountLabel->setFont(QFont("Arial", 7));
            if (isDarkMode) {
                rightCountLabel->setStyleSheet("color: white;");
            } else {
                rightCountLabel->setStyleSheet("color: black;");
            }
            rightContainer->addWidget(rightButton);
            rightContainer->addWidget(rightCountLabel);
            downAndArrowsLayout->addLayout(rightContainer);

            directionLayout->addLayout(downAndArrowsLayout);
            keyboardRows[6]->addLayout(directionLayout);
        }

        tempKeyCodeToButton[keyCode] = keyButton;
    }

    // 右侧：统计信息
    QWidget *statsWidget = new QWidget();
    if (isDarkMode) {
        statsWidget->setStyleSheet("background-color: #333; border-radius: 5px; padding: 10px;");
    } else {
        statsWidget->setStyleSheet("background-color: #f0f0f0; border-radius: 5px; padding: 10px;");
    }
    QVBoxLayout *statsLayout = new QVBoxLayout(statsWidget);

    // 统计标题
    QLabel *statsTitle = new QLabel("统计信息");
    statsTitle->setAlignment(Qt::AlignCenter);
    if (isDarkMode) {
        statsTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: white;");
    } else {
        statsTitle->setStyleSheet("font-weight: bold; font-size: 14px; color: black;");
    }
    statsLayout->addWidget(statsTitle);

    // 键盘总次数统计
    QGroupBox *keyboardTotalGroup = new QGroupBox("键盘统计:");
    if (isDarkMode) {
        keyboardTotalGroup->setStyleSheet("color:white");
    } else {
        keyboardTotalGroup->setStyleSheet("color:black");
    }
    QVBoxLayout *keyboardTotalLayout = new QVBoxLayout(keyboardTotalGroup);
    QLabel *keyTotalLabel = new QLabel(QString("键盘按键总次数: %1 次").arg(keyPressCount));
    if (isDarkMode) {
        keyTotalLabel->setStyleSheet("color: white;");
    } else {
        keyTotalLabel->setStyleSheet("color: black;");
    }
    keyboardTotalLayout->addWidget(keyTotalLabel);
    statsLayout->addWidget(keyboardTotalGroup);

    // 鼠标统计
    QGroupBox *mouseGroup = new QGroupBox("鼠标统计:");
    if (isDarkMode) {
        mouseGroup->setStyleSheet("color: white;");
    } else {
        mouseGroup->setStyleSheet("color: black;");
    }
    QVBoxLayout *mouseLayout = new QVBoxLayout(mouseGroup);
    QLabel *leftClickLabel = new QLabel(QString("左键点击次数: %1 次").arg(leftClickCount));
    if (isDarkMode) {
        leftClickLabel->setStyleSheet("color: white;");
    } else {
        leftClickLabel->setStyleSheet("color: black;");
    }
    mouseLayout->addWidget(leftClickLabel);
    QLabel *rightClickLabel = new QLabel(QString("右键点击次数: %1 次").arg(rightClickCount));
    if (isDarkMode) {
        rightClickLabel->setStyleSheet("color: white;");
    } else {
        rightClickLabel->setStyleSheet("color: black;");
    }
    mouseLayout->addWidget(rightClickLabel);
    QLabel *totalClickLabel = new QLabel(QString("总点击次数: %1 次").arg(mouseClickCount));
    if (isDarkMode) {
        totalClickLabel->setStyleSheet("color: white;");
    } else {
        totalClickLabel->setStyleSheet("color: black;");
    }
    mouseLayout->addWidget(totalClickLabel);
    statsLayout->addWidget(mouseGroup);

    // 按键前十榜
    QGroupBox *topKeysGroup = new QGroupBox("按键次数前十:");
    if (isDarkMode) {
        topKeysGroup->setStyleSheet("color: white;");
    } else {
        topKeysGroup->setStyleSheet("color: black;");
    }
    QVBoxLayout *topKeysLayout = new QVBoxLayout(topKeysGroup);

    // 排序按键数据
    QList<QPair<int, int>> sortedKeys;
    for (auto it = keyCounts.begin(); it != keyCounts.end(); ++it) {
        sortedKeys.append(qMakePair(it.key(), it.value()));
    }

    std::sort(sortedKeys.begin(), sortedKeys.end(), [](const QPair<int, int>& a, const QPair<int, int>& b) {
        return a.second > b.second;
    });

    // 显示前10个按键
    int topCount = qMin(10, sortedKeys.size());
    for (int i = 0; i < topCount; i++) {
        int keyCode = sortedKeys[i].first;
        int count = sortedKeys[i].second;
        QString keyName = getKeyName(keyCode);
        QLabel *keyLabel = new QLabel(QString("%1. %2: %3 次").arg(i+1).arg(keyName).arg(count));
        if (isDarkMode) {
            keyLabel->setStyleSheet("color: white;");
        } else {
            keyLabel->setStyleSheet("color: black;");
        }
        topKeysLayout->addWidget(keyLabel);
    }
    statsLayout->addWidget(topKeysGroup);

    // 添加到主布局
    mainLayout->addWidget(keyboardWidget, 2);
    mainLayout->addWidget(statsWidget, 1);

    // 创建导出逻辑的lambda表达式
    auto doExport = [this, exportDialog, showDialog]() -> bool {
        // 调整导出图片宽度为窗口宽度-50-50-30像素
        QSize originalSize = exportDialog->size();
        int adjustedWidth = originalSize.width() - 50 - 50 - 30;
        QSize adjustedSize(adjustedWidth, originalSize.height());
        QPixmap pixmap(adjustedSize);
        exportDialog->render(&pixmap, QPoint(), QRegion(QRect(QPoint(), adjustedSize)));

        QString fileName;
        QString filePath;
        bool isAutoSave = (exportDialog->windowTitle() == "自动导出") || !showDialog;

        // 获取当前日期时间
        QDateTime now = QDateTime::currentDateTime();
        QString datetimeStr = now.toString("yyyyMMddHHmmss");
        QString dateStr = now.toString("yyyy-MM-dd");

        // 创建文件夹路径 - 根目录下output文件夹\年月日
        QString appPath = QCoreApplication::applicationDirPath();
        QString outputDirPath = appPath + QDir::separator() + "output" + QDir::separator() + dateStr + QDir::separator();
        QDir outputDir;
        if (!outputDir.mkpath(outputDirPath)) {
            if (!isAutoSave) {
                QMessageBox::critical(this, tr("错误"), tr("无法创建输出文件夹: ") + outputDirPath);
            }
            qDebug() << "无法创建输出文件夹: " << outputDirPath;
            return false;
        }

        if (isAutoSave) {
            // 自动导出或静默模式
            fileName = QString("Autosave_KeyboardLayout_%1.png").arg(datetimeStr);
            filePath = outputDirPath + fileName;
        } else {
            // 主动导出时直接按格式保存，无需用户选择
            fileName = QString("%1.png").arg(datetimeStr);
            filePath = outputDirPath + fileName;
        }

        // 保存图片
        bool saved = pixmap.save(filePath, "PNG");
        if (saved) {
            if (!isAutoSave) {
                QMessageBox::information(this, tr("成功"), tr("图片已成功保存到:\n") + filePath);
            }
            return true;
        } else {
            if (!isAutoSave) {
                QMessageBox::critical(this, tr("错误"), tr("无法保存图片!"));
            }
            return false;
        }
    };

    // 创建按钮布局
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    // 创建导出图片按钮
    QPushButton *exportButton = new QPushButton("导出为图片", exportDialog);
    if (isDarkMode) {
        exportButton->setStyleSheet("QPushButton { background-color: rgb(50, 50, 50); color: white; border-radius: 3px; padding: 5px; border: 1px solid rgb(70, 70, 70); } QPushButton:hover { background-color: rgb(70, 70, 70); } ");
    } else {
        exportButton->setStyleSheet("QPushButton { background-color: rgb(255, 255, 255); color: black; border-radius: 3px; padding: 5px; border: 1px solid rgb(172, 209, 245); } QPushButton:hover { background-color: rgb(142, 189, 235); } ");
    }
    connect(exportButton, &QPushButton::clicked, this, [this, exportDialog, doExport]() {
        if (doExport()) {
            exportDialog->accept(); // 导出后关闭窗口
        }
    });
    buttonLayout->addWidget(exportButton);

    // 创建取消按钮
    QPushButton *cancelButton = new QPushButton("取消", exportDialog);
    if (isDarkMode) {
        cancelButton->setStyleSheet("QPushButton { background-color: rgb(50, 50, 50); color: white; border-radius: 3px; padding: 5px; border: 1px solid rgb(70, 70, 70); } QPushButton:hover { background-color: rgb(70, 70, 70); } ");
    } else {
        cancelButton->setStyleSheet("QPushButton { background-color: rgb(255, 255, 255); color: black; border-radius: 3px; padding: 5px; border: 1px solid rgb(172, 209, 245); } QPushButton:hover { background-color: rgb(142, 189, 235); } ");
    }
    connect(cancelButton, &QPushButton::clicked, exportDialog, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addLayout(buttonLayout);

    // 执行导出
    if (!showDialog) {
        doExport();
        delete exportDialog;
        return;
    }

    // 显示对话框
    exportDialog->exec();
    delete exportDialog;

    // 添加默认参数的重载函数
}

void MainWindow::exportData()
{
    exportKeyboardLayoutAsImage(true);
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

    if (showDialog) {
        QMessageBox::information(this, "成功", "统计数据已成功导出到:\n" + filePath);
    }
}

void MainWindow::checkForUpdates()
{
    // 创建网络访问管理器
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    connect(manager, &QNetworkAccessManager::finished, this, [this](QNetworkReply *reply) {
        if (reply->error() != QNetworkReply::NoError) {
            QMessageBox::critical(this, "更新检查失败", "无法连接到服务器: " + reply->errorString() + "\n\n可能需要使用代理服务器访问GitHub。");
            reply->deleteLater();
            return;
        }

        // 解析GitHub API响应
        QJsonDocument jsonDoc = QJsonDocument::fromJson(reply->readAll());
        if (!jsonDoc.isArray()) {
            QMessageBox::critical(this, "更新检查失败", "无效的响应格式");
            reply->deleteLater();
            return;
        }

        QJsonArray releases = jsonDoc.array();
        if (releases.isEmpty()) {
            QMessageBox::information(this, "更新检查", "没有找到发布版本");
            reply->deleteLater();
            return;
        }

        // 获取最新版本
        QJsonObject latestRelease = releases[0].toObject();
        QString latestVersion = latestRelease["tag_name"].toString();
        // 去掉版本号前面的"v"或其他前缀
        latestVersion = latestVersion.remove(QRegularExpression("^[^0-9]*"));

        // 比较版本号
        if (isVersionGreater(latestVersion, APP_VERSION)) {
            // 有新版本
            QString updateUrl;
            QString checksum;

            // 查找资产 - 同时查找ZIP和EXE文件
        QJsonArray assets = latestRelease["assets"].toArray();
        QString zipUrl, exeUrl;
        for (const QJsonValue &asset : assets) {
            QJsonObject assetObj = asset.toObject();
            QString name = assetObj["name"].toString();
            if (name.contains(".zip", Qt::CaseInsensitive)) {
                zipUrl = assetObj["browser_download_url"].toString();
            }
            if (name.contains(".exe", Qt::CaseInsensitive)) {
                exeUrl = assetObj["browser_download_url"].toString();
            }
        }

            // 尝试从release body中提取校验和
            QString body = latestRelease["body"].toString();
            QRegularExpression checksumRegex("SHA256:\\s*([0-9a-fA-F]{64})", QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch match = checksumRegex.match(body);
            if (match.hasMatch()) {
                checksum = match.captured(1);
            }

            // 提示用户选择下载类型
            QString downloadType;
            if (!zipUrl.isEmpty() && !exeUrl.isEmpty()) {
                // 同时有ZIP和EXE文件
                QMessageBox msgBox(this);
                msgBox.setWindowTitle("发现新版本");
                msgBox.setText(QString("发现新版本 %1，当前版本 %2。\n请选择下载类型：").arg(latestVersion).arg(APP_VERSION));
                msgBox.setStandardButtons(QMessageBox::NoButton);

                // 添加自定义按钮
                QPushButton *zipButton = msgBox.addButton("ZIP压缩包", QMessageBox::ActionRole);
                QPushButton *exeButton = msgBox.addButton("EXE安装程序", QMessageBox::ActionRole);
                msgBox.addButton(QMessageBox::Cancel);
                msgBox.button(QMessageBox::Cancel)->setText("取消");

                msgBox.exec();

                if (msgBox.clickedButton() == zipButton) {
                    downloadUpdate(zipUrl, checksum, latestVersion, "zip");
                } else if (msgBox.clickedButton() == exeButton) {
                    downloadUpdate(exeUrl, checksum, latestVersion, "exe");
                }
            } else if (!zipUrl.isEmpty()) {
                // 只有ZIP文件
                int reply = QMessageBox::question(this, "发现新版本",
                    QString("发现新版本 %1，当前版本 %2。\n是否下载ZIP压缩包更新？").arg(latestVersion).arg(APP_VERSION),
                    QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    downloadUpdate(zipUrl, checksum, latestVersion, "zip");
                }
            } else if (!exeUrl.isEmpty()) {
                // 只有EXE文件
                int reply = QMessageBox::question(this, "发现新版本",
                    QString("发现新版本 %1，当前版本 %2。\n是否下载EXE安装程序更新？").arg(latestVersion).arg(APP_VERSION),
                    QMessageBox::Yes | QMessageBox::No);

                if (reply == QMessageBox::Yes) {
                    downloadUpdate(exeUrl, checksum, latestVersion, "exe");
                }
            } else {
                QMessageBox::critical(this, "更新失败", "未找到可用的更新文件");
            }
        } else {
            QMessageBox::information(this, "更新检查", "当前已经是最新版本");
        }

        reply->deleteLater();
    });

    // 发送请求到GitHub API
    QUrl url("https://api.github.com/repos/mangfufu/-KeyboardandMouseUsageCounter/releases");
    manager->get(QNetworkRequest(url));
}

bool MainWindow::isVersionGreater(const QString &newVersion, const QString &oldVersion)
{
    // 版本号比较函数
    QStringList newParts = newVersion.split(".");
    QStringList oldParts = oldVersion.split(".");

    int maxLen = qMax(newParts.size(), oldParts.size());
    for (int i = 0; i < maxLen; i++) {
        int newValue = (i < newParts.size()) ? newParts[i].toInt() : 0;
        int oldValue = (i < oldParts.size()) ? oldParts[i].toInt() : 0;

        if (newValue > oldValue) return true;
        if (newValue < oldValue) return false;
    }
    return false; // 版本相同
}

void MainWindow::downloadUpdate(const QString &updateUrl, const QString &checksum, const QString &latestVersion, const QString &fileType)
{
    if (updateUrl.isEmpty()) {
        QMessageBox::critical(this, "下载失败", "更新文件URL为空");
        return;
    }

    // 创建下载目录
    QString appPath = QCoreApplication::applicationDirPath();
    QString updateDirPath = appPath + QDir::separator() + "updates" + QDir::separator() + latestVersion + QDir::separator();
    QDir updateDir;
    if (!updateDir.mkpath(updateDirPath)) {
        QMessageBox::critical(this, "下载失败", "无法创建更新目录: " + updateDirPath);
        return;
    }

    // 提取文件名
    QUrl url(updateUrl);
    QString fileName = url.fileName();
    QString filePath = updateDirPath + fileName;

    // 创建进度对话框
    QProgressDialog progress("正在下载更新...", "取消", 0, 100, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);
    progress.setValue(0);

    // 创建网络访问管理器
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QNetworkReply *reply = manager->get(QNetworkRequest(url));

    // 连接下载进度信号
    connect(reply, &QNetworkReply::downloadProgress, this, [&progress, reply](qint64 bytesReceived, qint64 bytesTotal) {
        if (bytesTotal > 0) {
            int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
            progress.setValue(percent);
        }
        if (progress.wasCanceled()) {
            reply->abort();
        }
    });

    // 连接完成信号
    connect(manager, &QNetworkAccessManager::finished, this, [this, reply, filePath, checksum, latestVersion, fileType](QNetworkReply *networkReply) {
        if (networkReply->error() != QNetworkReply::NoError) {
            if (networkReply->error() != QNetworkReply::OperationCanceledError) {
                QMessageBox::critical(this, "下载失败", "下载更新失败: " + networkReply->errorString() + "\n\n可能需要使用代理服务器访问GitHub。");
            }
            networkReply->deleteLater();
            return;
        }

        // 保存文件
        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, "保存失败", "无法保存更新文件: " + file.errorString());
            reply->deleteLater();
            return;
        }

        file.write(networkReply->readAll());
        file.close();
        networkReply->deleteLater();

        // 验证校验和
        if (!checksum.isEmpty() && !verifyChecksum(filePath, checksum)) {
            QMessageBox::critical(this, "校验失败", "更新文件校验和不匹配，可能已损坏");
            QFile::remove(filePath);
            return;
        }

        // 根据文件类型显示不同的提示信息
        QString message;
        if (fileType == "exe") {
            message = QString("更新文件已下载到:\n%1\n\n请退出程序，运行安装程序进行更新。").arg(filePath);
        } else {
            message = QString("更新文件已下载到:\n%1\n\n请退出程序，解压缩文件并覆盖旧程序。").arg(filePath);
        }
        QMessageBox::information(this, "下载完成", message);
    });

    progress.exec();
}

bool MainWindow::verifyChecksum(const QString &filePath, const QString &expectedChecksum)
{
    // 计算文件的SHA256校验和
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "无法打开文件进行校验和计算: " << filePath;
        return false;
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    if (!hash.addData(&file)) {
        file.close();
        qDebug() << "无法计算文件校验和: " << filePath;
        return false;
    }

    file.close();
    QString actualChecksum = hash.result().toHex();

    // 比较校验和（忽略大小写）
    return actualChecksum.compare(expectedChecksum, Qt::CaseInsensitive) == 0;
}

void MainWindow::applyUpdate(const QString &updateFilePath)
{
    Q_UNUSED(updateFilePath);
    // 此函数留空，因为需求是提示用户手动解压缩
}

void MainWindow::restartApplication()
{
    // 重启应用程序
    qApp->quit();
    QProcess::startDetached(qApp->applicationFilePath(), QStringList());
}

// 重复代码已删除
