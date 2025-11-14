#include "DataAverager.h"
#include <QRegularExpression>

#pragma execution_character_set("utf-8")

DataAverager::DataAverager()
{
}

DataAverager::~DataAverager()
{
}

QVector<QStringList> DataAverager::averageData(
    const QVector<QStringList>& originalData,
    int groupSize)
{
    QVector<QStringList> result;
    m_lastError.clear();
    m_processInfo.clear();

    // 验证输入
    if (originalData.isEmpty()) {
        m_lastError = QString::fromUtf8("输入数据为空");
        return result;
    }

    if (groupSize <= 0) {
        m_lastError = QString::fromUtf8("分组大小必须大于0");
        return result;
    }

    // 提取表头
    QStringList headers = originalData[0];
    result.append(headers);

    // 计算有效数据行数
    int dataRowCount = originalData.size() - 1;
    if (dataRowCount == 0) {
        m_lastError = QString::fromUtf8("没有数据行可处理");
        return result;
    }

    // 按组处理数据
    int groupCount = 0;
    int processedRows = 0;

    for (int i = 1; i < originalData.size(); i += groupSize) {
        QVector<QStringList> group;

        // 收集当前组的数据
        for (int j = 0; j < groupSize && (i + j) < originalData.size(); ++j) {
            group.append(originalData[i + j]);
        }

        // 处理分组（计算平均值）
        if (!group.isEmpty()) {
            QStringList avgRow = processGroup(group, headers);
            result.append(avgRow);
            groupCount++;
            processedRows += group.size();
        }
    }

    // 生成处理信息
    m_processInfo = QString::fromUtf8(
        "数据平均处理完成\n"
        "原始数据行数: %1\n"
        "分组大小: %2\n"
        "生成组数: %3\n"
        "处理的数据行: %4\n"
        "平均后数据行数: %5"
    ).arg(dataRowCount)
     .arg(groupSize)
     .arg(groupCount)
     .arg(processedRows)
     .arg(result.size() - 1); // 减去表头

    return result;
}

bool DataAverager::isNumeric(const QString& str) const
{
    if (str.isEmpty()) {
        return false;
    }

    // 使用正则表达式匹配数值（包括整数、小数、负数、科学计数法）
    QRegularExpression numericPattern(R"(^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$)");
    return numericPattern.match(str.trimmed()).hasMatch();
}

QString DataAverager::calculateAverage(const QVector<QString>& values) const
{
    if (values.isEmpty()) {
        return "";
    }

    double sum = 0.0;
    int count = 0;

    for (const QString& val : values) {
        if (isNumeric(val)) {
            sum += val.toDouble();
            count++;
        }
    }

    if (count == 0) {
        // 如果没有有效数值，返回第一个值
        return values[0];
    }

    double average = sum / count;
    return QString::number(average, 'g', 10); // 'g' 格式自动选择最佳表示
}

QStringList DataAverager::processGroup(
    const QVector<QStringList>& group,
    const QStringList& headers) const
{
    QStringList result;

    if (group.isEmpty()) {
        return result;
    }

    int columnCount = headers.size();

    // 对每一列计算平均值
    for (int col = 0; col < columnCount; ++col) {
        QVector<QString> columnValues;

        // 收集该列的所有值
        for (const QStringList& row : group) {
            if (col < row.size()) {
                columnValues.append(row[col]);
            }
        }

        // 计算平均值
        QString avgValue = calculateAverage(columnValues);
        result.append(avgValue);
    }

    return result;
}
