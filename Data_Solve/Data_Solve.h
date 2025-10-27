#pragma once

#include <QtWidgets/QMainWindow>
#include <QTableWidget>
#include <QTextEdit>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QFile>
#include <QMessageBox>
#include <QFileDialog>
#include "ui_Data_Solve.h"

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

private:
    Ui::Data_SolveClass ui;

    QVector<QStringList> originalData;
    QVector<QStringList> processedData; // 重复性误差数据
    QVector<QStringList> linearityData; // 线性度Fx数据
    QString currentFilePath;

    bool loadCSVFile(const QString& filePath);
    bool loadExcelFile(const QString& filePath);
    void displayDataInTable(QTableWidget* table, const QVector<QStringList>& data);
    void processData();
    void processLinearityData();
    void calculateStatistics();
    bool exportToCSV(const QString& filePath);
    bool exportToExcel(const QString& filePath);
    void clearAllData();
    void updateStatus(const QString& message);
    void showError(const QString& title, const QString& message);
    void showInfo(const QString& title, const QString& message);
    QStringList parseCSVLine(const QString& line);
    QString convertExcelToCSV(const QString& excelPath);
};