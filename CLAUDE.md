# CLAUDE.md

该文件为 Claude Code (claude.ai/code) 在处理此代码库时提供指导。

## 项目概述

这是一个使用 Visual Studio 2022 (v143 工具集) 在 Windows 上构建的 Qt 5.15.2 C++ 桌面应用程序。

## 构建命令

### 构建项目
```bash
# 使用 Visual Studio 开发者命令提示符
msbuild Data_Solve.sln /p:Configuration=Debug /p:Platform=Win32
msbuild Data_Solve.sln /p:Configuration=Release /p:Platform=Win32
```

### 运行应用程序
```bash
# Debug 版本
Debug\Data_Solve.exe

# Release 版本
Release\Data_Solve.exe
```

## 项目结构

### 核心文件
- **Data_Solve.sln** - Visual Studio 解决方案文件
- **Data_Solve.vcxproj** - Visual Studio 项目配置文件
- **main.cpp** - 应用程序入口点，初始化 QApplication
- **Data_Solve.h/.cpp** - 主窗口类实现（重构后的模块化版本）
- **Data_Solve.ui** - Qt Designer UI 文件（主窗口界面）
- **Data_Solve.qrc** - Qt 资源文件

### 模块化架构（新增）
- **DataLoader.h/.cpp** - 数据加载模块，负责CSV/Excel文件读取
- **DataProcessor.h/.cpp** - 数据处理模块，负责重复性误差和线性度分析
- **DataExporter.h/.cpp** - 数据导出模块，负责CSV/Excel文件导出
- **StatisticsCalculator.h/.cpp** - 统计计算模块，负责统计信息生成

### 备份文件
- **Data_Solve_original.h/.cpp** - 原始单一文件版本的备份
- **Data_Solve_original.vcxproj** - 原始项目配置文件备份

## 开发配置

- **Qt 版本**: 5.15.2_msvc2019
- **Qt 模块**: core, gui, widgets
- **平台工具集**: v143 (Visual Studio 2022)
- **目标平台**: Windows (Win32)
- **字符集**: Unicode

## 关键技术细节

- 项目使用 Qt 的元对象编译器 (MOC) 处理信号/槽机制
- UI 文件由 Qt 用户界面编译器 (UIC) 处理
- 资源文件使用 Qt 资源编译器 (RCC) 编译
- 已启用多处理器编译以加快构建速度

## 架构特点

### 模块化设计
- **低耦合**：各模块职责单一，相互独立
- **高复用性**：模块可在其他项目中复用
- **易维护**：修改功能只需修改对应模块
- **可测试**：每个模块可独立进行单元测试

### 主要模块职责
1. **DataLoader** - 文件格式处理和数据解析
2. **DataProcessor** - 核心算法（重复性误差、线性度分析）
3. **DataExporter** - 结果输出和格式转换
4. **StatisticsCalculator** - 统计信息计算和格式化
5. **Data_Solve** - UI交互和模块协调