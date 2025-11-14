#include "DataZeroCalibrator.h"
#include <QRegularExpression>
#include <cmath>

#pragma execution_character_set("utf-8")

// 静态常量定义
const QStringList DataZeroCalibrator::U_COLUMN_NAMES = {"UFx", "UFy", "UFz", "UMx", "UMy", "UMz"};
const QStringList DataZeroCalibrator::F_COLUMN_NAMES = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};

DataZeroCalibrator::DataZeroCalibrator()
{
}

DataZeroCalibrator::~DataZeroCalibrator()
{
}

QVector<QStringList> DataZeroCalibrator::calibrateZeroPoints(
    const QVector<QStringList>& originalData,
    double zeroThreshold)
{
    QVector<QStringList> result;
    m_lastError.clear();
    m_processInfo.clear();

    // 验证输入
    if (originalData.isEmpty()) {
        m_lastError = QString::fromUtf8("输入数据为空");
        return result;
    }

    // 提取表头
    QStringList headers = originalData[0];
    result.append(headers);

    if (originalData.size() <= 1) {
        m_lastError = QString::fromUtf8("没有数据行可处理");
        return result;
    }

    // 查找所有零点
    QVector<int> zeroPointIndices = findZeroPoints(originalData, headers, zeroThreshold);

    if (zeroPointIndices.isEmpty()) {
        m_lastError = QString::fromUtf8("未找到零点（Fx=Fy=Fz=Mx=My=Mz≈0的行）");
        return result;
    }

    // 将零点分组（每4个零点为一组）
    QVector<QVector<int>> zeroPointGroups = groupZeroPoints(zeroPointIndices);

    if (zeroPointGroups.isEmpty()) {
        m_lastError = QString::fromUtf8("零点数量不足，至少需要4个零点才能形成一个测量循环组");
        return result;
    }

    // 处理每个测量循环组
    int totalCalibratedRows = 0;
    int currentGroup = 0;

    for (const QVector<int>& group : zeroPointGroups) {
        if (group.size() < 4) {
            continue; // 跳过不完整的组
        }

        currentGroup++;

        // 第1个零点的索引
        int firstZeroIndex = group[0];
        // 第4个零点的索引
        int fourthZeroIndex = group[3];

        // 获取第1个零点的数据行（作为基准）
        QStringList zeroPointRow = originalData[firstZeroIndex];

        // 处理从第1个零点到第4个零点之间的所有数据行
        for (int i = firstZeroIndex; i <= fourthZeroIndex && i < originalData.size(); ++i) {
            QStringList calibratedRow = applyZeroCalibration(
                originalData[i],
                zeroPointRow,
                headers
            );
            result.append(calibratedRow);
            totalCalibratedRows++;
        }
    }

    // 处理最后不完整的组（如果有的话）
    if (!zeroPointGroups.isEmpty()) {
        const QVector<int>& lastGroup = zeroPointGroups.last();
        if (lastGroup.size() >= 1) {
            int lastProcessedIndex = (lastGroup.size() >= 4) ? lastGroup[3] : lastGroup.last();

            // 如果还有剩余数据行，也进行处理
            if (lastProcessedIndex < originalData.size() - 1) {
                int firstZeroIndex = lastGroup[0];
                QStringList zeroPointRow = originalData[firstZeroIndex];

                for (int i = lastProcessedIndex + 1; i < originalData.size(); ++i) {
                    QStringList calibratedRow = applyZeroCalibration(
                        originalData[i],
                        zeroPointRow,
                        headers
                    );
                    result.append(calibratedRow);
                    totalCalibratedRows++;
                }
            }
        }
    }

    // 生成处理信息
    m_processInfo = QString::fromUtf8(
        "置零校准完成\n"
        "原始数据行数: %1\n"
        "找到零点数量: %2\n"
        "零点位置: [%3]\n"
        "测量循环组数: %4\n"
        "校准后数据行数: %5\n"
        "零点判断阈值: %6"
    ).arg(originalData.size() - 1)
     .arg(zeroPointIndices.size())
     .arg([&]() {
         QStringList indices;
         for (int idx : zeroPointIndices) {
             indices.append(QString::number(idx));
         }
         return indices.join(", ");
     }())
     .arg(zeroPointGroups.size())
     .arg(totalCalibratedRows)
     .arg(zeroThreshold);

    return result;
}

bool DataZeroCalibrator::isZeroPoint(
    const QStringList& row,
    const QStringList& headers,
    double threshold) const
{
    // 检查 Fx, Fy, Fz, Mx, My, Mz 是否都接近0
    for (const QString& fColName : F_COLUMN_NAMES) {
        int colIndex = getColumnIndex(headers, fColName);
        if (colIndex < 0 || colIndex >= row.size()) {
            return false;
        }

        QString valueStr = row[colIndex];
        if (!isNumeric(valueStr)) {
            return false;
        }

        double value = valueStr.toDouble();
        if (!isNearZero(value, threshold)) {
            return false;
        }
    }

    return true;
}

QVector<int> DataZeroCalibrator::findZeroPoints(
    const QVector<QStringList>& data,
    const QStringList& headers,
    double threshold) const
{
    QVector<int> zeroPoints;

    // 从索引1开始（跳过表头）
    for (int i = 1; i < data.size(); ++i) {
        if (isZeroPoint(data[i], headers, threshold)) {
            zeroPoints.append(i);
        }
    }

    return zeroPoints;
}

QVector<QVector<int>> DataZeroCalibrator::groupZeroPoints(const QVector<int>& zeroPointIndices) const
{
    QVector<QVector<int>> groups;

    // 每4个零点为一组
    for (int i = 0; i + 3 < zeroPointIndices.size(); i += 4) {
        QVector<int> group;
        group.append(zeroPointIndices[i]);
        group.append(zeroPointIndices[i + 1]);
        group.append(zeroPointIndices[i + 2]);
        group.append(zeroPointIndices[i + 3]);
        groups.append(group);
    }

    return groups;
}

int DataZeroCalibrator::getColumnIndex(const QStringList& headers, const QString& columnName) const
{
    return headers.indexOf(columnName);
}

bool DataZeroCalibrator::isNumeric(const QString& str) const
{
    if (str.isEmpty()) {
        return false;
    }

    QRegularExpression numericPattern(R"(^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$)");
    return numericPattern.match(str.trimmed()).hasMatch();
}

bool DataZeroCalibrator::isNearZero(double value, double threshold) const
{
    return std::abs(value) <= threshold;
}

QStringList DataZeroCalibrator::applyZeroCalibration(
    const QStringList& row,
    const QStringList& zeroPointRow,
    const QStringList& headers) const
{
    QStringList result = row;

    // 对每个U值列进行校准
    for (const QString& uColName : U_COLUMN_NAMES) {
        int colIndex = getColumnIndex(headers, uColName);
        if (colIndex < 0 || colIndex >= row.size() || colIndex >= zeroPointRow.size()) {
            continue;
        }

        QString valueStr = row[colIndex];
        QString zeroValueStr = zeroPointRow[colIndex];

        if (isNumeric(valueStr) && isNumeric(zeroValueStr)) {
            double value = valueStr.toDouble();
            double zeroValue = zeroValueStr.toDouble();
            double calibratedValue = value - zeroValue;

            result[colIndex] = QString::number(calibratedValue, 'g', 10);
        }
    }

    return result;
}
