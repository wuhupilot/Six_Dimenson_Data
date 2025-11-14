#pragma once

#include <QtWidgets/QMainWindow>
#include <QTableWidget>
#include <QTextEdit>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMessageBox>
#include <QFileDialog>
#include "ui_Data_Solve.h"
#include "DataLoader.h"
#include "DataProcessor.h"
#include "DataExporter.h"
#include "StatisticsCalculator.h"
#include "ExportManager.h"
#include "MatrixDisplayHelper.h"
#include "RangeSettingsDialog.h"
#include "DataAverager.h"
#include "DataZeroCalibrator.h"

/**
 * @brief 主窗口类（重构版）
 *
 * 职责：
 * - UI事件处理和用户交互
 * - 协调各模块完成数据处理流程
 * - 显示处理结果和状态更新
 *
 * 已将以下功能拆分到独立模块：
 * - ExportManager: 导出逻辑（单通道/全通道/性能指标汇总）
 * - MatrixDisplayHelper: 矩阵显示逻辑
 * - RangeSettingsDialog: 量程设置对话框
 */
class Data_Solve : public QMainWindow
{
    Q_OBJECT

public:
    Data_Solve(QWidget *parent = nullptr);
    ~Data_Solve();

private slots:
    void onLoadDataClicked();
    void onProcessDataClicked();
    void onExportDataClicked();
    void onClearDataClicked();
    void onActionOpenTriggered();
    void onActionSaveTriggered();
    void onActionExitTriggered();
    void onActionAboutTriggered();
    void onSetRangeClicked();
    void onAverageDataClicked();      // 新增：数据平均功能
    void onZeroCalibrateClicked();    // 新增：置零校准功能

private:
    Ui::Data_SolveClass ui;

    // 数据存储
    QVector<QStringList> originalData;
    QVector<QStringList> processedData;
    QVector<QStringList> linearityData;
    QString currentFilePath;

    // 模块实例
    DataLoader* dataLoader;
    DataProcessor* dataProcessor;
    DataExporter* dataExporter;
    StatisticsCalculator* statisticsCalculator;
    ExportManager* exportManager;           // 新增：导出管理器
    MatrixDisplayHelper* matrixDisplayHelper; // 新增：矩阵显示助手
    DataAverager* dataAverager;             // 新增：数据平均处理器
    DataZeroCalibrator* dataZeroCalibrator; // 新增：置零校准器

    // UI相关方法
    void updateStatus(const QString& message);
    void showError(const QString& title, const QString& message);
    void showInfo(const QString& title, const QString& message);
    void clearAllData();
    void setupConnections();

    // 通道处理相关方法
    QString getSelectedChannel() const;

    // 解耦矩阵相关方法
    void calculateAndDisplayDecouplingMatrix();

    // 量程设置相关方法
    void updateRangeDisplay();
};