#include "Data_Solve.h"
#include <QDir>
#include <QDateTime>
#include <QInputDialog>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>

#pragma execution_character_set("utf-8")

Data_Solve::Data_Solve(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    // 初始化模块
    dataLoader = new DataLoader();
    dataProcessor = new DataProcessor();
    dataExporter = new DataExporter();
    statisticsCalculator = new StatisticsCalculator();

    // 设置信号槽连接
    setupConnections();

    updateStatus("程序启动完成，请加载数据文件");
}

Data_Solve::~Data_Solve()
{
    delete dataLoader;
    delete dataProcessor;
    delete dataExporter;
    delete statisticsCalculator;
}

void Data_Solve::setupConnections()
{
    connect(ui.btnLoadData, &QPushButton::clicked, this, &Data_Solve::onLoadDataClicked);
    connect(ui.btnProcessData, &QPushButton::clicked, this, &Data_Solve::onProcessDataClicked);
    connect(ui.btnExportData, &QPushButton::clicked, this, &Data_Solve::onExportDataClicked);
    connect(ui.btnClearData, &QPushButton::clicked, this, &Data_Solve::onClearDataClicked);

    connect(ui.actionOpen, &QAction::triggered, this, &Data_Solve::onActionOpenTriggered);
    connect(ui.actionSave, &QAction::triggered, this, &Data_Solve::onActionSaveTriggered);
    connect(ui.actionExit, &QAction::triggered, this, &Data_Solve::onActionExitTriggered);
    connect(ui.actionAbout, &QAction::triggered, this, &Data_Solve::onActionAboutTriggered);

    // 新增：量程设置按钮连接
    connect(ui.btnSetRange, &QPushButton::clicked, this, &Data_Solve::onSetRangeClicked);

    // 初始化量程显示
    updateRangeDisplay();
}

void Data_Solve::onLoadDataClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "选择数据文件",
        QDir::currentPath(),
        "Excel Files (*.xlsx *.xls);;CSV Files (*.csv);;All Files (*)");

    if (!fileName.isEmpty()) {
        currentFilePath = fileName;
        updateStatus("正在加载数据文件...");

        if (dataLoader->loadFile(fileName)) {
            originalData = dataLoader->getData();
            displayDataInTable(ui.tableOriginalData, originalData);
            ui.btnProcessData->setEnabled(true);
            ui.btnExportData->setEnabled(true);
            updateStatus(QString("成功加载 %1 行数据").arg(originalData.size()));

            // 生成基本统计信息
            QString stats = statisticsCalculator->generateStatistics(currentFilePath, originalData, "");
            ui.textStatistics->setPlainText(stats);

            // 计算并显示解耦矩阵
            calculateAndDisplayDecouplingMatrix();
        } else {
            showError("加载失败", dataLoader->getLastError());
        }
    }
}

void Data_Solve::onProcessDataClicked()
{
    if (originalData.isEmpty()) {
        showError("处理失败", "没有数据可处理");
        return;
    }

    // 获取用户选择的通道
    QString selectedChannel = getSelectedChannel();

    if (selectedChannel.isEmpty()) {
        showError("处理失败", "请选择一个通道进行处理");
        return;
    }

    updateStatus(QString::fromUtf8("正在处理 %1 通道数据...").arg(selectedChannel));

    // 使用数据处理模块处理选定的通道
    ProcessResult result = dataProcessor->processChannelData(originalData, selectedChannel);

    if (!result.lastError.isEmpty()) {
        showError("处理失败", result.lastError);
        return;
    }

    // 更新处理结果
    processedData = result.processedData;
    linearityData = result.linearityData;

    // 显示处理结果
    displayDataInTable(ui.tableProcessedData, processedData);
    displayDataInTable(ui.tableLinearityData, linearityData);

    // 更新统计信息
    QString basicStats = statisticsCalculator->generateStatistics(currentFilePath, originalData, "");
    QString fullStats = basicStats + QString::fromUtf8("\n\n========== %1 通道计算详情 ==========\n").arg(selectedChannel) + result.calculationDetails;
    ui.textStatistics->setPlainText(fullStats);

    // 重新添加解耦矩阵信息（因为统计信息被重建了）
    calculateAndDisplayDecouplingMatrix();

    // 切换到处理结果标签页
    ui.tabWidget->setCurrentIndex(1);
    updateStatus(QString::fromUtf8("%1 通道数据处理完成").arg(selectedChannel));
}

QString Data_Solve::getSelectedChannel() const
{
    if (ui.radioFx->isChecked()) return "Fx";
    if (ui.radioFy->isChecked()) return "Fy";
    if (ui.radioFz->isChecked()) return "Fz";
    if (ui.radioMx->isChecked()) return "Mx";
    if (ui.radioMy->isChecked()) return "My";
    if (ui.radioMz->isChecked()) return "Mz";

    // 默认返回 Fx（保持向后兼容）
    return "Fx";
}

void Data_Solve::onExportDataClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出数据",
        QDir::currentPath(),
        "CSV Files (*.csv);;Excel Files (*.xlsx);;All Files (*)");

    if (!fileName.isEmpty()) {
        updateStatus("正在导出数据...");

        bool success = false;
        QVector<QStringList> dataToExport;

        // 如果没有原始数据，只导出原始数据
        if (originalData.isEmpty()) {
            showError("导出失败", "没有数据可导出");
            return;
        }

        // 如果用户已经处理了数据，询问是导出单通道还是所有通道
        if (!processedData.isEmpty()) {
            int ret = QMessageBox::question(this, QString::fromUtf8("选择导出方式"),
                QString::fromUtf8("是否导出所有6个通道的处理结果？\n\n"
                "选择\"是\"：导出Fx、Fy、Fz、Mx、My、Mz所有通道\n"
                "选择\"否\"：仅导出当前已处理的通道"),
                QMessageBox::Yes | QMessageBox::No);

            if (ret == QMessageBox::Yes) {
                // 导出所有通道
                exportAllChannels(fileName);
                return;
            } else {
                // 导出当前通道
                exportCurrentChannel(fileName);
                return;
            }
        } else {
            // 没有处理数据，导出原始数据
            dataToExport = originalData;
        }

        if (fileName.endsWith(".csv")) {
            success = dataExporter->exportToCSV(fileName, dataToExport);
        } else if (fileName.endsWith(".xlsx")) {
            success = dataExporter->exportToExcel(fileName, dataToExport);
        } else {
            fileName += ".csv";
            success = dataExporter->exportToCSV(fileName, dataToExport);
        }

        if (success) {
            showInfo("导出成功", QString("数据已成功导出到:\n%1").arg(fileName));
            updateStatus("数据导出完成");
        } else {
            showError("导出失败", dataExporter->getLastError());
        }
    }
}

void Data_Solve::onClearDataClicked()
{
    if (!originalData.isEmpty() || !processedData.isEmpty()) {
        int ret = QMessageBox::question(this, "确认清空",
            "确定要清空所有数据吗？",
            QMessageBox::Yes | QMessageBox::No);

        if (ret == QMessageBox::Yes) {
            clearAllData();
            updateStatus("数据已清空");
        }
    }
}

void Data_Solve::clearAllData()
{
    originalData.clear();
    processedData.clear();
    linearityData.clear();
    currentFilePath.clear();

    ui.tableOriginalData->clear();
    ui.tableOriginalData->setRowCount(0);
    ui.tableOriginalData->setColumnCount(0);

    ui.tableProcessedData->clear();
    ui.tableProcessedData->setRowCount(0);
    ui.tableProcessedData->setColumnCount(0);

    ui.tableLinearityData->clear();
    ui.tableLinearityData->setRowCount(0);
    ui.tableLinearityData->setColumnCount(0);

    ui.tableDecouplingMatrix->clear();
    ui.tableDecouplingMatrix->setRowCount(0);
    ui.tableDecouplingMatrix->setColumnCount(0);

    ui.tableSecondMatrix->clear();
    ui.tableSecondMatrix->setRowCount(0);
    ui.tableSecondMatrix->setColumnCount(0);

    ui.tableThirdMatrix->clear();
    ui.tableThirdMatrix->setRowCount(0);
    ui.tableThirdMatrix->setColumnCount(0);

    ui.textStatistics->clear();

    ui.btnProcessData->setEnabled(false);
    ui.btnExportData->setEnabled(false);
}

void Data_Solve::displayDataInTable(QTableWidget* table, const QVector<QStringList>& data)
{
    if (data.isEmpty()) {
        return;
    }

    table->clear();
    table->setRowCount(data.size() - 1);

    if (data.size() > 0) {
        QStringList headers = data[0];
        table->setColumnCount(headers.size());
        table->setHorizontalHeaderLabels(headers);

        for (int i = 1; i < data.size(); ++i) {
            const QStringList& row = data[i];
            for (int j = 0; j < row.size() && j < headers.size(); ++j) {
                QTableWidgetItem* item = new QTableWidgetItem(row[j]);
                table->setItem(i - 1, j, item);
            }
        }
    }

    table->resizeColumnsToContents();
}

void Data_Solve::onActionOpenTriggered()
{
    onLoadDataClicked();
}

void Data_Solve::onActionSaveTriggered()
{
    onExportDataClicked();
}

void Data_Solve::onActionExitTriggered()
{
    close();
}

void Data_Solve::onActionAboutTriggered()
{
    QMessageBox::about(this, "关于",
        "数据处理工具 v1.0\n\n"
        "这是一个用于处理Excel和CSV数据的工具\n"
        "支持数据加载、处理、统计和导出功能\n\n"
        "使用Qt 5.15.2开发\n"
        "采用模块化架构设计");
}

void Data_Solve::updateStatus(const QString& message)
{
    ui.lblStatus->setText(message);
    ui.statusBar->showMessage(message, 3000);
}

void Data_Solve::showError(const QString& title, const QString& message)
{
    QMessageBox::critical(this, title, message);
}

void Data_Solve::showInfo(const QString& title, const QString& message)
{
    QMessageBox::information(this, title, message);
}

void Data_Solve::calculateAndDisplayDecouplingMatrix()
{
    if (originalData.isEmpty()) {
        return;
    }

    // 计算解耦矩阵
    DecouplingResult result = dataProcessor->calculateDecouplingMatrix(originalData);

    // 在统计信息中添加矩阵计算说明
    QString matrixInfo = QString::fromUtf8("\n\n");
    matrixInfo += QString::fromUtf8("============================================================\n");
    matrixInfo += QString::fromUtf8("解耦矩阵计算说明\n");
    matrixInfo += QString::fromUtf8("============================================================\n\n");

    if (result.success) {
        matrixInfo += QString::fromUtf8("✓ 矩阵计算成功\n\n");

        matrixInfo += QString::fromUtf8("【第一矩阵 - 解耦矩阵】\n");
        matrixInfo += QString::fromUtf8("公式: D = U^T × F × (F^T × F)^(-1)\n");
        matrixInfo += QString::fromUtf8("说明: 将测量电压值转换为真实力/力矩值\n");
        matrixInfo += QString::fromUtf8("应用: 真实力 = 第一矩阵 × 测量电压\n\n");

        matrixInfo += QString::fromUtf8("【第二矩阵】\n");
        matrixInfo += QString::fromUtf8("公式: (D^T × D)^(-1) × D^T\n");
        matrixInfo += QString::fromUtf8("说明: Moore-Penrose伪逆矩阵\n");
        matrixInfo += QString::fromUtf8("应用: 用于反向校准和误差分析\n\n");

        matrixInfo += QString::fromUtf8("【第三矩阵】\n");
        matrixInfo += QString::fromUtf8("公式: F^T × U × (U^T × U)^(-1)\n");
        matrixInfo += QString::fromUtf8("说明: 从设定力值计算期望测量值\n");
        matrixInfo += QString::fromUtf8("应用: 期望电压 = 第三矩阵^(-1) × 设定力\n\n");

        matrixInfo += QString::fromUtf8("计算步骤:\n");
        matrixInfo += QString::fromUtf8("1. 从原始数据中提取U矩阵(测量值)和F矩阵(设定值)\n");
        matrixInfo += QString::fromUtf8("2. 计算各矩阵的转置 (U^T, F^T)\n");
        matrixInfo += QString::fromUtf8("3. 进行矩阵乘法运算\n");
        matrixInfo += QString::fromUtf8("4. 使用高斯-若尔当消元法计算逆矩阵\n");
        matrixInfo += QString::fromUtf8("5. 完成最终的矩阵乘法得到三个解耦矩阵\n\n");

        matrixInfo += QString::fromUtf8("注: 详细的矩阵数值请查看【解耦矩阵】标签页\n\n");

        // 添加量程使用情况
        matrixInfo += QString::fromUtf8("============================================================\n");
        matrixInfo += QString::fromUtf8("各通道量程 (用于误差归一化)\n");
        matrixInfo += QString::fromUtf8("============================================================\n\n");

        QStringList channelNames = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
        for (int i = 0; i < 6; ++i) {
            matrixInfo += QString::fromUtf8("%1 量程: %2\n")
                .arg(channelNames[i])
                .arg(result.channelRanges[i], 0, 'f', 8);
        }

        matrixInfo += QString::fromUtf8("\n说明: 使用手动设置的量程值（可在界面上方的【各通道量程设置】区域修改）\n\n");

        // 添加各通道纯通道测试数据行数统计
        matrixInfo += QString::fromUtf8("============================================================\n");
        matrixInfo += QString::fromUtf8("各通道纯通道测试数据行数统计\n");
        matrixInfo += QString::fromUtf8("============================================================\n\n");

        for (int i = 0; i < 6; ++i) {
            matrixInfo += QString::fromUtf8("%1 通道: %2 行\n")
                .arg(channelNames[i])
                .arg(result.channelCounts[i]);
        }

        matrixInfo += QString::fromUtf8("\n说明: 统计归一化平方误差数据中各通道的纯通道测试数据行数\n\n");

        // 添加误差计算过程说明
        matrixInfo += QString::fromUtf8("============================================================\n");
        matrixInfo += QString::fromUtf8("误差分析计算过程\n");
        matrixInfo += QString::fromUtf8("============================================================\n\n");

        matrixInfo += QString::fromUtf8("【步骤1: 计算原始误差】\n");
        matrixInfo += QString::fromUtf8("  公式: 误差 = 设定值 - 解耦后值\n");
        matrixInfo += QString::fromUtf8("  例如: Fx误差 = Fx - Fx'\n");
        matrixInfo += QString::fromUtf8("  结果: 查看【误差分析(原始)】标签页\n\n");

        matrixInfo += QString::fromUtf8("【步骤2: 归一化平方误差】\n");
        matrixInfo += QString::fromUtf8("  公式: 归一化平方误差 = ((误差 / 量程)²)\n");
        matrixInfo += QString::fromUtf8("  目的: 消除量纲影响，使各通道误差可比\n");
        matrixInfo += QString::fromUtf8("  结果: 查看【归一化平方误差】标签页\n\n");

        matrixInfo += QString::fromUtf8("【后续步骤】\n");
        matrixInfo += QString::fromUtf8("  - I类误差: 基于归一化平方误差计算\n");
        matrixInfo += QString::fromUtf8("  - II类误差: 基于归一化平方误差计算\n");
    } else {
        matrixInfo += QString::fromUtf8("✗ 矩阵计算失败\n\n");
        matrixInfo += QString::fromUtf8("错误信息: ") + result.errorMessage + "\n\n";
        matrixInfo += QString::fromUtf8("可能原因:\n");
        matrixInfo += QString::fromUtf8("- 数据不足（至少需要6行有效数据）\n");
        matrixInfo += QString::fromUtf8("- 缺少必需的通道列（Fx-Mz, UFx-UMz）\n");
        matrixInfo += QString::fromUtf8("- 矩阵奇异，无法求逆\n");
    }

    // 追加到统计信息
    QString currentStats = ui.textStatistics->toPlainText();
    ui.textStatistics->setPlainText(currentStats + matrixInfo);

    // 设置第一矩阵表格
    ui.tableDecouplingMatrix->clear();
    ui.tableDecouplingMatrix->setRowCount(6);
    ui.tableDecouplingMatrix->setColumnCount(6);

    // 设置第二矩阵表格
    ui.tableSecondMatrix->clear();
    ui.tableSecondMatrix->setRowCount(6);
    ui.tableSecondMatrix->setColumnCount(6);

    // 设置第三矩阵表格
    ui.tableThirdMatrix->clear();
    ui.tableThirdMatrix->setRowCount(6);
    ui.tableThirdMatrix->setColumnCount(6);

    // 设置表头
    QStringList rowHeaders = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
    QStringList colHeaders = {"UFx", "UFy", "UFz", "UMx", "UMy", "UMz"};

    ui.tableDecouplingMatrix->setHorizontalHeaderLabels(colHeaders);
    ui.tableDecouplingMatrix->setVerticalHeaderLabels(rowHeaders);

    ui.tableSecondMatrix->setHorizontalHeaderLabels(colHeaders);
    ui.tableSecondMatrix->setVerticalHeaderLabels(rowHeaders);

    ui.tableThirdMatrix->setHorizontalHeaderLabels(colHeaders);
    ui.tableThirdMatrix->setVerticalHeaderLabels(rowHeaders);

    if (result.success) {
        // 填充第一矩阵数据
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                QTableWidgetItem* item = new QTableWidgetItem(
                    QString::number(result.matrix[i][j], 'f', 8)
                );
                item->setTextAlignment(Qt::AlignCenter);
                ui.tableDecouplingMatrix->setItem(i, j, item);
            }
        }

        // 填充第二矩阵数据
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                QTableWidgetItem* item = new QTableWidgetItem(
                    QString::number(result.secondMatrix[i][j], 'f', 8)
                );
                item->setTextAlignment(Qt::AlignCenter);
                ui.tableSecondMatrix->setItem(i, j, item);
            }
        }

        // 填充第三矩阵数据
        for (int i = 0; i < 6; ++i) {
            for (int j = 0; j < 6; ++j) {
                QTableWidgetItem* item = new QTableWidgetItem(
                    QString::number(result.thirdMatrix[i][j], 'f', 8)
                );
                item->setTextAlignment(Qt::AlignCenter);
                ui.tableThirdMatrix->setItem(i, j, item);
            }
        }

        // 调整列宽
        ui.tableDecouplingMatrix->resizeColumnsToContents();
        ui.tableSecondMatrix->resizeColumnsToContents();
        ui.tableThirdMatrix->resizeColumnsToContents();

        // 显示解耦后的数据
        displayDataInTable(ui.tableDecoupledData, result.decoupledData);

        // 显示原始误差数据
        displayDataInTable(ui.tableErrorAnalysis, result.errorData);

        // 显示归一化平方误差数据
        displayDataInTable(ui.tableNormalizedError, result.normalizedSquaredError);

        // 显示通道统计值数据
        displayDataInTable(ui.tableChannelStatistics, result.channelStatistics);

        updateStatus(QString::fromUtf8("解耦矩阵计算成功"));
    } else {
        // 显示错误
        QTableWidgetItem* errorItem1 = new QTableWidgetItem(
            QString::fromUtf8("计算失败: ") + result.errorMessage
        );
        errorItem1->setTextAlignment(Qt::AlignCenter);
        ui.tableDecouplingMatrix->setItem(0, 0, errorItem1);
        ui.tableDecouplingMatrix->setSpan(0, 0, 6, 6);

        QTableWidgetItem* errorItem2 = new QTableWidgetItem(
            QString::fromUtf8("计算失败: ") + result.errorMessage
        );
        errorItem2->setTextAlignment(Qt::AlignCenter);
        ui.tableSecondMatrix->setItem(0, 0, errorItem2);
        ui.tableSecondMatrix->setSpan(0, 0, 6, 6);

        QTableWidgetItem* errorItem3 = new QTableWidgetItem(
            QString::fromUtf8("计算失败: ") + result.errorMessage
        );
        errorItem3->setTextAlignment(Qt::AlignCenter);
        ui.tableThirdMatrix->setItem(0, 0, errorItem3);
        ui.tableThirdMatrix->setSpan(0, 0, 6, 6);

        updateStatus(QString::fromUtf8("解耦矩阵计算失败"));
    }
}

void Data_Solve::exportCurrentChannel(const QString& fileName)
{
    QVector<QStringList> dataToExport;

    // 合并重复性误差和线性度数据
    if (!processedData.isEmpty() && !linearityData.isEmpty()) {
        // 添加重复性误差数据
        dataToExport = processedData;

        // 添加空行分隔
        QStringList emptyRow;
        for (int i = 0; i < processedData[0].size(); ++i) {
            emptyRow << "";
        }
        dataToExport.append(emptyRow);

        // 添加线性度数据标题
        QStringList linearityTitle;
        linearityTitle << QString::fromUtf8("========== 线性度数据 ==========");
        for (int i = 1; i < processedData[0].size(); ++i) {
            linearityTitle << "";
        }
        dataToExport.append(linearityTitle);

        // 添加线性度数据
        dataToExport.append(linearityData);
    } else if (!processedData.isEmpty()) {
        dataToExport = processedData;
    } else {
        showError("导出失败", "没有处理后的数据可导出");
        return;
    }

    bool success = false;
    if (fileName.endsWith(".csv")) {
        success = dataExporter->exportToCSV(fileName, dataToExport);
    } else if (fileName.endsWith(".xlsx")) {
        success = dataExporter->exportToExcel(fileName, dataToExport);
    } else {
        QString csvFileName = fileName + ".csv";
        success = dataExporter->exportToCSV(csvFileName, dataToExport);
    }

    if (success) {
        showInfo("导出成功", QString("数据已成功导出到:\n%1").arg(fileName));
        updateStatus("数据导出完成");
    } else {
        showError("导出失败", dataExporter->getLastError());
    }
}

void Data_Solve::exportAllChannels(const QString& fileName)
{
    updateStatus("正在处理所有通道并导出...");

    QVector<QStringList> allChannelsData;
    QStringList channels = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
    QMap<QString, ProcessResult> channelResults;  // 存储每个通道的结果用于汇总表

    // 先计算解耦矩阵（用于获取I类/II类误差）
    DecouplingResult decouplingResult = dataProcessor->calculateDecouplingMatrix(originalData);

    // ========== 第一部分：性能指标评价表 ==========
    QStringList emptyRow;
    for (int i = 0; i < 9; ++i) {
        emptyRow << "";
    }

    // 性能指标评价标题
    QStringList performanceTitle;
    performanceTitle << QString::fromUtf8("========== 性能指标评价 ==========");
    for (int i = 1; i < 9; ++i) {
        performanceTitle << "";
    }
    allChannelsData.append(performanceTitle);
    allChannelsData.append(emptyRow);

    // 先处理所有通道以获取性能指标
    for (const QString& channel : channels) {
        // 处理当前通道
        ProcessResult result = dataProcessor->processChannelData(originalData, channel);

        if (!result.lastError.isEmpty()) {
            showError(QString::fromUtf8("处理%1失败").arg(channel), result.lastError);
            continue;
        }

        // 保存结果用于汇总
        channelResults[channel] = result;
    }

    // 性能指标表头
    QStringList performanceHeader;
    performanceHeader << "" << "Fx" << "Fy" << "Fz" << "Mx" << "My" << "Mz";
    for (int i = 7; i < 9; ++i) {
        performanceHeader << "";
    }
    allChannelsData.append(performanceHeader);

    // 重复性R行
    QStringList repeatabilityRow;
    repeatabilityRow << QString::fromUtf8("重复性R（%FS）");
    for (const QString& channel : channels) {
        if (channelResults.contains(channel)) {
            repeatabilityRow << QString::number(channelResults[channel].repeatability, 'f', 8);
        } else {
            repeatabilityRow << "";
        }
    }
    for (int i = 7; i < 9; ++i) {
        repeatabilityRow << "";
    }
    allChannelsData.append(repeatabilityRow);

    // 迟滞性H行
    QStringList hysteresisRow;
    hysteresisRow << QString::fromUtf8("迟滞性H（%FS）");
    for (const QString& channel : channels) {
        if (channelResults.contains(channel)) {
            hysteresisRow << QString::number(channelResults[channel].hysteresis, 'f', 8);
        } else {
            hysteresisRow << "";
        }
    }
    for (int i = 7; i < 9; ++i) {
        hysteresisRow << "";
    }
    allChannelsData.append(hysteresisRow);

    // 线性度L行
    QStringList linearityRow;
    linearityRow << QString::fromUtf8("线性度L（%FS）");
    for (const QString& channel : channels) {
        if (channelResults.contains(channel)) {
            linearityRow << QString::number(channelResults[channel].linearity, 'f', 8);
        } else {
            linearityRow << "";
        }
    }
    for (int i = 7; i < 9; ++i) {
        linearityRow << "";
    }
    allChannelsData.append(linearityRow);

    // I类误差行（传感器级别误差）
    if (decouplingResult.success && !decouplingResult.channelStatistics.isEmpty()) {
        // 找到I类误差行（倒数第二行）
        int iErrorRowIndex = -1;
        for (int i = 0; i < decouplingResult.channelStatistics.size(); ++i) {
            if (decouplingResult.channelStatistics[i].size() > 0 &&
                decouplingResult.channelStatistics[i][0] == QString::fromUtf8("I类误差")) {
                iErrorRowIndex = i;
                break;
            }
        }

        if (iErrorRowIndex >= 0 && iErrorRowIndex < decouplingResult.channelStatistics.size()) {
            QStringList iErrorRow;
            iErrorRow << QString::fromUtf8("【传感器I类误差】（%FS）");
            if (decouplingResult.channelStatistics[iErrorRowIndex].size() > 1) {
                iErrorRow << decouplingResult.channelStatistics[iErrorRowIndex][1];
            } else {
                iErrorRow << "";
            }
            for (int i = 2; i < 9; ++i) {
                iErrorRow << "";
            }
            allChannelsData.append(iErrorRow);
        }
    }

    // II类误差行（传感器级别误差）
    if (decouplingResult.success && !decouplingResult.channelStatistics.isEmpty()) {
        // 找到II类误差行（最后一行）
        int iiErrorRowIndex = -1;
        for (int i = 0; i < decouplingResult.channelStatistics.size(); ++i) {
            if (decouplingResult.channelStatistics[i].size() > 0 &&
                decouplingResult.channelStatistics[i][0] == QString::fromUtf8("II类误差")) {
                iiErrorRowIndex = i;
                break;
            }
        }

        if (iiErrorRowIndex >= 0 && iiErrorRowIndex < decouplingResult.channelStatistics.size()) {
            QStringList iiErrorRow;
            iiErrorRow << QString::fromUtf8("【传感器II类误差】（%FS）");
            if (decouplingResult.channelStatistics[iiErrorRowIndex].size() > 1) {
                iiErrorRow << decouplingResult.channelStatistics[iiErrorRowIndex][1];
            } else {
                iiErrorRow << "";
            }
            for (int i = 2; i < 9; ++i) {
                iiErrorRow << "";
            }
            allChannelsData.append(iiErrorRow);
        }
    }

    allChannelsData.append(emptyRow);
    allChannelsData.append(emptyRow);
    allChannelsData.append(emptyRow);

    // ========== 第二部分：详细数据 ==========
    for (const QString& channel : channels) {
        if (!channelResults.contains(channel)) continue;

        // 添加通道分隔标题
        QStringList channelTitle;
        channelTitle << QString::fromUtf8("========== %1 通道 ==========").arg(channel);
        for (int i = 1; i < 9; ++i) {  // 填充空列
            channelTitle << "";
        }
        allChannelsData.append(channelTitle);

        // 添加重复性误差数据标题
        QStringList repeatTitle;
        repeatTitle << QString::fromUtf8("重复性误差");
        for (int i = 1; i < 9; ++i) {
            repeatTitle << "";
        }
        allChannelsData.append(repeatTitle);

        // 添加重复性误差数据
        allChannelsData.append(channelResults[channel].processedData);

        // 添加空行
        allChannelsData.append(emptyRow);

        // 添加线性度数据标题
        QStringList linearityTitle;
        linearityTitle << QString::fromUtf8("线性度数据");
        for (int i = 1; i < 9; ++i) {
            linearityTitle << "";
        }
        allChannelsData.append(linearityTitle);

        // 添加线性度数据
        allChannelsData.append(channelResults[channel].linearityData);

        // 添加两个空行作为通道间分隔
        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        updateStatus(QString::fromUtf8("已处理 %1 通道...").arg(channel));
    }

    // ========== 第三部分：解耦矩阵和误差分析 ==========
    allChannelsData.append(emptyRow);
    allChannelsData.append(emptyRow);

    if (decouplingResult.success) {
        // 第一矩阵标题
        QStringList firstMatrixTitle;
        firstMatrixTitle << QString::fromUtf8("========== 第一矩阵 (解耦矩阵) ==========");
        for (int i = 1; i < 9; ++i) {
            firstMatrixTitle << "";
        }
        allChannelsData.append(firstMatrixTitle);

        // 第一矩阵表头
        QStringList firstMatrixHeader;
        firstMatrixHeader << "" << "UFx" << "UFy" << "UFz" << "UMx" << "UMy" << "UMz";
        for (int i = 7; i < 9; ++i) {
            firstMatrixHeader << "";
        }
        allChannelsData.append(firstMatrixHeader);

        // 第一矩阵数据
        QStringList rowLabels = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
        for (int i = 0; i < 6; ++i) {
            QStringList matrixRow;
            matrixRow << rowLabels[i];
            for (int j = 0; j < 6; ++j) {
                matrixRow << QString::number(decouplingResult.matrix[i][j], 'f', 8);
            }
            for (int k = 7; k < 9; ++k) {
                matrixRow << "";
            }
            allChannelsData.append(matrixRow);
        }

        // 添加空行
        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 第二矩阵标题
        QStringList secondMatrixTitle;
        secondMatrixTitle << QString::fromUtf8("========== 第二矩阵 ==========");
        for (int i = 1; i < 9; ++i) {
            secondMatrixTitle << "";
        }
        allChannelsData.append(secondMatrixTitle);

        // 第二矩阵表头
        QStringList secondMatrixHeader;
        secondMatrixHeader << "" << "UFx" << "UFy" << "UFz" << "UMx" << "UMy" << "UMz";
        for (int i = 7; i < 9; ++i) {
            secondMatrixHeader << "";
        }
        allChannelsData.append(secondMatrixHeader);

        // 第二矩阵数据
        for (int i = 0; i < 6; ++i) {
            QStringList matrixRow;
            matrixRow << rowLabels[i];
            for (int j = 0; j < 6; ++j) {
                matrixRow << QString::number(decouplingResult.secondMatrix[i][j], 'f', 8);
            }
            for (int k = 7; k < 9; ++k) {
                matrixRow << "";
            }
            allChannelsData.append(matrixRow);
        }

        // 添加空行
        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 第三矩阵标题
        QStringList thirdMatrixTitle;
        thirdMatrixTitle << QString::fromUtf8("========== 第三矩阵 ==========");
        for (int i = 1; i < 9; ++i) {
            thirdMatrixTitle << "";
        }
        allChannelsData.append(thirdMatrixTitle);

        // 第三矩阵表头
        QStringList thirdMatrixHeader;
        thirdMatrixHeader << "" << "UFx" << "UFy" << "UFz" << "UMx" << "UMy" << "UMz";
        for (int i = 7; i < 9; ++i) {
            thirdMatrixHeader << "";
        }
        allChannelsData.append(thirdMatrixHeader);

        // 第三矩阵数据
        for (int i = 0; i < 6; ++i) {
            QStringList matrixRow;
            matrixRow << rowLabels[i];
            for (int j = 0; j < 6; ++j) {
                matrixRow << QString::number(decouplingResult.thirdMatrix[i][j], 'f', 8);
            }
            for (int k = 7; k < 9; ++k) {
                matrixRow << "";
            }
            allChannelsData.append(matrixRow);
        }

        // 添加空行
        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 解耦后数据标题
        QStringList decoupledDataTitle;
        decoupledDataTitle << QString::fromUtf8("========== 解耦后数据 (Fx', Fy', Fz', Mx', My', Mz') ==========");
        for (int i = 1; i < 9; ++i) {
            decoupledDataTitle << "";
        }
        allChannelsData.append(decoupledDataTitle);

        // 添加解耦后数据
        if (!decouplingResult.decoupledData.isEmpty()) {
            for (const QStringList& row : decouplingResult.decoupledData) {
                QStringList exportRow = row;
                // 补齐到9列
                while (exportRow.size() < 9) {
                    exportRow << "";
                }
                allChannelsData.append(exportRow);
            }
        }

        // 添加空行
        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // I类误差/II类误差标题
        QStringList errorDataTitle;
        errorDataTitle << QString::fromUtf8("========== I类误差/II类误差 (Fx-Fx', Fy-Fy', ...) ==========");
        for (int i = 1; i < 9; ++i) {
            errorDataTitle << "";
        }
        allChannelsData.append(errorDataTitle);

        // 添加原始误差数据
        if (!decouplingResult.errorData.isEmpty()) {
            for (const QStringList& row : decouplingResult.errorData) {
                QStringList exportRow = row;
                // 补齐到9列
                while (exportRow.size() < 9) {
                    exportRow << "";
                }
                allChannelsData.append(exportRow);
            }
        }

        // 添加空行
        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 归一化平方误差标题
        QStringList normalizedErrorTitle;
        normalizedErrorTitle << QString::fromUtf8("========== 归一化平方误差 ((误差/量程)²) ==========");
        for (int i = 1; i < 9; ++i) {
            normalizedErrorTitle << "";
        }
        allChannelsData.append(normalizedErrorTitle);

        // 添加归一化平方误差数据
        if (!decouplingResult.normalizedSquaredError.isEmpty()) {
            for (const QStringList& row : decouplingResult.normalizedSquaredError) {
                QStringList exportRow = row;
                // 补齐到9列
                while (exportRow.size() < 9) {
                    exportRow << "";
                }
                allChannelsData.append(exportRow);
            }
        }

        // 添加空行
        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 通道统计值标题
        QStringList channelStatisticsTitle;
        channelStatisticsTitle << QString::fromUtf8("========== 通道统计值 (I类/II类误差) ==========");
        for (int i = 1; i < 9; ++i) {
            channelStatisticsTitle << "";
        }
        allChannelsData.append(channelStatisticsTitle);

        // 添加通道统计值数据
        if (!decouplingResult.channelStatistics.isEmpty()) {
            for (const QStringList& row : decouplingResult.channelStatistics) {
                QStringList exportRow = row;
                // 补齐到9列
                while (exportRow.size() < 9) {
                    exportRow << "";
                }
                allChannelsData.append(exportRow);
            }
        }
    } else {
        // 如果计算失败，显示错误信息
        QStringList errorTitle;
        errorTitle << QString::fromUtf8("========== 解耦矩阵计算失败 ==========");
        for (int i = 1; i < 9; ++i) {
            errorTitle << "";
        }
        allChannelsData.append(errorTitle);

        QStringList errorRow;
        errorRow << QString::fromUtf8("错误: ") + decouplingResult.errorMessage;
        for (int i = 1; i < 9; ++i) {
            errorRow << "";
        }
        allChannelsData.append(errorRow);
    }

    // 导出合并后的数据
    bool success = false;
    if (fileName.endsWith(".csv")) {
        success = dataExporter->exportToCSV(fileName, allChannelsData);
    } else if (fileName.endsWith(".xlsx")) {
        success = dataExporter->exportToExcel(fileName, allChannelsData);
    } else {
        QString csvFileName = fileName + ".csv";
        success = dataExporter->exportToCSV(csvFileName, allChannelsData);
    }

    if (success) {
        showInfo("导出成功", QString::fromUtf8("所有6个通道的数据已成功导出到:\n%1").arg(fileName));
        updateStatus("所有通道数据导出完成");
    } else {
        showError("导出失败", dataExporter->getLastError());
    }
}

// ==================== 量程设置功能 ====================

void Data_Solve::onSetRangeClicked()
{
    // 创建量程设置对话框
    QDialog dialog(this);
    dialog.setWindowTitle(QString::fromUtf8("设置各通道量程"));
    dialog.setMinimumWidth(450);

    QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);

    // 说明文字
    QLabel* infoLabel = new QLabel(QString::fromUtf8(
        "设置各通道的最大量程值，用于归一化计算：\n"
        "• 默认量程: Fx=1000, Fy=1000, Fz=1500, Mx=My=Mz=50\n"
        "• 量程设置立即生效，应用于后续所有计算"
    ), &dialog);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("QLabel { padding: 10px; background-color: #f0f0f0; border-radius: 5px; }");
    mainLayout->addWidget(infoLabel);

    // 输入框网格
    QGridLayout* gridLayout = new QGridLayout();

    // 创建6个通道的输入框
    QDoubleSpinBox* spinFx = new QDoubleSpinBox(&dialog);
    QDoubleSpinBox* spinFy = new QDoubleSpinBox(&dialog);
    QDoubleSpinBox* spinFz = new QDoubleSpinBox(&dialog);
    QDoubleSpinBox* spinMx = new QDoubleSpinBox(&dialog);
    QDoubleSpinBox* spinMy = new QDoubleSpinBox(&dialog);
    QDoubleSpinBox* spinMz = new QDoubleSpinBox(&dialog);

    // 设置范围和当前值
    QList<QDoubleSpinBox*> spinBoxes = {spinFx, spinFy, spinFz, spinMx, spinMy, spinMz};
    QStringList channels = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};

    for (int i = 0; i < spinBoxes.size(); ++i) {
        spinBoxes[i]->setMinimum(0.1);
        spinBoxes[i]->setMaximum(100000.0);
        spinBoxes[i]->setSingleStep(10.0);
        spinBoxes[i]->setDecimals(1);
        spinBoxes[i]->setValue(dataProcessor->getChannelRange(channels[i]));
        spinBoxes[i]->setMinimumWidth(120);
    }

    // 布局（3列×2行）
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Fx 量程:"), &dialog), 0, 0);
    gridLayout->addWidget(spinFx, 0, 1);
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Fy 量程:"), &dialog), 0, 2);
    gridLayout->addWidget(spinFy, 0, 3);
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Fz 量程:"), &dialog), 0, 4);
    gridLayout->addWidget(spinFz, 0, 5);

    gridLayout->addWidget(new QLabel(QString::fromUtf8("Mx 量程:"), &dialog), 1, 0);
    gridLayout->addWidget(spinMx, 1, 1);
    gridLayout->addWidget(new QLabel(QString::fromUtf8("My 量程:"), &dialog), 1, 2);
    gridLayout->addWidget(spinMy, 1, 3);
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Mz 量程:"), &dialog), 1, 4);
    gridLayout->addWidget(spinMz, 1, 5);

    mainLayout->addLayout(gridLayout);

    // 按钮区域
    QDialogButtonBox* buttonBox = new QDialogButtonBox(&dialog);
    QPushButton* applyButton = buttonBox->addButton(QString::fromUtf8("应用"), QDialogButtonBox::AcceptRole);
    QPushButton* resetButton = buttonBox->addButton(QString::fromUtf8("恢复默认"), QDialogButtonBox::ResetRole);
    QPushButton* cancelButton = buttonBox->addButton(QString::fromUtf8("取消"), QDialogButtonBox::RejectRole);

    mainLayout->addWidget(buttonBox);

    // 连接信号
    connect(applyButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(resetButton, &QPushButton::clicked, [&]() {
        spinFx->setValue(1000.0);
        spinFy->setValue(1000.0);
        spinFz->setValue(1500.0);
        spinMx->setValue(50.0);
        spinMy->setValue(50.0);
        spinMz->setValue(50.0);
    });

    // 显示对话框
    if (dialog.exec() == QDialog::Accepted) {
        // 应用新的量程值
        dataProcessor->setAllChannelRanges(
            spinFx->value(),
            spinFy->value(),
            spinFz->value(),
            spinMx->value(),
            spinMy->value(),
            spinMz->value()
        );

        // 更新主界面显示
        updateRangeDisplay();

        updateStatus(QString::fromUtf8("量程已更新: Fx=%1, Fy=%2, Fz=%3, Mx=%4, My=%5, Mz=%6")
            .arg(spinFx->value(), 0, 'f', 1)
            .arg(spinFy->value(), 0, 'f', 1)
            .arg(spinFz->value(), 0, 'f', 1)
            .arg(spinMx->value(), 0, 'f', 1)
            .arg(spinMy->value(), 0, 'f', 1)
            .arg(spinMz->value(), 0, 'f', 1)
        );

        showInfo(QString::fromUtf8("设置成功"), QString::fromUtf8("各通道量程已更新，将应用于后续所有计算"));
    }
}

void Data_Solve::updateRangeDisplay()
{
    // 从 dataProcessor 读取当前量程并显示到UI（LED数字显示样式）
    ui.displayRangeFx->setText(QString::number(dataProcessor->getChannelRange("Fx"), 'f', 1));
    ui.displayRangeFy->setText(QString::number(dataProcessor->getChannelRange("Fy"), 'f', 1));
    ui.displayRangeFz->setText(QString::number(dataProcessor->getChannelRange("Fz"), 'f', 1));
    ui.displayRangeMx->setText(QString::number(dataProcessor->getChannelRange("Mx"), 'f', 1));
    ui.displayRangeMy->setText(QString::number(dataProcessor->getChannelRange("My"), 'f', 1));
    ui.displayRangeMz->setText(QString::number(dataProcessor->getChannelRange("Mz"), 'f', 1));
}