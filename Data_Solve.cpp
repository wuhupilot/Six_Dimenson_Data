#include "Data_Solve.h"
#include <QDir>
#include <QDateTime>
#include <QInputDialog>
#include <QVBoxLayout>
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
    exportManager = new ExportManager(dataProcessor, dataExporter);
    matrixDisplayHelper = new MatrixDisplayHelper();
    dataAverager = new DataAverager();
    dataZeroCalibrator = new DataZeroCalibrator();

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
    delete exportManager;
    delete matrixDisplayHelper;
    delete dataAverager;
    delete dataZeroCalibrator;
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

    // 量程设置按钮连接
    connect(ui.btnSetRange, &QPushButton::clicked, this, &Data_Solve::onSetRangeClicked);

    // 数据平均按钮连接（如果UI中有该按钮，取消注释）
     connect(ui.btnAverageData, &QPushButton::clicked, this, &Data_Solve::onAverageDataClicked);

    // 置零校准按钮连接（如果UI中有该按钮，取消注释）
     connect(ui.btnZeroCalibrate, &QPushButton::clicked, this, &Data_Solve::onZeroCalibrateClicked);

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
            matrixDisplayHelper->displayDataInTable(ui.tableOriginalData, originalData);
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
    matrixDisplayHelper->displayDataInTable(ui.tableProcessedData, processedData);
    matrixDisplayHelper->displayDataInTable(ui.tableLinearityData, linearityData);

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

    if (fileName.isEmpty()) {
        return;
    }

    updateStatus("正在导出数据...");

    // 如果没有原始数据
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

        bool success = false;
        if (ret == QMessageBox::Yes) {
            // 导出所有通道
            success = exportManager->exportAllChannels(fileName, originalData);
        } else {
            // 导出当前通道
            success = exportManager->exportCurrentChannel(fileName, processedData, linearityData);
        }

        if (success) {
            showInfo("导出成功", QString("数据已成功导出到:\n%1").arg(fileName));
            updateStatus("数据导出完成");
        } else {
            showError("导出失败", exportManager->getLastError());
        }
    } else {
        // 没有处理数据，导出原始数据
        bool success = false;
        if (fileName.endsWith(".csv")) {
            success = dataExporter->exportToCSV(fileName, originalData);
        } else if (fileName.endsWith(".xlsx")) {
            success = dataExporter->exportToExcel(fileName, originalData);
        } else {
            QString csvFileName = fileName + ".csv";
            success = dataExporter->exportToCSV(csvFileName, originalData);
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
        "数据处理工具 v2.0 (重构版)\n\n"
        "这是一个用于处理Excel和CSV数据的工具\n"
        "支持数据加载、处理、统计和导出功能\n\n"
        "使用Qt 5.15.2开发\n"
        "采用模块化架构设计\n\n"
        "重构更新:\n"
        "- ExportManager: 导出逻辑模块化\n"
        "- MatrixDisplayHelper: 矩阵显示模块化\n"
        "- RangeSettingsDialog: 量程设置对话框独立");
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

    // 使用 MatrixDisplayHelper 显示矩阵
    matrixDisplayHelper->displayDecouplingResult(
        result,
        ui.tableDecouplingMatrix,
        ui.tableSecondMatrix,
        ui.tableThirdMatrix,
        ui.tableDecoupledData,
        ui.tableErrorAnalysis,
        ui.tableNormalizedError,
        ui.tableChannelStatistics
    );

    // 生成并追加矩阵说明文本
    QString matrixInfo = matrixDisplayHelper->generateMatrixInfoText(result);
    QString currentStats = ui.textStatistics->toPlainText();
    ui.textStatistics->setPlainText(currentStats + matrixInfo);

    if (result.success) {
        updateStatus(QString::fromUtf8("解耦矩阵计算成功"));
    } else {
        updateStatus(QString::fromUtf8("解耦矩阵计算失败"));
    }
}

void Data_Solve::onSetRangeClicked()
{
    // 使用 RangeSettingsDialog 替代原来的内联对话框代码
    RangeSettingsDialog dialog(dataProcessor, this);

    if (dialog.exec() == QDialog::Accepted) {
        // 获取新的量程值
        QMap<QString, double> ranges = dialog.getRangeValues();

        // 更新主界面显示
        updateRangeDisplay();

        updateStatus(QString::fromUtf8("量程已更新: Fx=%1, Fy=%2, Fz=%3, Mx=%4, My=%5, Mz=%6")
            .arg(ranges["Fx"], 0, 'f', 1)
            .arg(ranges["Fy"], 0, 'f', 1)
            .arg(ranges["Fz"], 0, 'f', 1)
            .arg(ranges["Mx"], 0, 'f', 1)
            .arg(ranges["My"], 0, 'f', 1)
            .arg(ranges["Mz"], 0, 'f', 1)
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

void Data_Solve::onAverageDataClicked()
{
    if (originalData.isEmpty()) {
        showError(QString::fromUtf8("操作失败"), QString::fromUtf8("请先加载数据文件"));
        return;
    }

    // 询问用户分组大小
    bool ok;
    int groupSize = QInputDialog::getInt(
        this,
        QString::fromUtf8("数据平均设置"),
        QString::fromUtf8("请输入每组数据行数（将对每组求平均值）："),
        5,      // 默认值：5行一组
        1,      // 最小值
        100,    // 最大值
        1,      // 步长
        &ok
    );

    if (!ok) {
        return; // 用户取消
    }

    updateStatus(QString::fromUtf8("正在处理数据平均（每%1行一组）...").arg(groupSize));

    // 执行数据平均
    QVector<QStringList> averagedData = dataAverager->averageData(originalData, groupSize);

    if (averagedData.isEmpty() || averagedData.size() <= 1) {
        showError(QString::fromUtf8("平均失败"), dataAverager->getLastError());
        return;
    }

    // 询问用户是否替换原始数据
    int ret = QMessageBox::question(
        this,
        QString::fromUtf8("确认操作"),
        QString::fromUtf8("数据平均处理完成！\n\n%1\n\n是否用平均后的数据替换原始数据？\n"
            "点击\"是\"：替换原始数据（后续处理将使用平均后的数据）\n"
            "点击\"否\"：仅预览平均结果（不影响原始数据）")
            .arg(dataAverager->getProcessInfo()),
        QMessageBox::Yes | QMessageBox::No
    );

    if (ret == QMessageBox::Yes) {
        // 替换原始数据
        originalData = averagedData;
        matrixDisplayHelper->displayDataInTable(ui.tableOriginalData, originalData);

        // 清空已处理的数据
        processedData.clear();
        linearityData.clear();
        ui.tableProcessedData->clear();
        ui.tableLinearityData->clear();

        // 更新统计信息
        QString stats = statisticsCalculator->generateStatistics(currentFilePath, originalData, "");
        ui.textStatistics->setPlainText(stats);

        // 重新计算解耦矩阵
        calculateAndDisplayDecouplingMatrix();

        updateStatus(QString::fromUtf8("数据平均完成，原始数据已更新（%1行 → %2行）")
            .arg(averagedData.size() - 1 + (averagedData.size() - 1) * (groupSize - 1))
            .arg(averagedData.size() - 1));

        showInfo(QString::fromUtf8("处理完成"),
            QString::fromUtf8("数据平均完成！\n原始数据已更新为平均后的数据。\n可以继续进行数据处理。"));
    } else {
        // 仅预览，不替换
        // 创建一个临时对话框显示平均后的数据
        QDialog previewDialog(this);
        previewDialog.setWindowTitle(QString::fromUtf8("数据平均预览"));
        previewDialog.resize(800, 600);

        QVBoxLayout* layout = new QVBoxLayout(&previewDialog);

        QLabel* infoLabel = new QLabel(dataAverager->getProcessInfo(), &previewDialog);
        infoLabel->setStyleSheet("QLabel { padding: 10px; background-color: #e8f4f8; border-radius: 5px; }");
        layout->addWidget(infoLabel);

        QTableWidget* previewTable = new QTableWidget(&previewDialog);
        matrixDisplayHelper->displayDataInTable(previewTable, averagedData);
        layout->addWidget(previewTable);

        QPushButton* closeBtn = new QPushButton(QString::fromUtf8("关闭"), &previewDialog);
        connect(closeBtn, &QPushButton::clicked, &previewDialog, &QDialog::accept);
        layout->addWidget(closeBtn);

        previewDialog.exec();

        updateStatus(QString::fromUtf8("数据平均预览完成，原始数据未更改"));
    }
}

void Data_Solve::onZeroCalibrateClicked()
{
    if (originalData.isEmpty()) {
        showError(QString::fromUtf8("操作失败"), QString::fromUtf8("请先加载数据文件"));
        return;
    }

    // 询问用户零点判断阈值
    bool ok;
    double threshold = QInputDialog::getDouble(
        this,
        QString::fromUtf8("置零校准设置"),
        QString::fromUtf8("请输入零点判断阈值（Fx-Mz的绝对值小于此值视为零点）："),
        0.1,    // 默认值：0.1
        0.001,  // 最小值
        10.0,   // 最大值
        3,      // 小数位数
        &ok
    );

    if (!ok) {
        return; // 用户取消
    }

    updateStatus(QString::fromUtf8("正在执行置零校准（阈值=%1）...").arg(threshold));

    // 执行置零校准
    QVector<QStringList> calibratedData = dataZeroCalibrator->calibrateZeroPoints(originalData, threshold);

    if (calibratedData.isEmpty() || calibratedData.size() <= 1) {
        showError(QString::fromUtf8("校准失败"), dataZeroCalibrator->getLastError());
        return;
    }

    // 询问用户是否替换原始数据
    int ret = QMessageBox::question(
        this,
        QString::fromUtf8("确认操作"),
        QString::fromUtf8("置零校准完成！\n\n%1\n\n是否用校准后的数据替换原始数据？\n"
            "点击\"是\"：替换原始数据（后续处理将使用校准后的数据）\n"
            "点击\"否\"：仅预览校准结果（不影响原始数据）")
            .arg(dataZeroCalibrator->getProcessInfo()),
        QMessageBox::Yes | QMessageBox::No
    );

    if (ret == QMessageBox::Yes) {
        // 替换原始数据
        originalData = calibratedData;
        matrixDisplayHelper->displayDataInTable(ui.tableOriginalData, originalData);

        // 清空已处理的数据
        processedData.clear();
        linearityData.clear();
        ui.tableProcessedData->clear();
        ui.tableLinearityData->clear();

        // 更新统计信息
        QString stats = statisticsCalculator->generateStatistics(currentFilePath, originalData, "");
        ui.textStatistics->setPlainText(stats);

        // 重新计算解耦矩阵
        calculateAndDisplayDecouplingMatrix();

        updateStatus(QString::fromUtf8("置零校准完成，原始数据已更新"));

        showInfo(QString::fromUtf8("处理完成"),
            QString::fromUtf8("置零校准完成！\n原始数据已更新为校准后的数据。\n"
                "所有U值已相对于各测量循环组的第一个零点进行了校准。\n"
                "可以继续进行数据处理。"));
    } else {
        // 仅预览，不替换
        QDialog previewDialog(this);
        previewDialog.setWindowTitle(QString::fromUtf8("置零校准预览"));
        previewDialog.resize(900, 600);

        QVBoxLayout* layout = new QVBoxLayout(&previewDialog);

        QLabel* infoLabel = new QLabel(dataZeroCalibrator->getProcessInfo(), &previewDialog);
        infoLabel->setStyleSheet("QLabel { padding: 10px; background-color: #fff3cd; border-radius: 5px; }");
        layout->addWidget(infoLabel);

        QTableWidget* previewTable = new QTableWidget(&previewDialog);
        matrixDisplayHelper->displayDataInTable(previewTable, calibratedData);
        layout->addWidget(previewTable);

        QPushButton* closeBtn = new QPushButton(QString::fromUtf8("关闭"), &previewDialog);
        connect(closeBtn, &QPushButton::clicked, &previewDialog, &QDialog::accept);
        layout->addWidget(closeBtn);

        previewDialog.exec();

        updateStatus(QString::fromUtf8("置零校准预览完成，原始数据未更改"));
    }
}
