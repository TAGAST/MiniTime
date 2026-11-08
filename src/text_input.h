#pragma once

#include <windows.h>
#include <string>

// 单行文本输入对话框。
// Win32 菜单无法输入文本，重命名之类的操作需要独立的输入界面。
// 该窗口只做"取一段字符串"，不含任何业务逻辑。
class TextInputDialog {
public:
    TextInputDialog() = default;
    ~TextInputDialog();

    TextInputDialog(const TextInputDialog&) = delete;
    TextInputDialog& operator=(const TextInputDialog&) = delete;

    // 显示模态输入框。
    // 返回 false 表示用户取消；成功时 outText 为去空白后的输入。
    bool Show(HINSTANCE instance, HWND owner,
              const std::wstring& title,
              const std::wstring& initialText,
              std::wstring& outText);

private:
    static LRESULT CALLBACK WndProcThunk(HWND, UINT, WPARAM, LPARAM);
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    void CreateControls(HINSTANCE instance);
    void LayoutControls();
    void Confirm();
    void CancelDialog();

    HWND        hwnd_   = nullptr;
    HWND        edit_   = nullptr;
    HWND        okBtn_  = nullptr;
    HWND        cancelBtn_ = nullptr;
    HFONT       font_   = nullptr;

    bool        done_   = false;
    bool        accepted_ = false;
    std::wstring text_;
    std::wstring title_;
};