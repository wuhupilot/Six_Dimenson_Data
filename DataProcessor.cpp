#include "DataProcessor.h"
#include <QtMath>
#include <QSet>
#include <algorithm>
#include <numeric>

#pragma execution_character_set("utf-8")
using namespace std;

DataProcessor::DataProcessor()
    : m_maxRange(210.0)
    , m_zeroThreshold(0.1)
    , m_autoDetected(false)
{
    // 初始化默认量程：Fx=1000, Fy=1000, Fz=1500, Mx=My=Mz=50
    resetToDefaultRanges();
}

DataProcessor::~DataProcessor()
{
}

QVector<QString> DataProcessor::getOtherChannels(const QString& currentChannel)
{
    QVector<QString> allChannels = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
    QVector<QString> others;

    for (const QString& channel : allChannels) {
        if (channel != currentChannel) {
            others.append(channel);
        }
    }

    return others;
}

QVector<QString> DataProcessor::getChannelsToCheckZero(const QString& currentChannel)
{
    QVector<QString> allChannels = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
    QVector<QString> channelsToCheck;

    // 定义每个通道允许的伴随通道
    QMap<QString, QVector<QString>> allowedCompanions;
    allowedCompanions["Fx"] = {};                    // Fx测试：只有Fx有数据，其他都应为0
    allowedCompanions["Fy"] = {};                    // Fy测试：只有Fy有数据，其他都应为0
    allowedCompanions["Fz"] = {};                    // Fz测试：只有Fz有数据，其他都应为0
    allowedCompanions["Mx"] = {"Fx", "Fy", "Fz"};   // Mx测试：测力矩时Fx,Fy,Fz都可能有值（力乘以力臂产生力矩）
    allowedCompanions["My"] = {"Fx", "Fy", "Fz"};   // My测试：测力矩时Fx,Fy,Fz都可能有值
    allowedCompanions["Mz"] = {"Fx", "Fy", "Fz"};   // Mz测试：测力矩时Fx,Fy,Fz都可能有值

    // 获取当前通道允许的伴随通道
    QVector<QString> allowed = allowedCompanions.value(currentChannel, QVector<QString>());

    // 遍历所有通道，排除当前通道和允许的伴随通道
    for (const QString& channel : allChannels) {
        if (channel != currentChannel && !allowed.contains(channel)) {
            channelsToCheck.append(channel);
        }
    }

    return channelsToCheck;
}

ProcessResult DataProcessor::processData(const QVector<QStringList>& originalData)
{
    ProcessResult result;

    if (originalData.size() <= 1) {
        result.lastError = QString::fromUtf8("没有数据可处理");
        return result;
    }

    // 如果未手动设置参数，自动检测量程参数
    if (!m_autoDetected) {
        autoDetectRangeParameters(originalData);
    }

    // 默认处理Fx通道（保持向后兼容）
    return processChannelData(originalData, "Fx");
}

ProcessResult DataProcessor::processChannelData(const QVector<QStringList>& originalData, const QString& channelName)
{
    ProcessResult result;
    result.repeatability = 0.0;
    result.hysteresis = 0.0;
    result.linearity = 0.0;

    if (originalData.size() <= 1) {
        result.lastError = QString::fromUtf8("没有数据可处理");
        return result;
    }

    // 每次处理都重新检测当前通道的量程参数
    autoDetectRangeParameters(originalData, channelName);

    // 先计算解耦矩阵，获取解耦后的数据
    DecouplingResult decouplingResult = calculateDecouplingMatrix(originalData);
    QVector<QStringList> decoupledData;
    if (decouplingResult.success) {
        decoupledData = decouplingResult.decoupledData;
    }

    // 处理重复性误差（使用解耦后的数据）
    processRepeatabilityError(originalData, result.processedData, result.calculationDetails, channelName, decoupledData);

    // 从processedData中提取重复性和迟滞性
    if (result.processedData.size() >= 5) {
        // 找到"重复性"和"迟滞性"行
        for (int i = result.processedData.size() - 1; i >= 0; i--) {
            if (result.processedData[i].size() >= 2) {
                if (result.processedData[i][0] == QString::fromUtf8("重复性")) {
                    result.repeatability = result.processedData[i][1].toDouble() * 100.0;
                }
                if (result.processedData[i][0] == QString::fromUtf8("迟滞性")) {
                    result.hysteresis = result.processedData[i][1].toDouble() * 100.0;
                }
            }
        }
    }

    // 处理线性度数据（线性度仍使用UF数据）
    processLinearityData(originalData, result.linearityData, result.calculationDetails, channelName, decoupledData);

    // 从linearityData中提取MAX(ABS(差值))/量程
    // 这个值在倒数第3行（a和b之前）
    if (result.linearityData.size() >= 3) {
        for (int i = result.linearityData.size() - 1; i >= 0; i--) {
            if (result.linearityData[i].size() >= 4) {
                QString firstCol = result.linearityData[i][0];
                if (firstCol.contains("MAX(ABS") && firstCol.contains(")")) {
                    result.linearity = result.linearityData[i][3].toDouble() * 100.0;
                    break;
                }
            }
        }
    }

    return result;
}

int DataProcessor::findColumnIndex(const QStringList& headers, const QString& columnName)
{
    for (int i = 0; i < headers.size(); ++i) {
        if (headers[i].compare(columnName, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

double DataProcessor::calculateStandardDeviation(const QVector<double>& values, double mean)
{
    if (values.size() <= 1) return 0.0;

    double sumSquares = 0.0;
    for (int i = 0; i < values.size(); ++i) {
        double diff = values[i] - mean;
        sumSquares += diff * diff;
    }

    return sqrt(sumSquares / (values.size() - 1));
}

QString DataProcessor::generateValuesList(const QVector<double>& values)
{
    if (values.isEmpty()) return QString::fromUtf8("无");

    QStringList valueStrList;
    for (int i = 0; i < values.size(); ++i) {
        valueStrList.append(QString::number(values[i], 'f', 8));
    }
    return "[" + valueStrList.join(", ") + "]";
}

QVector<ChannelDataPoint> DataProcessor::collectPureChannelData(
    const QVector<QStringList>& originalData,
    const QString& channelName,
    const QVector<QStringList>& decoupledData)
{
    QVector<ChannelDataPoint> pureChannelData;

    if (originalData.isEmpty()) return pureChannelData;

    QStringList headers = originalData[0];

    // 获取设定值的列索引（从原始数据）
    int setColumnIndex = findColumnIndex(headers, channelName);
    if (setColumnIndex == -1) {
        return pureChannelData;
    }

    // 获取测量值的列索引（从解耦后数据或原始UF数据）
    int measureColumnIndex = -1;
    bool useDecoupledData = !decoupledData.isEmpty() && decoupledData.size() > 1;

    if (useDecoupledData) {
        // 使用解耦后数据：Fx' Fy' Fz' Mx' My' Mz'
        QStringList decoupledHeaders = decoupledData[0];
        QString decoupledColumnName = channelName + "'";  // 例如: Fx -> Fx'
        measureColumnIndex = findColumnIndex(decoupledHeaders, decoupledColumnName);

        if (measureColumnIndex == -1) {
            // 找不到解耦后的列，退回到使用UF数据
            useDecoupledData = false;
        }
    }

    if (!useDecoupledData) {
        // 使用原始UF数据
        QString measureColumnName = "U" + channelName;  // 例如: Fx -> UFx
        measureColumnIndex = findColumnIndex(headers, measureColumnName);

        if (measureColumnIndex == -1) {
            return pureChannelData;
        }
    }

    // 获取需要检查为0的通道（排除当前通道和允许的伴随通道）
    QVector<QString> channelsToCheckZero = getChannelsToCheckZero(channelName);
    QVector<int> checkColumnIndices;
    for (const QString& channel : channelsToCheckZero) {
        int idx = findColumnIndex(headers, channel);
        if (idx != -1) {
            checkColumnIndices.append(idx);
        }
    }

    for (int i = 1; i < originalData.size(); ++i) {
        const QStringList& row = originalData[i];
        if (setColumnIndex >= row.size()) continue;

        // 获取设定值（从原始数据）
        bool setOk;
        double setValue = row[setColumnIndex].toDouble(&setOk);
        if (!setOk) continue;

        // 获取测量值（从解耦后数据或原始数据）
        bool measureOk;
        double measureValue = 0.0;

        if (useDecoupledData) {
            // 从解耦后数据获取（注意：解耦数据有表头，所以行号是i，不是i-1）
            if (i < decoupledData.size() && measureColumnIndex < decoupledData[i].size()) {
                measureValue = decoupledData[i][measureColumnIndex].toDouble(&measureOk);
            } else {
                continue;
            }
        } else {
            // 从原始数据获取UF值
            if (measureColumnIndex >= row.size()) continue;
            measureValue = row[measureColumnIndex].toDouble(&measureOk);
        }

        if (!measureOk) continue;

        // 检查需要为0的通道是否都接近0（允许的伴随通道不在检查范围内）
        bool isPureChannelTest = true;
        for (int colIndex : checkColumnIndices) {
            if (colIndex < row.size()) {
                bool ok;
                double value = row[colIndex].toDouble(&ok);
                if (ok && qAbs(value) > m_zeroThreshold) {
                    isPureChannelTest = false;
                    break;
                }
            }
        }

        if (isPureChannelTest) {
            // 判断边沿类型
            QString edgeType = QString::fromUtf8("平稳");
            if (i > 1 && i < originalData.size() - 1) {
                bool prevOk = false, nextOk = false;
                double prevValue = -999, nextValue = -999;

                if (setColumnIndex < originalData[i-1].size()) {
                    prevValue = originalData[i-1][setColumnIndex].toDouble(&prevOk);
                }
                if (setColumnIndex < originalData[i+1].size()) {
                    nextValue = originalData[i+1][setColumnIndex].toDouble(&nextOk);
                }

                if (prevOk && nextOk) {
                    if (prevValue < setValue && setValue < nextValue) {
                        edgeType = QString::fromUtf8("上升");
                    } else if (prevValue > setValue && setValue > nextValue) {
                        edgeType = QString::fromUtf8("下降");
                    } else if (prevValue < setValue && setValue > nextValue) {
                        edgeType = QString::fromUtf8("上升+下降");
                    } else if (prevValue > setValue && setValue < nextValue) {
                        edgeType = QString::fromUtf8("下降+上升");
                    }
                }
            }

            pureChannelData.append({setValue, measureValue, i, edgeType});
        }
    }

    return pureChannelData;
}

// 保留旧方法以兼容
QVector<FxDataPoint> DataProcessor::collectPureFxData(const QVector<QStringList>& originalData)
{
    QVector<QStringList> emptyDecoupledData;
    return collectPureChannelData(originalData, "Fx", emptyDecoupledData);
}

void DataProcessor::processRepeatabilityError(
    const QVector<QStringList>& originalData,
    QVector<QStringList>& processedData,
    QString& calculationDetails,
    const QString& channelName,
    const QVector<QStringList>& decoupledData)
{
    processedData.clear();

    // 收集纯通道数据（使用解耦后的数据）
    QVector<ChannelDataPoint> pureChannelData = collectPureChannelData(originalData, channelName, decoupledData);

    if (pureChannelData.isEmpty()) {
        return;
    }

    // 分别处理正值循环和负值循环
    QMap<double, QVector<ChannelDataPoint>> positiveGroups;
    QMap<double, QVector<ChannelDataPoint>> negativeGroups;

    for (int i = 0; i < pureChannelData.size(); ++i) {
        const ChannelDataPoint& point = pureChannelData[i];

        if (qAbs(point.setValue) < m_zeroThreshold) {
            // 0点根据邻近值判断分组
            bool foundPositive = false, foundNegative = false;

            if (i > 0 && qAbs(pureChannelData[i-1].setValue) > m_zeroThreshold) {
                if (pureChannelData[i-1].setValue > 0) foundPositive = true;
                else foundNegative = true;
            }

            if (!foundPositive && !foundNegative && i < pureChannelData.size() - 1 && qAbs(pureChannelData[i+1].setValue) > m_zeroThreshold) {
                if (pureChannelData[i+1].setValue > 0) foundPositive = true;
                else foundNegative = true;
            }

            if (foundPositive) {
                positiveGroups[0].append(point);
            } else if (foundNegative) {
                negativeGroups[0].append(point);
            }
        } else if (point.setValue > 0) {
            positiveGroups[point.setValue].append(point);
        } else {
            negativeGroups[point.setValue].append(point);
        }
    }

    // 用于收集标准差的容器
    QVector<double> positiveStdDevs;
    QVector<double> negativeStdDevs;
    QVector<double> allAvgDifferences;

    // 创建详细数据表头
    QStringList detailHeaders;
    detailHeaders << QString::fromUtf8("循环类型")
                  << QString::fromUtf8("%1设定值").arg(channelName)
                  << QString::fromUtf8("上升沿平均值")
                  << QString::fromUtf8("上升沿标准差")
                  << QString::fromUtf8("上升沿数值列表")
                  << QString::fromUtf8("下降沿平均值")
                  << QString::fromUtf8("下降沿标准差")
                  << QString::fromUtf8("下降沿数值列表")
                  << QString::fromUtf8("|正行程平均值-反行程平均值|");
    processedData.append(detailHeaders);

    // 处理正值循环
    for (QMap<double, QVector<ChannelDataPoint>>::iterator it = positiveGroups.begin(); it != positiveGroups.end(); ++it) {
        double setValue = it.key();
        QVector<ChannelDataPoint> points = it.value();

        QVector<double> risingValues, fallingValues;

        if (qAbs(setValue) < m_zeroThreshold) {
            // 0点特殊处理
            if (points.size() >= 4) {
                for (int i = 0; i < 3; ++i) {
                    risingValues.append(points[i].measuredValue);
                }
                for (int i = 1; i < 4; ++i) {
                    fallingValues.append(points[i].measuredValue);
                }
            } else {
                for (int i = 0; i < points.size(); ++i) {
                    if (i < 3) {
                        risingValues.append(points[i].measuredValue);
                    } else {
                        fallingValues.append(points[i].measuredValue);
                    }
                }
            }
        } else {
            // 非0点按边沿类型分组
            for (int i = 0; i < points.size(); ++i) {
                const ChannelDataPoint& point = points[i];
                if (point.edgeType.contains("上升")) {
                    risingValues.append(point.measuredValue);
                }
                if (point.edgeType.contains("下降")) {
                    fallingValues.append(point.measuredValue);
                }
            }
        }

        // 计算平均值和标准差
        double risingAvg = 0.0, fallingAvg = 0.0;
        if (!risingValues.isEmpty()) {
            risingAvg = accumulate(risingValues.begin(), risingValues.end(), 0.0) / risingValues.size();
        }
        if (!fallingValues.isEmpty()) {
            fallingAvg = accumulate(fallingValues.begin(), fallingValues.end(), 0.0) / fallingValues.size();
        }

        double risingStdDev = calculateStandardDeviation(risingValues, risingAvg);
        double fallingStdDev = calculateStandardDeviation(fallingValues, fallingAvg);

        // 收集标准差
        if (!risingValues.isEmpty()) positiveStdDevs.append(risingStdDev);
        if (!fallingValues.isEmpty()) positiveStdDevs.append(fallingStdDev);

        // 计算平均值差值
        QString avgDifference = QString::fromUtf8("无");
        if (!risingValues.isEmpty() && !fallingValues.isEmpty()) {
            double difference = qAbs(risingAvg - fallingAvg);
            avgDifference = QString::number(difference, 'f', 8);
            allAvgDifferences.append(difference);
        } else if (!risingValues.isEmpty()) {
            double difference = qAbs(risingAvg);
            avgDifference = QString::number(difference, 'f', 8);
            allAvgDifferences.append(difference);
        } else if (!fallingValues.isEmpty()) {
            double difference = qAbs(fallingAvg);
            avgDifference = QString::number(difference, 'f', 8);
            allAvgDifferences.append(difference);
        }

        QStringList resultRow;
        resultRow << QString::fromUtf8("正值循环")
                  << QString::number(setValue, 'f', 1)
                  << (risingValues.isEmpty() ? "无" : QString::number(risingAvg, 'f', 8))
                  << (risingValues.isEmpty() ? "无" : QString::number(risingStdDev, 'f', 8))
                  << generateValuesList(risingValues)
                  << (fallingValues.isEmpty() ? "无" : QString::number(fallingAvg, 'f', 8))
                  << (fallingValues.isEmpty() ? "无" : QString::number(fallingStdDev, 'f', 8))
                  << generateValuesList(fallingValues)
                  << avgDifference;

        processedData.append(resultRow);
    }

    // 处理负值循环（类似逻辑）
    for (QMap<double, QVector<ChannelDataPoint>>::iterator it = negativeGroups.begin(); it != negativeGroups.end(); ++it) {
        double setValue = it.key();
        QVector<ChannelDataPoint> points = it.value();

        QVector<double> risingValues, fallingValues;

        if (qAbs(setValue) < m_zeroThreshold) {
            if (points.size() >= 4) {
                for (int i = 0; i < 3; ++i) {
                    risingValues.append(points[i].measuredValue);
                }
                for (int i = 1; i < 4; ++i) {
                    fallingValues.append(points[i].measuredValue);
                }
            } else {
                for (int i = 0; i < points.size(); ++i) {
                    if (i < 3) {
                        risingValues.append(points[i].measuredValue);
                    } else {
                        fallingValues.append(points[i].measuredValue);
                    }
                }
            }
        } else {
            for (const ChannelDataPoint& point : points) {
                if (point.edgeType.contains("上升")) {
                    risingValues.append(point.measuredValue);
                }
                if (point.edgeType.contains("下降")) {
                    fallingValues.append(point.measuredValue);
                }
            }
        }

        double risingAvg = 0.0, fallingAvg = 0.0;
        if (!risingValues.isEmpty()) {
            risingAvg = accumulate(risingValues.begin(), risingValues.end(), 0.0) / risingValues.size();
        }
        if (!fallingValues.isEmpty()) {
            fallingAvg = accumulate(fallingValues.begin(), fallingValues.end(), 0.0) / fallingValues.size();
        }

        double risingStdDev = calculateStandardDeviation(risingValues, risingAvg);
        double fallingStdDev = calculateStandardDeviation(fallingValues, fallingAvg);

        if (!risingValues.isEmpty()) negativeStdDevs.append(risingStdDev);
        if (!fallingValues.isEmpty()) negativeStdDevs.append(fallingStdDev);

        QString avgDifference = QString::fromUtf8("无");
        if (!risingValues.isEmpty() && !fallingValues.isEmpty()) {
            double difference = qAbs(risingAvg - fallingAvg);
            avgDifference = QString::number(difference, 'f', 8);
            allAvgDifferences.append(difference);
        } else if (!risingValues.isEmpty()) {
            double difference = qAbs(risingAvg);
            avgDifference = QString::number(difference, 'f', 8);
            allAvgDifferences.append(difference);
        } else if (!fallingValues.isEmpty()) {
            double difference = qAbs(fallingAvg);
            avgDifference = QString::number(difference, 'f', 8);
            allAvgDifferences.append(difference);
        }

        QStringList resultRow;
        resultRow << QString::fromUtf8("负值循环")
                  << QString::number(setValue, 'f', 1)
                  << (risingValues.isEmpty() ? "无" : QString::number(risingAvg, 'f', 8))
                  << (risingValues.isEmpty() ? "无" : QString::number(risingStdDev, 'f', 8))
                  << generateValuesList(risingValues)
                  << (fallingValues.isEmpty() ? "无" : QString::number(fallingAvg, 'f', 8))
                  << (fallingValues.isEmpty() ? "无" : QString::number(fallingStdDev, 'f', 8))
                  << generateValuesList(fallingValues)
                  << avgDifference;

        processedData.append(resultRow);
    }

    // 计算标准偏差
    double positiveStdDeviation = 0.0;
    if (!positiveStdDevs.isEmpty()) {
        double sumSquares = 0.0;
        for (int i = 0; i < positiveStdDevs.size(); ++i) {
            sumSquares += positiveStdDevs[i] * positiveStdDevs[i];
        }
        positiveStdDeviation = sqrt(sumSquares / positiveStdDevs.size());
    }

    double negativeStdDeviation = 0.0;
    if (!negativeStdDevs.isEmpty()) {
        double sumSquares = 0.0;
        for (int i = 0; i < negativeStdDevs.size(); ++i) {
            sumSquares += negativeStdDevs[i] * negativeStdDevs[i];
        }
        negativeStdDeviation = sqrt(sumSquares / negativeStdDevs.size());
    }

    // 计算记录量（使用当前通道的量程）
    double currentChannelRange = getChannelRange(channelName);
    double maxStdDeviation = qMax(positiveStdDeviation, negativeStdDeviation);
    double recordValue = 2.0 * maxStdDeviation / currentChannelRange;
    double lastValue = 2.0 * recordValue / currentChannelRange;

    double maxAvgDifference = 0.0;
    double thirdRecordValue = 0.0;
    if (!allAvgDifferences.isEmpty()) {
        maxAvgDifference = *max_element(allAvgDifferences.begin(), allAvgDifferences.end());
        thirdRecordValue = maxAvgDifference / currentChannelRange;
    }

    // 添加标准偏差和记录量行
    QStringList positiveStdRow;
    positiveStdRow << QString::fromUtf8("正循环标准偏差") << QString::number(positiveStdDeviation, 'f', 8) << "" << "" << "" << "" << "" << "" << "";
    processedData.append(positiveStdRow);

    QStringList negativeStdRow;
    negativeStdRow << QString::fromUtf8("负循环标准偏差") << QString::number(negativeStdDeviation, 'f', 8) << "" << "" << "" << "" << "" << "" << "";
    processedData.append(negativeStdRow);

    QStringList recordRow;
    recordRow << QString::fromUtf8("重复性") << QString::number(recordValue, 'f', 8) << "" << "" << "" << "" << "" << "" << "";
    processedData.append(recordRow);

    QStringList lastRecordRow;
    lastRecordRow << QString::fromUtf8("Last记录量") << QString::number(lastValue, 'f', 8) << "" << "" << "" << "" << "" << "" << "";
    processedData.append(lastRecordRow);

    QStringList thirdRecordRow;
    thirdRecordRow << QString::fromUtf8("迟滞性") << QString::number(thirdRecordValue, 'f', 8) << "" << "" << "" << "" << "" << "" << "";
    processedData.append(thirdRecordRow);

    // 生成详细计算过程
    QString positiveStdListStr = "";
    if (!positiveStdDevs.isEmpty()) {
        QStringList positiveStdList;
        for (double val : positiveStdDevs) {
            positiveStdList.append(QString::number(val, 'f', 8));
        }
        positiveStdListStr = "[" + positiveStdList.join(", ") + "]";
    }

    QString negativeStdListStr = "";
    if (!negativeStdDevs.isEmpty()) {
        QStringList negativeStdList;
        for (double val : negativeStdDevs) {
            negativeStdList.append(QString::number(val, 'f', 8));
        }
        negativeStdListStr = "[" + negativeStdList.join(", ") + "]";
    }

    // 计算详细的计算过程
    QString positiveCalcProcess = "";
    if (!positiveStdDevs.isEmpty()) {
        double sumSquares = 0.0;
        for (int i = 0; i < positiveStdDevs.size(); ++i) {
            sumSquares += positiveStdDevs[i] * positiveStdDevs[i];
        }
        positiveCalcProcess = QString("SQRT(%1/%2) = %3")
                    .arg(sumSquares, 0, 'f', 8)
                    .arg(positiveStdDevs.size())
                    .arg(positiveStdDeviation, 0, 'f', 8);
    }

    QString negativeCalcProcess = "";
    if (!negativeStdDevs.isEmpty()) {
        double sumSquares = 0.0;
        for (int i = 0; i < negativeStdDevs.size(); ++i) {
            sumSquares += negativeStdDevs[i] * negativeStdDevs[i];
        }
        negativeCalcProcess = QString("SQRT(%1/%2) = %3")
                    .arg(sumSquares, 0, 'f', 8)
                    .arg(negativeStdDevs.size())
                    .arg(negativeStdDeviation, 0, 'f', 8);
    }

    QString recordCalcProcess = QString("2 * MAX(%1, %2) / %5 = 2 * %3 / %5 = %4")
                .arg(positiveStdDeviation, 0, 'f', 8)
                .arg(negativeStdDeviation, 0, 'f', 8)
                .arg(maxStdDeviation, 0, 'f', 8)
                .arg(recordValue, 0, 'f', 8)
                .arg(currentChannelRange, 0, 'f', 1);

    QString lastCalcProcess = QString("2 * %1 / %3 = %2")
                .arg(recordValue, 0, 'f', 8)
                .arg(lastValue, 0, 'f', 8)
                .arg(currentChannelRange, 0, 'f', 1);

    QString thirdRecordCalcProcess = "";
    if (!allAvgDifferences.isEmpty()) {
        QStringList diffList;
        for (int i = 0; i < allAvgDifferences.size(); ++i) {
            diffList.append(QString::number(allAvgDifferences[i], 'f', 8));
        }
        QString diffListStr = "[" + diffList.join(", ") + "]";

        thirdRecordCalcProcess = QString::fromUtf8("绝对值差值列表: %1\nMAX = %2\n%2 / %4 = %3")
                    .arg(diffListStr)
                    .arg(maxAvgDifference, 0, 'f', 8)
                    .arg(thirdRecordValue, 0, 'f', 8)
                    .arg(currentChannelRange, 0, 'f', 1);
    } else {
        thirdRecordCalcProcess = QString::fromUtf8("无可用的差值数据");
    }

    calculationDetails += QString::fromUtf8(
        "===== 标准偏差计算过程 =====\n\n"
        "正值循环标准差: %1 (共%2个)\n"
        "正值循环标准偏差: %3\n\n"
        "负值循环标准差: %4 (共%5个)\n"
        "负值循环标准偏差: %6\n\n"
        "===== 记录量计算 =====\n"
        "记录量计算过程: %7\n"
        "最终记录量: %8\n\n"
        "===== Last记录量计算 =====\n"
        "Last记录量计算过程: %9\n"
        "最终Last记录量: %10\n\n"
        "===== 第三记录量计算 =====\n"
        "%11\n"
        "最终第三记录量: %12\n\n"
        "===== 量程参数 =====\n"
        "%15通道量程: %13\n"
        "零点判断阈值: %14\n\n"
        "公式说明:\n"
        "- 标准偏差 = SQRT(SUMSQ(标准差值)/COUNT(标准差个数))\n"
        "- 重复性R = 2 * MAX(正量程标准偏差, 反量程标准偏差) / %15通道量程(%13)\n"
        "- Last记录量 = 2 * 重复性R / %15通道量程(%13)\n"
        "- 迟滞性H = MAX(|所有正行程平均值-反行程平均值|) / %15通道量程(%13)"
    ).arg(positiveStdListStr)
     .arg(positiveStdDevs.size())
     .arg(positiveCalcProcess)
     .arg(negativeStdListStr)
     .arg(negativeStdDevs.size())
     .arg(negativeCalcProcess)
     .arg(recordCalcProcess)
     .arg(recordValue, 0, 'f', 8)
     .arg(lastCalcProcess)
     .arg(lastValue, 0, 'f', 8)
     .arg(thirdRecordCalcProcess)
     .arg(thirdRecordValue, 0, 'f', 8)
     .arg(currentChannelRange, 0, 'f', 1)
     .arg(m_zeroThreshold, 0, 'f', 8)
     .arg(channelName);
}

void DataProcessor::processLinearityData(
    const QVector<QStringList>& originalData,
    QVector<QStringList>& linearityData,
    QString& calculationDetails,
    const QString& channelName,
    const QVector<QStringList>& decoupledData)
{
    // 线性度仍然使用UF数据，不使用解耦后数据
    // decoupledData参数保留但不使用，以保持接口一致
    Q_UNUSED(decoupledData);

    linearityData.clear();

    if (originalData.size() <= 1) {
        return;
    }

    // 创建线性度表头
    QString measureColumnName = "U" + channelName;
    QStringList linearityHeaders;
    linearityHeaders << QString::fromUtf8("平均电压值")
                     << QString::fromUtf8("标准压力值")
                     << QString::fromUtf8("Y=ax+b")
                     << QString::fromUtf8("标准压力值-最小二乘法")
                     << QString::fromUtf8("数值列表");
    linearityData.append(linearityHeaders);

    // 收集纯通道数据（线性度分析不使用解耦数据）
    QVector<QStringList> emptyDecoupledData;
    QVector<ChannelDataPoint> pureChannelData = collectPureChannelData(originalData, channelName, emptyDecoupledData);

    if (pureChannelData.isEmpty()) {
        return;
    }

    // 分别处理正值循环和负值循环
    QMap<double, QVector<ChannelDataPoint>> positiveGroups;
    QMap<double, QVector<ChannelDataPoint>> negativeGroups;

    for (int i = 0; i < pureChannelData.size(); ++i) {
        const ChannelDataPoint& point = pureChannelData[i];

        if (qAbs(point.setValue) < m_zeroThreshold) {
            bool foundPositive = false, foundNegative = false;

            if (i > 0 && qAbs(pureChannelData[i-1].setValue) > m_zeroThreshold) {
                if (pureChannelData[i-1].setValue > 0) foundPositive = true;
                else foundNegative = true;
            }

            if (!foundPositive && !foundNegative && i < pureChannelData.size() - 1 && qAbs(pureChannelData[i+1].setValue) > m_zeroThreshold) {
                if (pureChannelData[i+1].setValue > 0) foundPositive = true;
                else foundNegative = true;
            }

            if (foundPositive) {
                positiveGroups[0].append(point);
            } else if (foundNegative) {
                negativeGroups[0].append(point);
            }
        } else if (point.setValue > 0) {
            positiveGroups[point.setValue].append(point);
        } else {
            negativeGroups[point.setValue].append(point);
        }
    }

    // 计算SUMPRODUCT和其他统计量
    double sumProduct = 0.0;
    QVector<double> allSetValuesForRegression;    // 原 allFxForRegression
    QVector<double> allMeasuredValuesForRegression; // 原 allUfxForRegression

    // 从正循环收集数据
    for (QMap<double, QVector<ChannelDataPoint>>::iterator it = positiveGroups.begin(); it != positiveGroups.end(); ++it) {
        double setValue = it.key();
        QVector<ChannelDataPoint> points = it.value();
        for (const ChannelDataPoint& point : points) {
            allSetValuesForRegression.append(setValue);
            allMeasuredValuesForRegression.append(point.measuredValue);
            sumProduct += point.measuredValue * setValue;
        }
    }

    // 从负循环收集数据
    for (QMap<double, QVector<ChannelDataPoint>>::iterator it = negativeGroups.begin(); it != negativeGroups.end(); ++it) {
        double setValue = it.key();
        QVector<ChannelDataPoint> points = it.value();
        for (const ChannelDataPoint& point : points) {
            allSetValuesForRegression.append(setValue);
            allMeasuredValuesForRegression.append(point.measuredValue);
            sumProduct += point.measuredValue * setValue;
        }
    }

    // 计算最小二乘法参数
    double a = 0.0, b = 0.0;
    if (allSetValuesForRegression.size() >= 2) {
        int count_values = allSetValuesForRegression.size();
        double sum_setValues = accumulate(allSetValuesForRegression.begin(), allSetValuesForRegression.end(), 0.0);
        double sum_measured = accumulate(allMeasuredValuesForRegression.begin(), allMeasuredValuesForRegression.end(), 0.0);
        double sumsq_measured = 0.0;

        for (int i = 0; i < allMeasuredValuesForRegression.size(); ++i) {
            sumsq_measured += allMeasuredValuesForRegression[i] * allMeasuredValuesForRegression[i];
        }

        // 计算标准压力值总和（所有数据点的设定值总和，不去重）
        double totalStandardPressureSum = 0.0;
        for (double setValue : allSetValuesForRegression) {
            totalStandardPressureSum += setValue;
        }

        double numerator = count_values * sumProduct - sum_measured * totalStandardPressureSum;
        double denominator = count_values * sumsq_measured - sum_measured * sum_measured;

        if (qAbs(denominator) > 0.0001) {
            a = numerator / denominator;

            double b_numerator = sumsq_measured * totalStandardPressureSum - sum_measured * sumProduct;
            double b_denominator = count_values * sumsq_measured - sum_measured * sum_measured;

            if (qAbs(b_denominator) > 0.0001) {
                b = b_numerator / b_denominator;
            }
        }
    }

    // 生成最终结果
    QMap<QString, QVector<double>> allValues;
    QMap<QString, double> finalAverages;
    QVector<double> allDifferences;
    double totalStandardPressure = 0.0;

    // 处理正循环数据
    for (QMap<double, QVector<ChannelDataPoint>>::iterator it = positiveGroups.begin(); it != positiveGroups.end(); ++it) {
        double setValue = it.key();
        QVector<ChannelDataPoint> points = it.value();

        QVector<double> rawValues;
        for (const ChannelDataPoint& point : points) {
            rawValues.append(point.measuredValue);
        }

        double average = accumulate(rawValues.begin(), rawValues.end(), 0.0) / rawValues.size();

        QString key = (qAbs(setValue) < m_zeroThreshold) ? QString::fromUtf8("0(正)") : QString::number(setValue, 'f', 1);
        allValues[key] = rawValues;
        finalAverages[key] = average;
    }

    // 处理负循环数据
    for (QMap<double, QVector<ChannelDataPoint>>::iterator it = negativeGroups.begin(); it != negativeGroups.end(); ++it) {
        double setValue = it.key();
        QVector<ChannelDataPoint> points = it.value();

        QVector<double> rawValues;
        for (const ChannelDataPoint& point : points) {
            rawValues.append(point.measuredValue);
        }

        double average = accumulate(rawValues.begin(), rawValues.end(), 0.0) / rawValues.size();

        QString key = (qAbs(setValue) < m_zeroThreshold) ? QString::fromUtf8("0(负)") : QString::number(setValue, 'f', 1);
        allValues[key] = rawValues;
        finalAverages[key] = average;
    }

    // 生成显示结果
    for (QMap<QString, double>::iterator it = finalAverages.begin(); it != finalAverages.end(); ++it) {
        QString keyStr = it.key();
        double avgVoltage = it.value();

        double standardPressure = 0.0;
        if (keyStr == "0(正)" || keyStr == "0(负)") {
            standardPressure = 0.0;
        } else {
            standardPressure = keyStr.toDouble();
        }

        if (!keyStr.contains("(")) {
            totalStandardPressure += standardPressure;
        } else if (keyStr == "0(正)") {
            totalStandardPressure += standardPressure;
        }

        double theoreticalValue = a * avgVoltage + b;
        double difference = standardPressure - theoreticalValue;
        allDifferences.append(difference);

        QString valuesList = generateValuesList(allValues[keyStr]);

        QStringList resultRow;
        resultRow << QString::number(avgVoltage, 'f', 8)
                  << keyStr
                  << QString::number(theoreticalValue, 'f', 8)
                  << QString::number(difference, 'f', 8)
                  << valuesList;

        linearityData.append(resultRow);
    }

    // 计算MAX(ABS(差值))/通道量程
    double currentChannelRange = getChannelRange(channelName);
    double maxAbsDifference = 0.0;
    if (!allDifferences.isEmpty()) {
        for (int i = 0; i < allDifferences.size(); ++i) {
            double absDiff = qAbs(allDifferences[i]);
            if (absDiff > maxAbsDifference) {
                maxAbsDifference = absDiff;
            }
        }
    }
    double maxDifferenceResult = maxAbsDifference / currentChannelRange;

    // 计算真正的标准压力值总和（不去重，所有数据点的设定值总和）
    double totalStandardPressureSumForTable = 0.0;
    for (double setValue : allSetValuesForRegression) {
        totalStandardPressureSumForTable += setValue;
    }

    // 添加总计行、SUMPRODUCT行和MAX(ABS(差值))/210行
    QStringList totalRow1;
    totalRow1 << QString::fromUtf8("总计") << QString::number(totalStandardPressureSumForTable, 'f', 8) << "" << "" << "";
    linearityData.append(totalRow1);

    QStringList totalRow2;
    totalRow2 << "SUMPRODUCT" << QString::number(sumProduct, 'f', 8) << "" << "" << "";
    linearityData.append(totalRow2);

    QStringList totalRow3;
    totalRow3 << QString::fromUtf8("MAX(ABS(差值))/%1").arg(currentChannelRange, 0, 'f', 0) << QString::number(maxAbsDifference, 'f', 8) << "" << QString::number(maxDifferenceResult, 'f', 8) << "";
    linearityData.append(totalRow3);

    // 添加 a 和 b 值显示
    QStringList aRow;
    aRow << "a" << QString::number(a, 'f', 8) << "" << "" << "";
    linearityData.append(aRow);

    QStringList bRow;
    bRow << "b" << QString::number(b, 'f', 8) << "" << "" << "";
    linearityData.append(bRow);

    // 生成详细的回归计算过程
    QString sumProductDetailsStr = "";
    if (allSetValuesForRegression.size() >= 2) {
        int count_values = allSetValuesForRegression.size();
        double sum_setValues = accumulate(allSetValuesForRegression.begin(), allSetValuesForRegression.end(), 0.0);
        double sum_measured = accumulate(allMeasuredValuesForRegression.begin(), allMeasuredValuesForRegression.end(), 0.0);
        double sumsq_measured = 0.0;

        for (int i = 0; i < allMeasuredValuesForRegression.size(); ++i) {
            sumsq_measured += allMeasuredValuesForRegression[i] * allMeasuredValuesForRegression[i];
        }

        // 计算标准压力值总和（所有数据点的设定值总和，不去重）
        double totalStandardPressureSum = 0.0;
        for (double setValue : allSetValuesForRegression) {
            totalStandardPressureSum += setValue;
        }

        double numerator = count_values * sumProduct - sum_measured * totalStandardPressureSum;
        double denominator = count_values * sumsq_measured - sum_measured * sum_measured;
        double b_numerator = sumsq_measured * totalStandardPressureSum - sum_measured * sumProduct;
        double b_denominator = count_values * sumsq_measured - sum_measured * sum_measured;

        // 生成SUMPRODUCT计算详情
        QStringList sumProductDetailsList;
        for (int i = 0; i < allSetValuesForRegression.size(); ++i) {
            sumProductDetailsList.append(QString("(%1 × %2)")
                                        .arg(QString::number(allMeasuredValuesForRegression[i], 'f', 8))
                                        .arg(QString::number(allSetValuesForRegression[i], 'f', 1)));
        }
        sumProductDetailsStr = sumProductDetailsList.join(" + ");

        QString regressionDetails = QString::fromUtf8(
            "\n===== 最小二乘法计算详情 =====\n\n"
            "COUNT(所有%1) = %2\n"
            "SUM(所有%1) = %3\n"
            "SUMSQ(所有%1) = %4\n"
            "SUMPRODUCT(%1,%5) = %6\n"
            "总计行数据(标准压力值总和) = %7\n\n"
            "计算a的公式:\n"
            "a = (COUNT(%1)*SUMPRODUCT(%1,%5) - SUM(%1)*总计行数据) / (COUNT(%1)*SUMSQ(%1) - SUM(%1)*SUM(%1))\n"
            "a = (%2 * %6 - %3 * %7) / (%2 * %4 - %3 * %3)\n"
            "a = (%8 - %9) / (%10 - %11)\n"
            "a = %12 / %13\n"
            "a = %14\n\n"
            "计算b的公式:\n"
            "b = (SUMSQ(%1)*总计行数据 - SUM(%1)*SUMPRODUCT) / (COUNT(%1)*SUMSQ(%1) - SUM(%1)*SUM(%1))\n"
            "b = (%4 * %7 - %3 * %6) / (%2 * %4 - %3 * %3)\n"
            "b = (%15 - %16) / (%10 - %11)\n"
            "b = %17 / %13\n"
            "b = %18\n"
        ).arg(measureColumnName)
         .arg(count_values)
         .arg(QString::number(sum_measured, 'f', 8))
         .arg(QString::number(sumsq_measured, 'f', 8))
         .arg(channelName)
         .arg(QString::number(sumProduct, 'f', 8))
         .arg(QString::number(totalStandardPressureSum, 'f', 8))
         .arg(QString::number(count_values * sumProduct, 'f', 8))
         .arg(QString::number(sum_measured * totalStandardPressureSum, 'f', 8))
         .arg(QString::number(count_values * sumsq_measured, 'f', 8))
         .arg(QString::number(sum_measured * sum_measured, 'f', 8))
         .arg(QString::number(numerator, 'f', 8))
         .arg(QString::number(denominator, 'f', 8))
         .arg(QString::number(a, 'f', 8))
         .arg(QString::number(sumsq_measured * totalStandardPressureSum, 'f', 8))
         .arg(QString::number(sum_measured * sumProduct, 'f', 8))
         .arg(QString::number(b_numerator, 'f', 8))
         .arg(QString::number(b, 'f', 8));

        calculationDetails += regressionDetails;
    }

    // 生成计算详情
    QStringList diffList;
    for (int i = 0; i < allDifferences.size(); ++i) {
        diffList.append(QString::number(allDifferences[i], 'f', 8));
    }
    QString diffListStr = "[" + diffList.join(", ") + "]";
    // 使用allSetValuesForRegression计算真正的所有数据点标准压力值总和（不去重）
    double totalStandardPressureSumForDisplay = accumulate(allSetValuesForRegression.begin(), allSetValuesForRegression.end(), 0.0);

    // 生成所有设定值的详细列表
    QStringList allSetValuesList;
    for (int i = 0; i < allSetValuesForRegression.size(); ++i) {
        allSetValuesList.append(QString::number(allSetValuesForRegression[i], 'f', 1));
    }
    QString allSetValuesStr = "[" + allSetValuesList.join(", ") + "]";

    calculationDetails += QString::fromUtf8(
        "\n===== 线性度计算详情 =====\n\n"
        "标准压力值总和: %1\n"
        "压力值个数: %2\n"
        "所有压力值列表: %3\n\n"
        "SUMPRODUCT计算过程:\n"
        "每个原始测量值(%4) × 对应的设定值(%5)：\n%6\n\n"
        "SUMPRODUCT总和: %7\n\n"
        "最小二乘法拟合直线: Y = %8x + %9\n\n"
        "===== 线性度记录量计算 =====\n"
        "所有差值列表(标准压力值-最小二乘法): %10\n"
        "MAX(ABS(差值)) = %11\n"
        "MAX(ABS(差值))/%12 = %11/%12 = %13\n"
    ).arg(QString::number(totalStandardPressureSumForDisplay, 'f', 8))
     .arg(allSetValuesForRegression.size())
     .arg(allSetValuesStr)
     .arg(measureColumnName)
     .arg(channelName)
     .arg(sumProductDetailsStr)
     .arg(QString::number(sumProduct, 'f', 8))
     .arg(QString::number(a, 'f', 8))
     .arg(QString::number(b, 'f', 8))
     .arg(diffListStr)
     .arg(QString::number(maxAbsDifference, 'f', 8))
     .arg(currentChannelRange, 0, 'f', 0)
     .arg(QString::number(maxDifferenceResult, 'f', 8));
}

void DataProcessor::setRangeParameters(double maxRange, double zeroThreshold)
{
    m_maxRange = maxRange;
    if (zeroThreshold > 0) {
        m_zeroThreshold = zeroThreshold;
    } else {
        // 自动设置零点阈值为最大量程的0.5%
        m_zeroThreshold = maxRange * 0.005;
    }
    m_autoDetected = true;
}

void DataProcessor::autoDetectRangeParameters(const QVector<QStringList>& originalData)
{
    autoDetectRangeParameters(originalData, "Fx");
}

void DataProcessor::autoDetectRangeParameters(const QVector<QStringList>& originalData, const QString& channelName)
{
    if (originalData.isEmpty()) {
        return;
    }

    QStringList headers = originalData[0];
    int channelColumnIndex = findColumnIndex(headers, channelName);

    if (channelColumnIndex == -1) {
        // 如果找不到指定通道列，使用默认参数
        m_maxRange = 210.0;
        m_zeroThreshold = 0.1;
        m_autoDetected = true;
        return;
    }

    // 收集所有通道值
    QVector<double> allChannelValues;
    for (int i = 1; i < originalData.size(); ++i) {
        const QStringList& row = originalData[i];
        if (channelColumnIndex < row.size()) {
            bool ok;
            double channelValue = row[channelColumnIndex].toDouble(&ok);
            if (ok) {
                allChannelValues.append(qAbs(channelValue));
            }
        }
    }

    if (allChannelValues.isEmpty()) {
        // 如果没有有效数据，使用默认参数
        m_maxRange = 210.0;
        m_zeroThreshold = 0.1;
        m_autoDetected = true;
        return;
    }

    // 找到最大绝对值作为量程
    double detectedMaxRange = 0.0;
    for (int i = 0; i < allChannelValues.size(); ++i) {
        if (allChannelValues[i] > detectedMaxRange) {
            detectedMaxRange = allChannelValues[i];
        }
    }

    // 设置参数
    m_maxRange = detectedMaxRange;

    // 如果使用手动量程，不自动覆盖
    if (!m_useManualRanges) {
        m_channelRanges[channelName] = detectedMaxRange;
    }

    // 零点阈值设为固定值，不随量程变化
    // 用于判断设定值是否为0（所有通道共用一个阈值）
    m_zeroThreshold = 1.0;  // 设定值绝对值小于1.0视为0点

    m_autoDetected = true;
}

// ==================== 新增：各通道独立量程设置方法 ====================

void DataProcessor::setChannelRange(const QString& channelName, double range)
{
    m_channelRanges[channelName] = range;
    m_useManualRanges = true;

    // 同时更新 m_maxRange 以保持向后兼容
    if (channelName == "Fx" || channelName == "Fy" || channelName == "Fz" ||
        channelName == "Mx" || channelName == "My" || channelName == "Mz") {
        m_maxRange = range;
    }
}

void DataProcessor::setAllChannelRanges(double fxRange, double fyRange, double fzRange,
                                       double mxRange, double myRange, double mzRange)
{
    m_channelRanges["Fx"] = fxRange;
    m_channelRanges["Fy"] = fyRange;
    m_channelRanges["Fz"] = fzRange;
    m_channelRanges["Mx"] = mxRange;
    m_channelRanges["My"] = myRange;
    m_channelRanges["Mz"] = mzRange;
    m_useManualRanges = true;
}

double DataProcessor::getChannelRange(const QString& channelName) const
{
    return m_channelRanges.value(channelName, 210.0);  // 默认返回210.0
}

void DataProcessor::resetToDefaultRanges()
{
    // 设置默认量程：Fx=1000, Fy=1000, Fz=1500, Mx=My=Mz=50
    m_channelRanges["Fx"] = 1000.0;
    m_channelRanges["Fy"] = 1000.0;
    m_channelRanges["Fz"] = 1500.0;
    m_channelRanges["Mx"] = 50.0;
    m_channelRanges["My"] = 50.0;
    m_channelRanges["Mz"] = 50.0;
    m_useManualRanges = true;  // 标记为手动设置（包括默认值），不允许自动检测覆盖
}