# Script 目录说明

本目录用于存放项目自动化脚本。

## 脚本列表
- `build.ps1`：配置并构建项目
- `package.ps1`：默认按 `Release` 自动先构建，再通过 `windeployqt` 收集依赖并打包为 zip（可在未安装 Qt 的 Windows 运行）；会附带 `LICENSE`、`NOTICE`、`COPYRIGHT`、`licenses/`，且 `doc/` 仅打包用户文档（`RUN.md`、`USER_MANUAL.md`，不包含 `README.md`、`CHANGELOG.md`）
- `backup.ps1`：备份源码（排除 build）

## 使用方式
在 PowerShell 中进入项目根目录后执行，例如：

```powershell
./script/build.ps1 -BuildType Release
./script/package.ps1 -BuildType Release
./script/backup.ps1
```
