#pragma once

// 开机自启：读写 HKCU\Software\Microsoft\Windows\CurrentVersion\Run
// 不需要管理员权限，用户可在任务管理器"启动应用"中管理。
namespace autostart {

// 读取当前是否已启用
bool IsEnabled();

// 启用/禁用；返回是否操作成功
bool SetEnabled(bool enabled);

// 程序路径变化时，把已存在的注册表项修正为当前路径
void SyncPathIfEnabled();

} // namespace autostart
