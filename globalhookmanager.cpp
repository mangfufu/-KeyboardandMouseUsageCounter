#include "globalhookmanager.h"
#include <QApplication>

// 静态成员初始化
GlobalHookManager *GlobalHookManager::m_instance = nullptr;

GlobalHookManager::GlobalHookManager(QObject *parent) : QObject(parent)
    , m_keyboardHook(nullptr)
    , m_mouseHook(nullptr)
{
    m_instance = this;
}

GlobalHookManager::~GlobalHookManager()
{
    uninstallHooks();
    m_instance = nullptr;
}

bool GlobalHookManager::installHooks()
{
    // 安装键盘钩子
    m_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, keyboardHookProc, GetModuleHandle(nullptr), 0);
    if (!m_keyboardHook) {
        return false;
    }

    // 安装鼠标钩子
    m_mouseHook = SetWindowsHookEx(WH_MOUSE_LL, mouseHookProc, GetModuleHandle(nullptr), 0);
    if (!m_mouseHook) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
        return false;
    }

    return true;
}

void GlobalHookManager::uninstallHooks()
{
    if (m_keyboardHook) {
        UnhookWindowsHookEx(m_keyboardHook);
        m_keyboardHook = nullptr;
    }

    if (m_mouseHook) {
        UnhookWindowsHookEx(m_mouseHook);
        m_mouseHook = nullptr;
    }
}

LRESULT CALLBACK GlobalHookManager::keyboardHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    static QSet<int> pressedKeys; // 使用集合跟踪所有按下的键

    if (nCode >= 0) {
        KBDLLHOOKSTRUCT* pKeyboardStruct = (KBDLLHOOKSTRUCT*)lParam;
        int vkCode = pKeyboardStruct->vkCode;
        bool isExtended = (pKeyboardStruct->flags & LLKHF_EXTENDED) != 0;

        // 处理左右Ctrl、Alt和Shift键
        if (vkCode == VK_CONTROL) {
            // 检查扩展键标志区分左右Ctrl
            if (isExtended) {
                vkCode = VK_RCONTROL; // 右Ctrl
            } else {
                vkCode = VK_LCONTROL; // 左Ctrl
            }
        } else if (vkCode == VK_MENU) {
            // 检查扩展键标志区分左右Alt
            if (isExtended) {
                vkCode = VK_RMENU; // 右Alt
            } else {
                vkCode = VK_LMENU; // 左Alt
            }
        } else if (vkCode == VK_SHIFT) {
            // 检查扩展键标志区分左右Shift
            if (isExtended) {
                vkCode = VK_RSHIFT; // 右Shift
            } else {
                vkCode = VK_LSHIFT; // 左Shift
            }
        }

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            // 键盘按下事件
            if (!pressedKeys.contains(vkCode)) {
                pressedKeys.insert(vkCode);
                if (m_instance) {
                    emit m_instance->keyPressed();
                    // 发送具体按键信号
                    emit m_instance->specificKeyPressed(vkCode);
                }
            }
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            // 键盘释放事件
            pressedKeys.remove(vkCode);
        }
    }

    // 传递钩子信息
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

LRESULT CALLBACK GlobalHookManager::mouseHookProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    static bool leftButtonClicked = false;
    static bool rightButtonClicked = false;

    if (nCode >= 0) {
        // 左键点击
        if (wParam == WM_LBUTTONDOWN && !leftButtonClicked) {
            leftButtonClicked = true;
            if (m_instance) {
                emit m_instance->leftButtonClicked();
            }
        } else if (wParam == WM_LBUTTONUP) {
            leftButtonClicked = false;
        }

        // 右键点击
        if (wParam == WM_RBUTTONDOWN && !rightButtonClicked) {
            rightButtonClicked = true;
            if (m_instance) {
                emit m_instance->rightButtonClicked();
            }
        } else if (wParam == WM_RBUTTONUP) {
            rightButtonClicked = false;
        }
    }

    // 传递钩子信息
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}