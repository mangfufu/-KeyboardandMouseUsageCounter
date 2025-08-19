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

    // 设置更新计时器，每秒更新一次显示和托盘提示
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateDisplay);
    connect(updateTimer, &QTimer::timeout, this, &MainWindow::updateTrayTooltip);
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
    // 更新显示
    updateDisplay();
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
        updateDisplay();
    }
}

void MainWindow::showAboutDialog()
{
    QMessageBox::about(this, "关于", "键盘鼠标统计工具\n版本 1.1\n在后台记录每日键盘按键和鼠标点击次数");
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
    detailsDialog->setWindowTitle("每小时统计详情");
    detailsDialog->resize(400, 400);

    // 连接对话框关闭信号到槽函数
    connect(detailsDialog, &QDialog::finished, this, &MainWindow::onDetailsDialogClosed);

    // 创建布局
    QVBoxLayout *mainLayout = new QVBoxLayout(detailsDialog);

    // 创建滚动区域
    QScrollArea *scrollArea = new QScrollArea(detailsDialog);
    scrollArea->setWidgetResizable(true);
    QWidget *scrollWidget = new QWidget();
    QVBoxLayout *scrollLayout = new QVBoxLayout(scrollWidget);

    // 创建一个容器来存储所有数据标签，方便后续更新
    QVector<QLabel*> keyLabels;
    QVector<QLabel*> mouseLabels;
    QVector<QLabel*> leftClickLabels;
    QVector<QLabel*> rightClickLabels;

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
    QLabel *titleLabel = new QLabel("每小时统计数据");
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
    for (int hour = 0; hour < 24; hour++) {
        // 创建水平布局
        QHBoxLayout *hourLayout = new QHBoxLayout();

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
        scrollLayout->addLayout(hourLayout);
    }

    // 设置滚动区域
    scrollArea->setWidget(scrollWidget);
    mainLayout->addWidget(scrollArea);

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

    // 创建更新数据的函数，捕获this指针和需要的变量
    auto updateDetailsData = [this, keyLabels, mouseLabels, leftClickLabels, rightClickLabels, getNumberColorStyle]() {
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
    };

    // 初始加载数据
    updateDetailsData();

    // 创建定时器，每5秒更新一次数据
    QTimer *timer = new QTimer(detailsDialog);
    QObject::connect(timer, &QTimer::timeout, timer, [updateDetailsData]() {
        updateDetailsData();
    });
    timer->start(5000); // 5000毫秒 = 5秒

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
    settings->sync();

    // 更新显示
    updateDisplay();
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
    settings->sync();

    // 更新显示
    updateDisplay();
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

    // 更新显示
    updateDisplay();
}

void MainWindow::loadStatistics()
{
    // 加载今日统计数据
    QString dateKey = today.toString("yyyy-MM-dd");
    keyPressCount = settings->value(dateKey + "/keyPressCount", 0).toInt();
    mouseClickCount = settings->value(dateKey + "/mouseClickCount", 0).toInt();
    leftClickCount = settings->value(dateKey + "/leftClickCount", 0).toInt();
    rightClickCount = settings->value(dateKey + "/rightClickCount", 0).toInt();
}

void MainWindow::saveStatistics()
{
    // 保存今日统计数据
    QString dateKey = today.toString("yyyy-MM-dd");
    settings->setValue(dateKey + "/keyPressCount", keyPressCount);
    settings->setValue(dateKey + "/mouseClickCount", mouseClickCount);
    settings->setValue(dateKey + "/leftClickCount", leftClickCount);
    settings->setValue(dateKey + "/rightClickCount", rightClickCount);
    settings->sync();
}

void MainWindow::checkDateChange()
{
    QDate currentDate = QDate::currentDate();
    if (currentDate != today) {
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
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportStatistics);
    buttonLayout->addWidget(exportButton);

    // 添加按钮布局到主布局
    mainLayout->addLayout(buttonLayout);

    // 设置样式
    QString styleSheet = ""
        "QLabel { font-size: 14px; color: #333; }"
        "QPushButton { background-color: #0bda68; color: green; border-radius: 6px; padding: 8px; font-weight: bold; border: 1px solid #1976D2; }"
        "QPushButton:hover { background-color: #00ddaa; }"
        "QWidget { background-color: #f5f5f5; }";
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
    // 设置默认图标（使用Qt内置图标）
    trayIcon->setIcon(QApplication::style()->standardIcon(QStyle::SP_TitleBarMenuButton));

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

void MainWindow::showMainWindow()
{
    // 显示并激活主窗口
    show();
    activateWindow();
}

void MainWindow::exportStatistics()
{
    // 添加导出确认对话框
    if (QMessageBox::question(this, "确认导出", "确定要导出统计数据吗？") != QMessageBox::Yes) {
        return; // 用户选择取消，不执行导出
    }

    // 获取当前日期时间用于创建文件夹和文件名
    QDateTime currentDateTime = QDateTime::currentDateTime();
    QString datetimeFormat = "yyyy-MM-dd-hh_mm_ss";
    QString datetimeStr = currentDateTime.toString(datetimeFormat);
    QString dateStr = currentDateTime.toString("yyyy-MM-dd");

    // 创建文件夹路径
    QString appPath = QCoreApplication::applicationDirPath();
    QString outputDirPath = appPath + "/output/" + dateStr + "/";
    QDir outputDir;
    if (!outputDir.mkpath(outputDirPath)) {
        QMessageBox::critical(this, "错误", "无法创建输出文件夹");
        return;
    }

    // 创建文件路径
    QString filePath = outputDirPath + datetimeStr + ".txt";
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, "错误", "无法创建输出文件");
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
    out << QString("%1%2%3%4%5")
           .arg("时间", -15)
           .arg("键盘次数", -10)
           .arg("鼠标次数", -10)
           .arg("左键次数", -10)
           .arg("右键次数", -10) << "\n";

    for (int hour = 0; hour < 24; hour++) {
        int keyCount = settings->value(dateStr + "/hourlyKeyCount/" + QString::number(hour), 0).toInt();
        int mouseCount = settings->value(dateStr + "/hourlyMouseCount/" + QString::number(hour), 0).toInt();
        int leftClickCount = settings->value(dateStr + "/hourlyLeftClickCount/" + QString::number(hour), 0).toInt();
        int rightClickCount = settings->value(dateStr + "/hourlyRightClickCount/" + QString::number(hour), 0).toInt();

        out << QString("%1:00-%1:59  %2%3%4%5")
               .arg(hour, 2, 10, QChar('0'))
               .arg(keyCount, -10)
               .arg(mouseCount, -10)
               .arg(leftClickCount, -10)
               .arg(rightClickCount, -10) << "\n";
    }

    // 关闭文件
    file.close();

    // 显示成功消息
    QMessageBox::information(this, "成功", "数据已成功导出到:\n" + filePath);
}
