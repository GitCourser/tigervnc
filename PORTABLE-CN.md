# TigerVNC Viewer Win64 简体中文便携版

## 功能

- 简体中文界面（强制 `zh_CN`）
- 配置与连接历史保存到程序同目录 `vncviewer.ini`
- 支持包含中文等非 ASCII 字符的程序目录
- 配置通过唯一临时文件原子替换，并用跨进程锁保护多实例写入
- 连接时配置与历史在同一次写入中保存
- 不写 `HKCU\Software\TigerVNC\vncviewer` 注册表
- 不创建 `%APPDATA%\TigerVNC`
- 不写 `%TEMP%\vncviewer.log`
- TLS 状态/证书相关文件也使用程序目录

## 单文件构建

便携目标会执行三项单文件约束：

1. 向链接器传入 `-static -static-libgcc -static-libstdc++`
2. 将 `po/zh_CN.mo` 作为 Windows `RCDATA` 资源嵌入 `vncviewer.exe`
3. 链接后用 `objdump` 检查导入表；发现 MinGW 或第三方 DLL 时直接让构建失败

因此运行时不需要外部 `.mo` 或第三方 DLL；运行所需文件只有
`vncviewer.exe` 和可写的 `vncviewer.ini`。发布 ZIP 另附 `LICENCE.TXT`，它不是
运行依赖。Windows 自带的 `KERNEL32.dll`、`USER32.dll` 等系统 DLL 仍会正常
出现在导入表中，这不属于额外分发文件。

### 推荐：MXE 全静态环境

TigerVNC 官方 Windows 单 EXE 使用 MXE 静态目标构建。普通 MSYS2 仓库并不保证
GnuTLS、p11-kit 等依赖都提供可完整静态链接的归档，因此要保留 TLS、RSA-AES
等全部安全功能时，推荐 MXE：

```bash
git clone https://github.com/mxe/mxe.git
cd mxe
make -j$(nproc) \
  MXE_TARGETS=x86_64-w64-mingw32.static \
  cmake gcc fltk gettext gnutls libjpeg-turbo pixman nettle
export PATH="$PWD/usr/bin:$PATH"

cd ../tigervnc
mkdir build-portable && cd build-portable
../../mxe/usr/bin/x86_64-w64-mingw32.static-cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PORTABLE_VIEWER=ON \
  -DBUILD_VIEWER=ON \
  -DBUILD_WINVNC=OFF \
  -DBUILD_JAVA=OFF \
  -DENABLE_NLS=ON \
  -DENABLE_GNUTLS=ON \
  -DENABLE_NETTLE=ON \
  -DENABLE_H264=ON \
  ..
make -j$(nproc) vncviewer
make portable
```

> MXE 的包名和可用选项可能随版本变化；若依赖构建失败，以当前 MXE 文档为准。

### MSYS2

若 MSYS2 中安装的所有依赖均带静态归档，也可直接构建；否则链接阶段会失败，
而不会生成一个运行时仍缺 DLL 的“伪单文件”。

```bash
pacman -S --needed \
  mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-cmake \
  mingw-w64-x86_64-fltk1.3 \
  mingw-w64-x86_64-libjpeg-turbo \
  mingw-w64-x86_64-gnutls \
  mingw-w64-x86_64-pixman \
  mingw-w64-x86_64-nettle \
  mingw-w64-x86_64-gmp \
  gettext

mkdir build-portable && cd build-portable
cmake -G "MSYS Makefiles" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_PORTABLE_VIEWER=ON \
  -DBUILD_VIEWER=ON \
  -DBUILD_WINVNC=OFF \
  -DBUILD_JAVA=OFF \
  -DENABLE_NLS=ON \
  -DENABLE_GNUTLS=ON \
  -DENABLE_NETTLE=ON \
  -DENABLE_H264=ON \
  ..
make -j$(nproc) vncviewer
make portable
```

产物：

- `build-portable/vncviewer/vncviewer.exe`
- `build-portable/TigerVNC-Viewer-Portable-x64-<version>.zip`

可用以下命令人工复核导入表；结果中不应出现 `libgcc_s_*.dll`、
`libstdc++-6.dll`、`libwinpthread-1.dll`、`libintl-*.dll`、
`libgnutls-*.dll` 等文件：

```bash
objdump -p vncviewer/vncviewer.exe | grep "DLL Name"
```

## GitHub Releases 自动构建

`.github/workflows/build.yml` 只构建 Win64 完整便携版，不再执行原项目的
Linux、macOS、Java、Windows 安装包和发行版矩阵构建，也不会上传 Actions
中间产物。工作流使用固定版本的 MXE 静态工具链，并强制启用 NLS、GnuTLS、
Nettle 和 Windows H.264；构建后还会检查功能宏、DLL 导入、ZIP 内容及内置 MO。

触发方式：

- 推送符合 `v*` 的标签，例如 `v1.16.80`
- 在 Actions 页面手动运行，并填写 `v<主版本>.<次版本>.<修订版本>` 格式的版本

手动运行使用页面中选择的分支或提交。若填写的标签已经存在，它必须指向该次
运行所选的提交；若 Release 已存在，则覆盖同名 ZIP 和 SHA-256 文件。最终只向
GitHub Release 上传：

```text
TigerVNC-Viewer-Portable-x64-<version>.zip
TigerVNC-Viewer-Portable-x64-<version>.zip.sha256
```

首次构建 MXE 依赖可能耗时较长；后续运行会复用以固定 MXE 提交为键的缓存。

## 运行时目录结构

首次启动前：

```text
TigerVNC-Viewer-Portable-x64/
├── vncviewer.exe
├── vncviewer.ini
└── LICENCE.TXT
```

`zh_CN.mo` 已内置在 EXE 中。接受 TLS 证书例外后，同目录可能额外生成
`x509_known_hosts`。

## INI 格式示例

```ini
; TigerVNC portable configuration
[General]
Language=zh_CN

[Viewer]
ServerName=192.168.1.100
AutoSelect=1
FullColor=1
PreferredEncoding=Tight
QualityLevel=8

[History]
0=192.168.1.100
1=server.example.com:1
```

## 验收

1. 删除 `%APPDATA%\TigerVNC` 后启动，确认不会重建
2. Process Monitor 确认不写 `Software\TigerVNC\vncviewer`
3. 连接后同目录生成/更新 `vncviewer.ini`
4. 重启后恢复上次服务器与选项
5. 界面显示简体中文
6. 程序目录只读时能启动，但提示保存失败，不回退注册表/AppData
