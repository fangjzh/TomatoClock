# 编译说明

## 环境要求
- Windows 10/11
- Qt 5.15+ 或 Qt 6.x（包含 Widgets 模块）
- CMake 3.5+
- 支持 C++17 的编译器（如 MSVC）
- 首次配置时可访问网络（CMake 会通过 FetchContent 拉取 `miniaudio`）

## 说明
- 项目音频主后端为 `miniaudio`，支持 `wav/mp3`，无需 Qt Multimedia。

## 使用 Qt Creator 编译
1. 打开项目根目录下的 `CMakeLists.txt`。
2. 选择 Qt Kit（含 CMake、编译器、Qt 版本）。
3. 点击“构建”。

## 命令行编译（PowerShell）
在项目根目录执行：

```powershell
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## 发布打包（含 Qt 依赖）
在项目根目录执行：

```powershell
./script/package.ps1 -BuildType Release
```

说明：
- `package.ps1` 默认使用 `Release`，并会先自动执行一次构建，再进行打包。
- 脚本会调用 `windeployqt` 自动收集 Qt 运行时依赖。
- 打包会附带必要许可证材料：`LICENSE`、`NOTICE`、`COPYRIGHT`、`licenses/`。
- `doc/` 目录仅打包最终用户文档：`RUN.md`、`USER_MANUAL.md`（不包含 `README.md`、`CHANGELOG.md`）。
- 生成 zip 位于 `dist/`，可在未安装 Qt 的 Windows 机器运行。

## GitHub Release（网页发布 + 本地打 tag）

### 1) 本地准备并打包
先在项目根目录生成最新发布包：

```powershell
./script/package.ps1 -BuildType Release
```

### 2) 本地创建并推送 tag
以 `v0.1.0` 为例：

```powershell
git tag -a v0.1.0 -m "Release v0.1.0"
git push origin v0.1.0
```

可选检查：

```powershell
git tag --list
```

### 3) GitHub 网页创建 Release
1. 打开：`https://github.com/fangjzh/TomatoClock/releases/new`
2. 选择 tag：`v0.1.0`
3. 标题填写：`TomatoClock v0.1.0`
4. Release Notes 可粘贴 `doc/RELEASE_CHANGELOG.md` 对应版本内容
5. 上传二进制包：`dist/TomatoClock-Release-*.zip`（建议选最新时间戳）
6. 点击 `Publish release`

### 4) 后续版本建议
- 下一个版本按顺序使用 `v0.1.1`、`v0.2.0` 等新 tag，避免复用已发布 tag。
- 每次发布前先更新 `doc/RELEASE_CHANGELOG.md` 对应版本条目。

## 常见问题
- 找不到 Qt：确认 Kit 中 Qt 路径正确，且 Widgets 模块已安装。
- 找不到 `windeployqt`：将 Qt 的 `bin` 目录加入 PATH，或设置环境变量 `WINDEPLOYQT_PATH`。
- 构建目录异常：删除 `build/` 后重新配置。
- 首次配置卡在依赖下载：检查网络或代理设置（`miniaudio` 通过 CMake FetchContent 获取）。
