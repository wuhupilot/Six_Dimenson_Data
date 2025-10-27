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
    void onSetRangeClicked();          // 新增：设置量程按钮

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

    // UI相关方法
    void displayDataInTable(QTableWidget* table, const QVector<QStringList>& data);
    void updateStatus(const QString& message);
    void showError(const QString& title, const QString& message);
    void showInfo(const QString& title, const QString& message);
    void clearAllData();
    void setupConnections();

    // 通道处理相关方法
    QString getSelectedChannel() const;

    // 导出相关方法
    void exportAllChannels(const QString& fileName);
    void exportCurrentChannel(const QString& fileName);

    // 解耦矩阵相关方法
    void calculateAndDisplayDecouplingMatrix();

    // 量程设置相关方法
    void showRangeSettingsDialog();
    void updateRangeDisplay();  // 更新UI上的量程显示
};