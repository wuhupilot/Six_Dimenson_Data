#include "DataSolverLib.h"
#include "DataLoader.h"
#include "DataProcessor.h"
#include "DataExporter.h"
#include "DataAverager.h"
#include "DataZeroCalibrator.h"
#include <QString>
#include <QFileInfo>
#include <QDir>
#include <QMap>
#include <cstring>

#pragma execution_character_set("utf-8")

// 全局数据处理器实例（用于保持量程设置）
static DataProcessor* g_dataProcessor = nullptr;
static DataExporter* g_dataExporter = nullptr;

// 初始化全局实例
static void EnsureGlobalInstancesInitialized() {
    if (!g_dataProcessor) {
        g_dataProcessor = new DataProcessor();
    }
    if (!g_dataExporter) {
        g_dataExporter = new DataExporter();
    }
}

// 辅助函数：设置错误信息
static void SetErrorMessage(char* errorMessage, int errorMessageSize, const QString& message) {
    if (errorMessage && errorMessageSize > 0) {
        QByteArray utf8 = message.toUtf8();
        int copySize = qMin(errorMessageSize - 1, utf8.size());
        memcpy(errorMessage, utf8.constData(), copySize);
        errorMessage[copySize] = '\0';
    }
}

// 辅助函数：导出所有通道数据（复制自Data_Solve::exportAllChannels逻辑）
static bool ExportAllChannelsData(
    const QVector<QStringList>& originalData,
    DataProcessor* dataProcessor,
    DataExporter* dataExporter,
    const QString& fileName,
    QString& errorMsg)
{
    QVector<QStringList> allChannelsData;
    QStringList channels = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
    QMap<QString, ProcessResult> channelResults;

    // 计算解耦矩阵
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
        ProcessResult result = dataProcessor->processChannelData(originalData, channel);
        if (!result.lastError.isEmpty()) {
            errorMsg = QString::fromUtf8("处理%1失败: %2").arg(channel).arg(result.lastError);
            continue;
        }
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

    // I类误差行
    if (decouplingResult.success && !decouplingResult.channelStatistics.isEmpty()) {
        int iErrorRowIndex = -1;
        for (int i = 0; i < decouplingResult.channelStatistics.size(); ++i) {
            if (decouplingResult.channelStatistics[i].size() > 0 &&
                decouplingResult.channelStatistics[i][0] == QString::fromUtf8("I类误差")) {
                iErrorRowIndex = i;
                break;
            }
        }

        if (iErrorRowIndex >= 0) {
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

    // II类误差行
    if (decouplingResult.success && !decouplingResult.channelStatistics.isEmpty()) {
        int iiErrorRowIndex = -1;
        for (int i = 0; i < decouplingResult.channelStatistics.size(); ++i) {
            if (decouplingResult.channelStatistics[i].size() > 0 &&
                decouplingResult.channelStatistics[i][0] == QString::fromUtf8("II类误差")) {
                iiErrorRowIndex = i;
                break;
            }
        }

        if (iiErrorRowIndex >= 0) {
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
        for (int i = 1; i < 9; ++i) {
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
    }

    // ========== 第三部分：解耦矩阵和误差分析 ==========
    allChannelsData.append(emptyRow);
    allChannelsData.append(emptyRow);

    if (decouplingResult.success) {
        // 第一矩阵
        QStringList firstMatrixTitle;
        firstMatrixTitle << QString::fromUtf8("========== 第一矩阵 (解耦矩阵) ==========");
        for (int i = 1; i < 9; ++i) {
            firstMatrixTitle << "";
        }
        allChannelsData.append(firstMatrixTitle);

        QStringList firstMatrixHeader;
        firstMatrixHeader << "" << "UFx" << "UFy" << "UFz" << "UMx" << "UMy" << "UMz";
        for (int i = 7; i < 9; ++i) {
            firstMatrixHeader << "";
        }
        allChannelsData.append(firstMatrixHeader);

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

        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 第二矩阵
        QStringList secondMatrixTitle;
        secondMatrixTitle << QString::fromUtf8("========== 第二矩阵 ==========");
        for (int i = 1; i < 9; ++i) {
            secondMatrixTitle << "";
        }
        allChannelsData.append(secondMatrixTitle);

        QStringList secondMatrixHeader;
        secondMatrixHeader << "" << "UFx" << "UFy" << "UFz" << "UMx" << "UMy" << "UMz";
        for (int i = 7; i < 9; ++i) {
            secondMatrixHeader << "";
        }
        allChannelsData.append(secondMatrixHeader);

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

        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 第三矩阵
        QStringList thirdMatrixTitle;
        thirdMatrixTitle << QString::fromUtf8("========== 第三矩阵 ==========");
        for (int i = 1; i < 9; ++i) {
            thirdMatrixTitle << "";
        }
        allChannelsData.append(thirdMatrixTitle);

        QStringList thirdMatrixHeader;
        thirdMatrixHeader << "" << "UFx" << "UFy" << "UFz" << "UMx" << "UMy" << "UMz";
        for (int i = 7; i < 9; ++i) {
            thirdMatrixHeader << "";
        }
        allChannelsData.append(thirdMatrixHeader);

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

        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 解耦后数据
        QStringList decoupledDataTitle;
        decoupledDataTitle << QString::fromUtf8("========== 解耦后数据 (Fx', Fy', Fz', Mx', My', Mz') ==========");
        for (int i = 1; i < 9; ++i) {
            decoupledDataTitle << "";
        }
        allChannelsData.append(decoupledDataTitle);

        if (!decouplingResult.decoupledData.isEmpty()) {
            for (const QStringList& row : decouplingResult.decoupledData) {
                QStringList exportRow = row;
                while (exportRow.size() < 9) {
                    exportRow << "";
                }
                allChannelsData.append(exportRow);
            }
        }

        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 原始误差数据
        QStringList errorDataTitle;
        errorDataTitle << QString::fromUtf8("========== I类误差/II类误差 (Fx-Fx', Fy-Fy', ...) ==========");
        for (int i = 1; i < 9; ++i) {
            errorDataTitle << "";
        }
        allChannelsData.append(errorDataTitle);

        if (!decouplingResult.errorData.isEmpty()) {
            for (const QStringList& row : decouplingResult.errorData) {
                QStringList exportRow = row;
                while (exportRow.size() < 9) {
                    exportRow << "";
                }
                allChannelsData.append(exportRow);
            }
        }

        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 归一化平方误差
        QStringList normalizedErrorTitle;
        normalizedErrorTitle << QString::fromUtf8("========== 归一化平方误差 ((误差/量程)²) ==========");
        for (int i = 1; i < 9; ++i) {
            normalizedErrorTitle << "";
        }
        allChannelsData.append(normalizedErrorTitle);

        if (!decouplingResult.normalizedSquaredError.isEmpty()) {
            for (const QStringList& row : decouplingResult.normalizedSquaredError) {
                QStringList exportRow = row;
                while (exportRow.size() < 9) {
                    exportRow << "";
                }
                allChannelsData.append(exportRow);
            }
        }

        allChannelsData.append(emptyRow);
        allChannelsData.append(emptyRow);

        // 通道统计值
        QStringList channelStatisticsTitle;
        channelStatisticsTitle << QString::fromUtf8("========== 通道统计值 (I类/II类误差) ==========");
        for (int i = 1; i < 9; ++i) {
            channelStatisticsTitle << "";
        }
        allChannelsData.append(channelStatisticsTitle);

        if (!decouplingResult.channelStatistics.isEmpty()) {
            for (const QStringList& row : decouplingResult.channelStatistics) {
                QStringList exportRow = row;
                while (exportRow.size() < 9) {
                    exportRow << "";
                }
                allChannelsData.append(exportRow);
            }
        }
    }

    // 导出数据
    bool success = dataExporter->exportToCSV(fileName, allChannelsData);
    if (!success) {
        errorMsg = dataExporter->getLastError();
    }
    return success;
}

// ==================== 公开API实现 ====================

DATASOLVERLIB_API int ProcessDataFile(
    const char* inputCsvPath,
    const char* outputCsvPath,
    char* errorMessage,
    int errorMessageSize)
{
    // 参数检查
    if (!inputCsvPath) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("输入文件路径为空"));
        return DS_ERROR_INVALID_PARAM;
    }

    EnsureGlobalInstancesInitialized();

    QString inputPath = QString::fromUtf8(inputCsvPath);
    QString outputPath;

    // 如果没有指定输出路径，自动生成
    if (!outputCsvPath || strlen(outputCsvPath) == 0) {
        QFileInfo inputInfo(inputPath);
        QString baseName = inputInfo.completeBaseName();
        QString dirPath = inputInfo.absolutePath();
        outputPath = QString("%1/%2_processed.csv").arg(dirPath).arg(baseName);
    } else {
        outputPath = QString::fromUtf8(outputCsvPath);
    }

    // 检查输入文件是否存在
    if (!QFileInfo::exists(inputPath)) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("输入文件不存在: %1").arg(inputPath));
        return DS_ERROR_FILE_NOT_FOUND;
    }

    // 加载数据
    DataLoader dataLoader;
    if (!dataLoader.loadFile(inputPath)) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("加载文件失败: %1").arg(dataLoader.getLastError()));
        return DS_ERROR_INVALID_FORMAT;
    }

    QVector<QStringList> originalData = dataLoader.getData();
    if (originalData.isEmpty()) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("文件中没有数据"));
        return DS_ERROR_INVALID_FORMAT;
    }

    // 导出所有通道数据
    QString errorMsg;
    if (!ExportAllChannelsData(originalData, g_dataProcessor, g_dataExporter, outputPath, errorMsg)) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("导出失败: %1").arg(errorMsg));
        return DS_ERROR_EXPORT;
    }

    // 成功
    if (errorMessage && errorMessageSize > 0) {
        errorMessage[0] = '\0';
    }
    return DS_SUCCESS;
}

DATASOLVERLIB_API int ProcessChannelDataFile(
    const char* inputCsvPath,
    const char* channelName,
    const char* outputCsvPath,
    char* errorMessage,
    int errorMessageSize)
{
    // 参数检查
    if (!inputCsvPath || !channelName) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("输入参数为空"));
        return DS_ERROR_INVALID_PARAM;
    }

    EnsureGlobalInstancesInitialized();

    QString inputPath = QString::fromUtf8(inputCsvPath);
    QString channel = QString::fromUtf8(channelName);
    QString outputPath;

    // 验证通道名称
    QStringList validChannels = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
    if (!validChannels.contains(channel)) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("无效的通道名称: %1").arg(channel));
        return DS_ERROR_INVALID_PARAM;
    }

    // 如果没有指定输出路径，自动生成
    if (!outputCsvPath || strlen(outputCsvPath) == 0) {
        QFileInfo inputInfo(inputPath);
        QString baseName = inputInfo.completeBaseName();
        QString dirPath = inputInfo.absolutePath();
        outputPath = QString("%1/%2_%3_processed.csv").arg(dirPath).arg(baseName).arg(channel);
    } else {
        outputPath = QString::fromUtf8(outputCsvPath);
    }

    // 检查输入文件是否存在
    if (!QFileInfo::exists(inputPath)) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("输入文件不存在: %1").arg(inputPath));
        return DS_ERROR_FILE_NOT_FOUND;
    }

    // 加载数据
    DataLoader dataLoader;
    if (!dataLoader.loadFile(inputPath)) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("加载文件失败: %1").arg(dataLoader.getLastError()));
        return DS_ERROR_INVALID_FORMAT;
    }

    QVector<QStringList> originalData = dataLoader.getData();
    if (originalData.isEmpty()) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("文件中没有数据"));
        return DS_ERROR_INVALID_FORMAT;
    }

    // 处理指定通道
    ProcessResult result = g_dataProcessor->processChannelData(originalData, channel);
    if (!result.lastError.isEmpty()) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("处理数据失败: %1").arg(result.lastError));
        return DS_ERROR_PROCESSING;
    }

    // 合并重复性误差和线性度数据
    QVector<QStringList> dataToExport;
    if (!result.processedData.isEmpty() && !result.linearityData.isEmpty()) {
        dataToExport = result.processedData;

        // 添加空行分隔
        QStringList emptyRow;
        for (int i = 0; i < result.processedData[0].size(); ++i) {
            emptyRow << "";
        }
        dataToExport.append(emptyRow);

        // 添加线性度数据标题
        QStringList linearityTitle;
        linearityTitle << QString::fromUtf8("========== 线性度数据 ==========");
        for (int i = 1; i < result.processedData[0].size(); ++i) {
            linearityTitle << "";
        }
        dataToExport.append(linearityTitle);

        // 添加线性度数据
        dataToExport.append(result.linearityData);
    }

    // 导出数据
    if (!g_dataExporter->exportToCSV(outputPath, dataToExport)) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("导出失败: %1").arg(g_dataExporter->getLastError()));
        return DS_ERROR_EXPORT;
    }

    // 成功
    if (errorMessage && errorMessageSize > 0) {
        errorMessage[0] = '\0';
    }
    return DS_SUCCESS;
}

DATASOLVERLIB_API void SetChannelRanges(
    double fxRange,
    double fyRange,
    double fzRange,
    double mxRange,
    double myRange,
    double mzRange)
{
    EnsureGlobalInstancesInitialized();
    g_dataProcessor->setAllChannelRanges(fxRange, fyRange, fzRange, mxRange, myRange, mzRange);
}

DATASOLVERLIB_API const char* GetLibraryVersion()
{
    return "2.1.0";
}

DATASOLVERLIB_API int ProcessDataFileWithPreprocessing(
    const char* inputCsvPath,
    const char* outputCsvPath,
    int averageGroupSize,
    double zeroThreshold,
    int enableZeroCalibration,
    char* errorMessage,
    int errorMessageSize)
{
    // 初始化全局实例
    EnsureGlobalInstancesInitialized();

    // 验证参数
    if (!inputCsvPath || !outputCsvPath) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("输入或输出路径为空"));
        return DS_ERROR_INVALID_PARAM;
    }

    QString inputPath = QString::fromUtf8(inputCsvPath);
    QString outputPath = QString::fromUtf8(outputCsvPath);

    // ========== 步骤1：加载CSV文件 ==========
    DataLoader loader;
    if (!loader.loadFile(inputPath)) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("加载文件失败: %1").arg(loader.getLastError()));
        return DS_ERROR_FILE_NOT_FOUND;
    }

    QVector<QStringList> data = loader.getData();
    if (data.isEmpty() || data.size() <= 1) {
        SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("文件中没有有效数据"));
        return DS_ERROR_INVALID_FORMAT;
    }

    QString processLog = QString::fromUtf8("原始数据行数: %1\n\n").arg(data.size() - 1);

    // ========== 步骤2：数据平均（如果需要） ==========
    if (averageGroupSize > 0) {
        DataAverager averager;
        data = averager.averageData(data, averageGroupSize);
        if (data.isEmpty() || data.size() <= 1) {
            SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("数据平均失败: %1").arg(averager.getLastError()));
            return DS_ERROR_PROCESSING;
        }
        processLog += QString::fromUtf8("【数据平均完成】\n%1\n\n").arg(averager.getProcessInfo());
    }

    // ========== 步骤3：置零校准（如果需要） ==========
    if (enableZeroCalibration) {
        DataZeroCalibrator calibrator;
        data = calibrator.calibrateZeroPoints(data, zeroThreshold);
        if (data.isEmpty() || data.size() <= 1) {
            SetErrorMessage(errorMessage, errorMessageSize, QString::fromUtf8("置零校准失败: %1").arg(calibrator.getLastError()));
            return DS_ERROR_PROCESSING;
        }
        processLog += QString::fromUtf8("【置零校准完成】\n%1\n\n").arg(calibrator.getProcessInfo());
    }

    processLog += QString::fromUtf8("预处理后数据行数: %1\n\n").arg(data.size() - 1);

    // ========== 步骤4：处理所有6个通道并导出 ==========
    QString exportError;
    bool exportSuccess = ExportAllChannelsData(data, g_dataProcessor, g_dataExporter, outputPath, exportError);

    if (!exportSuccess) {
        SetErrorMessage(errorMessage, errorMessageSize, exportError);
        return DS_ERROR_EXPORT;
    }

    // 成功，返回处理日志
    if (errorMessage && errorMessageSize > 0) {
        QByteArray logUtf8 = processLog.toUtf8();
        int copySize = qMin(errorMessageSize - 1, logUtf8.size());
        memcpy(errorMessage, logUtf8.constData(), copySize);
        errorMessage[copySize] = '\0';
    }

    return DS_SUCCESS;
}
