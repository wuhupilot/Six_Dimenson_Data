#include "ExportManager.h"

#pragma execution_character_set("utf-8")

// 静态常量定义
const QStringList ExportManager::CHANNEL_NAMES = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
const QStringList ExportManager::ROW_LABELS = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
const QStringList ExportManager::COL_LABELS = {"UFx", "UFy", "UFz", "UMx", "UMy", "UMz"};

ExportManager::ExportManager(DataProcessor* processor, DataExporter* exporter)
    : m_dataProcessor(processor)
    , m_dataExporter(exporter)
{
}

ExportManager::~ExportManager()
{
}

bool ExportManager::exportCurrentChannel(
    const QString& fileName,
    const QVector<QStringList>& processedData,
    const QVector<QStringList>& linearityData)
{
    QVector<QStringList> dataToExport = mergeProcessedAndLinearityData(processedData, linearityData);

    if (dataToExport.isEmpty()) {
        m_lastError = QString::fromUtf8("没有数据可导出");
        return false;
    }

    bool success = false;
    if (fileName.endsWith(".csv")) {
        success = m_dataExporter->exportToCSV(fileName, dataToExport);
    } else if (fileName.endsWith(".xlsx")) {
        success = m_dataExporter->exportToExcel(fileName, dataToExport);
    } else {
        QString csvFileName = fileName + ".csv";
        success = m_dataExporter->exportToCSV(csvFileName, dataToExport);
    }

    if (!success) {
        m_lastError = m_dataExporter->getLastError();
    }

    return success;
}

bool ExportManager::exportAllChannels(
    const QString& fileName,
    const QVector<QStringList>& originalData)
{
    QVector<QStringList> allChannelsData;
    QMap<QString, ProcessResult> channelResults;

    // 计算解耦矩阵
    DecouplingResult decouplingResult = m_dataProcessor->calculateDecouplingMatrix(originalData);

    // 处理所有通道
    for (const QString& channel : CHANNEL_NAMES) {
        ProcessResult result = m_dataProcessor->processChannelData(originalData, channel);
        if (result.lastError.isEmpty()) {
            channelResults[channel] = result;
        }
    }

    // 生成导出数据
    appendPerformanceTable(allChannelsData, channelResults, decouplingResult);
    appendChannelDetails(allChannelsData, channelResults);
    appendMatrixAndErrorAnalysis(allChannelsData, decouplingResult);

    // 导出数据
    bool success = false;
    if (fileName.endsWith(".csv")) {
        success = m_dataExporter->exportToCSV(fileName, allChannelsData);
    } else if (fileName.endsWith(".xlsx")) {
        success = m_dataExporter->exportToExcel(fileName, allChannelsData);
    } else {
        QString csvFileName = fileName + ".csv";
        success = m_dataExporter->exportToCSV(csvFileName, allChannelsData);
    }

    if (!success) {
        m_lastError = m_dataExporter->getLastError();
    }

    return success;
}

void ExportManager::appendPerformanceTable(
    QVector<QStringList>& output,
    const QMap<QString, ProcessResult>& channelResults,
    const DecouplingResult& decouplingResult)
{
    QStringList emptyRow = createEmptyRow();

    // 标题
    QStringList performanceTitle;
    performanceTitle << QString::fromUtf8("========== 性能指标评价 ==========");
    for (int i = 1; i < 9; ++i) {
        performanceTitle << "";
    }
    output.append(performanceTitle);
    output.append(emptyRow);

    // 表头
    QStringList performanceHeader;
    performanceHeader << "" << "Fx" << "Fy" << "Fz" << "Mx" << "My" << "Mz";
    for (int i = 7; i < 9; ++i) {
        performanceHeader << "";
    }
    output.append(performanceHeader);

    // 重复性R
    QStringList repeatabilityRow;
    repeatabilityRow << QString::fromUtf8("重复性R（%FS）");
    for (const QString& channel : CHANNEL_NAMES) {
        if (channelResults.contains(channel)) {
            repeatabilityRow << QString::number(channelResults[channel].repeatability, 'f', 8);
        } else {
            repeatabilityRow << "";
        }
    }
    for (int i = 7; i < 9; ++i) {
        repeatabilityRow << "";
    }
    output.append(repeatabilityRow);

    // 迟滞性H
    QStringList hysteresisRow;
    hysteresisRow << QString::fromUtf8("迟滞性H（%FS）");
    for (const QString& channel : CHANNEL_NAMES) {
        if (channelResults.contains(channel)) {
            hysteresisRow << QString::number(channelResults[channel].hysteresis, 'f', 8);
        } else {
            hysteresisRow << "";
        }
    }
    for (int i = 7; i < 9; ++i) {
        hysteresisRow << "";
    }
    output.append(hysteresisRow);

    // 线性度L
    QStringList linearityRow;
    linearityRow << QString::fromUtf8("线性度L（%FS）");
    for (const QString& channel : CHANNEL_NAMES) {
        if (channelResults.contains(channel)) {
            linearityRow << QString::number(channelResults[channel].linearity, 'f', 8);
        } else {
            linearityRow << "";
        }
    }
    for (int i = 7; i < 9; ++i) {
        linearityRow << "";
    }
    output.append(linearityRow);

    // I类误差
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
            output.append(iErrorRow);
        }
    }

    // II类误差
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
            output.append(iiErrorRow);
        }
    }

    output.append(emptyRow);
    output.append(emptyRow);
    output.append(emptyRow);
}

void ExportManager::appendChannelDetails(
    QVector<QStringList>& output,
    const QMap<QString, ProcessResult>& channelResults)
{
    QStringList emptyRow = createEmptyRow();

    for (const QString& channel : CHANNEL_NAMES) {
        if (!channelResults.contains(channel)) continue;

        // 通道标题
        QStringList channelTitle;
        channelTitle << QString::fromUtf8("========== %1 通道 ==========").arg(channel);
        for (int i = 1; i < 9; ++i) {
            channelTitle << "";
        }
        output.append(channelTitle);

        // 重复性误差标题
        QStringList repeatTitle;
        repeatTitle << QString::fromUtf8("重复性误差");
        for (int i = 1; i < 9; ++i) {
            repeatTitle << "";
        }
        output.append(repeatTitle);

        // 重复性误差数据
        output.append(channelResults[channel].processedData);
        output.append(emptyRow);

        // 线性度数据标题
        QStringList linearityTitle;
        linearityTitle << QString::fromUtf8("线性度数据");
        for (int i = 1; i < 9; ++i) {
            linearityTitle << "";
        }
        output.append(linearityTitle);

        // 线性度数据
        output.append(channelResults[channel].linearityData);

        // 通道间分隔
        output.append(emptyRow);
        output.append(emptyRow);
    }
}

void ExportManager::appendMatrixAndErrorAnalysis(
    QVector<QStringList>& output,
    const DecouplingResult& decouplingResult)
{
    QStringList emptyRow = createEmptyRow();

    output.append(emptyRow);
    output.append(emptyRow);

    if (!decouplingResult.success) {
        QStringList errorTitle;
        errorTitle << QString::fromUtf8("========== 解耦矩阵计算失败 ==========");
        for (int i = 1; i < 9; ++i) {
            errorTitle << "";
        }
        output.append(errorTitle);

        QStringList errorRow;
        errorRow << QString::fromUtf8("错误: ") + decouplingResult.errorMessage;
        for (int i = 1; i < 9; ++i) {
            errorRow << "";
        }
        output.append(errorRow);
        return;
    }

    // 第一矩阵
    appendMatrixSection(output, QString::fromUtf8("第一矩阵 (解耦矩阵)"), decouplingResult.matrix);
    output.append(emptyRow);
    output.append(emptyRow);

    // 第二矩阵
    appendMatrixSection(output, QString::fromUtf8("第二矩阵"), decouplingResult.secondMatrix);
    output.append(emptyRow);
    output.append(emptyRow);

    // 第三矩阵
    appendMatrixSection(output, QString::fromUtf8("第三矩阵"), decouplingResult.thirdMatrix);
    output.append(emptyRow);
    output.append(emptyRow);

    // 解耦后数据
    appendDataSection(output,
        QString::fromUtf8("解耦后数据 (Fx', Fy', Fz', Mx', My', Mz')"),
        decouplingResult.decoupledData);
    output.append(emptyRow);
    output.append(emptyRow);

    // 误差数据
    appendDataSection(output,
        QString::fromUtf8("I类误差/II类误差 (Fx-Fx', Fy-Fy', ...)"),
        decouplingResult.errorData);
    output.append(emptyRow);
    output.append(emptyRow);

    // 归一化平方误差
    appendDataSection(output,
        QString::fromUtf8("归一化平方误差 ((误差/量程)²)"),
        decouplingResult.normalizedSquaredError);
    output.append(emptyRow);
    output.append(emptyRow);

    // 通道统计值
    appendDataSection(output,
        QString::fromUtf8("通道统计值 (I类/II类误差)"),
        decouplingResult.channelStatistics);
}

void ExportManager::appendMatrixSection(
    QVector<QStringList>& output,
    const QString& title,
    const double matrix[6][6])
{
    // 标题
    QStringList titleRow;
    titleRow << QString::fromUtf8("========== %1 ==========").arg(title);
    for (int i = 1; i < 9; ++i) {
        titleRow << "";
    }
    output.append(titleRow);

    // 表头
    QStringList headerRow;
    headerRow << "";
    for (const QString& label : COL_LABELS) {
        headerRow << label;
    }
    for (int i = 7; i < 9; ++i) {
        headerRow << "";
    }
    output.append(headerRow);

    // 矩阵数据
    for (int i = 0; i < 6; ++i) {
        QStringList matrixRow;
        matrixRow << ROW_LABELS[i];
        for (int j = 0; j < 6; ++j) {
            matrixRow << QString::number(matrix[i][j], 'f', 8);
        }
        for (int k = 7; k < 9; ++k) {
            matrixRow << "";
        }
        output.append(matrixRow);
    }
}

void ExportManager::appendDataSection(
    QVector<QStringList>& output,
    const QString& title,
    const QVector<QStringList>& data)
{
    // 标题
    QStringList titleRow;
    titleRow << QString::fromUtf8("========== %1 ==========").arg(title);
    for (int i = 1; i < 9; ++i) {
        titleRow << "";
    }
    output.append(titleRow);

    // 数据
    if (!data.isEmpty()) {
        for (const QStringList& row : data) {
            QStringList exportRow = row;
            // 补齐到9列
            while (exportRow.size() < 9) {
                exportRow << "";
            }
            output.append(exportRow);
        }
    }
}

QStringList ExportManager::createEmptyRow(int columnCount)
{
    QStringList emptyRow;
    for (int i = 0; i < columnCount; ++i) {
        emptyRow << "";
    }
    return emptyRow;
}

QVector<QStringList> ExportManager::mergeProcessedAndLinearityData(
    const QVector<QStringList>& processedData,
    const QVector<QStringList>& linearityData)
{
    QVector<QStringList> result;

    if (processedData.isEmpty() && linearityData.isEmpty()) {
        return result;
    }

    if (!processedData.isEmpty()) {
        result = processedData;

        if (!linearityData.isEmpty()) {
            // 添加空行分隔
            QStringList emptyRow;
            for (int i = 0; i < processedData[0].size(); ++i) {
                emptyRow << "";
            }
            result.append(emptyRow);

            // 添加线性度数据标题
            QStringList linearityTitle;
            linearityTitle << QString::fromUtf8("========== 线性度数据 ==========");
            for (int i = 1; i < processedData[0].size(); ++i) {
                linearityTitle << "";
            }
            result.append(linearityTitle);

            // 添加线性度数据
            result.append(linearityData);
        }
    } else {
        result = linearityData;
    }

    return result;
}
