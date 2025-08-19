#ifndef GLOBALHOOKMANAGER_H
#define GLOBALHOOKMANAGER_H

#include <QObject>
#include <QWidget>
#include <windows.h>

// 全局钩子管理器类
class GlobalHookManager : public QObject
{
    Q_OBJECT

public:
    explicit GlobalHookManager(QObject *parent = nullptr);
    ~GlobalHookManager();

    // 安装钩子
    bool installHooks();
    // 卸载钩子
    void uninstallHooks();

signals:
    // 键盘按键信号
    void keyPressed();
    // 鼠标点击信号
    void leftButtonClicked();
    void rightButtonClicked();

private:
    // 键盘钩子过程
    static LRESULT CALLBACK keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam);
    // 鼠标钩子过程
    static LRESULT CALLBACK mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam);

    // 钩子句柄
    HHOOK m_keyboardHook;
    HHOOK m_mouseHook;

    // 单例实例（用于钩子过程回调）
    static GlobalHookManager *m_instance;
};

#endif // GLOBALHOOKMANAGER_H