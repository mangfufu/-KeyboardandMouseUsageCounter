#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QDate>
#include <QSettings>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include "globalhookmanager.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // 窗口显示事件
    void showEvent(QShowEvent *event) override;
    // 窗口关闭事件
    void closeEvent(QCloseEvent *event) override;

private slots:
    // 更新显示槽函数
    void updateDisplay();
    // 重置统计数据
    void resetStatistics();
    // 显示关于对话框
    void showAboutDialog();
    // 处理全局键盘事件
    void handleGlobalKeyPress();
    // 处理具体按键事件
    void handleSpecificKeyPress(int keyCode);
    // 处理左键点击
    void handleLeftButtonClick();
    // 处理右键点击
    void handleRightButtonClick();
    // 显示详情对话框
    void showDetailsDialog();

private:
    // 测试QToolTip功能的辅助函数 [已注释]
    // void testToolTip();
    // 导出统计数据
    void exportStatistics(bool showDialog = true);
    // 详情对话框关闭槽函数
    void onDetailsDialogClosed();

private:
    Ui::MainWindow *ui;
    int keyPressCount;         // 键盘按键计数
    QMap<int, int> keyCounts;  // 具体按键计数
    QMap<int, QPushButton*> keyCodeToButton;  // 按键代码到按钮的映射
    int mouseClickCount;       // 鼠标总点击计数
    int leftClickCount;        // 左键点击计数
    int rightClickCount;       // 右键点击计数
    QDate today;               // 当前日期
    QSettings *settings;       // 设置对象
    QTimer *updateTimer;       // 更新计时器
    GlobalHookManager *hookManager; // 全局钩子管理器
    QSystemTrayIcon *trayIcon; // 托盘图标
    QMenu *trayMenu;           // 托盘菜单
    QDialog *detailsDialog;    // 详情对话框

    // 获取按键颜色
    QString getKeyColor(int count);
    // 获取按键名称
    QString getKeyName(int keyCode);



    // 加载保存的统计数据
    void loadStatistics();
    // 保存统计数据
    void saveStatistics();
    // 检查是否跨天
    void checkDateChange();
    // 初始化UI
    void initUI();
    // 初始化钩子
    void initHooks();
    // 清理钩子
    void cleanupHooks();
    // 初始化托盘
    void initTray();
    // 更新托盘提示
    void updateTrayTooltip();
    // 显示主窗口
    void showMainWindow();
    // 根据数值范围返回颜色样式
    QString getColorStyle(int value);
};
#endif // MAINWINDOW_H
