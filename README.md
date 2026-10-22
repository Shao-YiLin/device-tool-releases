# 鼠标设备私有协议Python重构项目
## 项目概述
基于OpenCode + Superpowers 构建工具重构原有C语言私有通信库 & MouseTool上位机
1. 将C通信库转为Python，支持BLE蓝牙、USB（直连/单接收器/多接收器）
2. 实现多设备管理：通用基础功能 + 鼠标/键盘差异化专属功能

## 目录说明
- `src_archive/`：只读原始C工程参考，禁止修改
- `new_project/`：OpenCode生成的Python完整项目（通信库+MouseTool）
- `docs/refactor_requirement.md`：完整重构需求文档
- `AGENTS.md`：OpenCode编码约束规范，所有AI生成代码必须遵守
- `scripts/`：批量生成、对比、校验辅助脚本
- `cache/`：OpenCode临时缓存，可随时删除

## 构建工具
- AI生成：OpenCode
- 项目构建：superpowers

## 开发规范
所有代码生成、修改严格遵循 [AGENTS.md](./AGENTS.md)
重构需求以 `docs/01_refactor_requirement.md` 为准

## 初始化&构建流程
1. 拉取仓库
2. 阅读 `docs/01_refactor_requirement.md` 确认重构需求
3. OpenCode加载 AGENTS.md + 需求文档分模块生成代码
4. 使用 superpowers 构建 new_project 工程
5. 运行 test 校验通信、设备扫描、OTA、鼠标CPI功能