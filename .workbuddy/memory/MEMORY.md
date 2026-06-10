# Embedded Software Tools - 长期记忆

## 项目概览
- **名称**: EST Studio
- **定位**: 嵌入式开发综合工具平台（CAN分析/串口调试/协议解码/固件烧录/仪表采集）
- **技术栈**: Qt6 + CMake + C++17
- **目标用户**: 个人/团队内部使用
- **硬件策略**: 纯软件模拟 + 离线回放优先，真实硬件为未来扩展

## 架构决策
- **架构风格**: 插件化模块单体（Plugin-based Modular Monolith）
- **核心模式**: DataBus 发布/订阅 + ITransport 传输抽象 + 协议栈式解码
- **UI方案**: QMainWindow + QDockWidget，插件通过 IViewFactory 注册面板
- **构建策略**: est_core 静态库 + 插件共享库（运行时动态加载）
- **详细文档**: docs/architecture.md（含5个ADR、4阶段演进路线图）

## 演进阶段
- Phase 1: 基础骨架（4-6周）→ Core + CAN Analyzer MVP + ASC回放
- Phase 2: 协议栈 + 串口（6-8周）→ UDS/CCP/XCP + Serial Terminal
- Phase 3: DBC + 仪表 + 烧录（6-8周）→ SignalDB + Instrument Panel + Flasher
- Phase 4: 硬件接入 + 打磨（持续）→ 真实CAN适配器/串口 + 主题/国际化

## UI架构
- **主窗口**: AppShell (QMainWindow + ICore)
- **布局**: SideNavBar(左侧) + QStackedWidget(右侧)，3个页面：HomePage/SerialAssistantPage/DataToolsPage
- **侧边栏**: 自定义 NavIconButton 绘制（非QListWidget），默认收缩显示圆形字母图标，鼠标hover展开显示文字+动画
- **工具栏**: 标题+页面标签，无帮助/关于按钮（已移至菜单栏）
- **构建环境**: MSVC2022 (F:\Microsoft Visual Studio\18) + Qt6.11.0 (F:\Qt\6.11.0\msvc2022_64) + JOM
- **构建命令**: 需先调用 vcvarsall.bat x64，再 cmake --build

## 通道命名规范
- `transport.<type>.<instance>` — 传输层原始数据
- `protocol.<type>.<channel>` — 解码后协议数据
- `plugin.<name>.<signal>` — 插件自定义信号
