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
- **Data_Solve.h/.cpp** - 主窗口类实现（v2.0重构版，370行）
- **Data_Solve.ui** - Qt Designer UI 文件（主窗口界面）
- **Data_Solve.qrc** - Qt 资源文件

### 模块化架构（v2.0深度重构）
#### 数据处理层
- **DataLoader.h/.cpp** - 数据加载模块，负责CSV/Excel文件读取
- **DataProcessor.h/.cpp** - 数据处理模块，负责重复性误差和线性度分析
- **DataExporter.h/.cpp** - 数据导出模块，负责CSV/Excel文件导出
- **StatisticsCalculator.h/.cpp** - 统计计算模块，负责统计信息生成

#### UI辅助层（v2.0新增）
- **ExportManager.h/.cpp** - 导出管理器，封装所有导出逻辑（单通道/全通道/性能汇总）
- **MatrixDisplayHelper.h/.cpp** - 矩阵显示助手，负责解耦矩阵的UI展示
- **RangeSettingsDialog.h/.cpp** - 量程设置对话框，独立的量程配置界面
- **DataAverager.h/.cpp** - 数据平均处理器，按指定行数分组求平均（v2.1新增）

#### 工具类
- **MatrixHelper.cpp** - 矩阵运算辅助函数

### 备份文件
- **Data_Solve_original.h/.cpp** - 原始单一文件版本的备份（1130行）
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

### 模块化设计（v2.0深度重构）
- **低耦合**：各模块职责单一，相互独立
- **高复用性**：模块可在其他项目中复用
- **易维护**：修改功能只需修改对应模块
- **可测试**：每个模块可独立进行单元测试
- **代码量优化**：主文件从1130行减少到370行（减少67%）

### 主要模块职责
#### 数据处理层
1. **DataLoader** - 文件格式处理和数据解析
2. **DataProcessor** - 核心算法（重复性误差、线性度分析、解耦矩阵计算）
3. **DataExporter** - 底层文件导出（CSV/Excel）
4. **StatisticsCalculator** - 统计信息计算和格式化

#### UI辅助层（v2.0新增）
5. **ExportManager** - 高层导出逻辑管理
   - 单通道导出
   - 全通道批量导出
   - 性能指标汇总表生成
   - 解耦矩阵和误差分析导出
6. **MatrixDisplayHelper** - UI显示辅助
   - 解耦矩阵表格填充
   - 误差分析数据显示
   - 矩阵说明文本生成
7. **RangeSettingsDialog** - 独立对话框
   - 量程参数配置
   - 默认值恢复
   - 实时预览

#### 控制层
8. **Data_Solve** - 主控制器（精简版）
   - UI事件处理
   - 模块协调
   - 状态更新

## 主要功能

### 数据预处理（v2.1新增）
- **数据平均**: 按指定行数分组求平均（默认5行一组）
  - 自动识别数值列
  - 支持预览模式（不修改原始数据）
  - 支持替换模式（用平均数据替换原始数据）
  - 实时显示处理统计信息

### 数据加载
- 支持CSV和Excel格式
- 自动解析表头
- 显示数据预览

### 数据处理
- 六通道独立处理（Fx, Fy, Fz, Mx, My, Mz）
- 重复性误差计算
- 迟滞性分析
- 线性度计算
- 解耦矩阵计算（3个6×6矩阵）

### 误差分析
- I类误差（传感器级别）
- II类误差（传感器级别）
- 归一化平方误差
- 各通道统计值

### 数据导出
- 单通道导出（重复性+线性度）
- 全通道导出（6个通道完整数据）
- 性能指标汇总表
- 支持CSV和Excel格式

### 参数设置
- 各通道独立量程设置
- 默认量程：Fx/Fy=1000, Fz=1500, Mx/My/Mz=50
- 实时量程显示