#include "StatisticsCalculator.h"
#include <QTextStream>
#include <QMap>

#pragma execution_character_set("utf-8")

StatisticsCalculator::StatisticsCalculator()
{
}

StatisticsCalculator::~StatisticsCalculator()
{
}

QString StatisticsCalculator::generateStatistics(
    const QString& filePath,
    const QVector<QStringList>& originalData,
    const QString& calculationDetails)
{
    QString stats;
    QTextStream stream(&stats);

    // 基本统计信息
    stream << generateBasicStatistics(filePath, originalData);

    // 列统计信息
    if (!originalData.isEmpty()) {
        stream << generateColumnStatistics(originalData);
    }

    // 计算详情
    if (!calculationDetails.isEmpty()) {
        stream << calculationDetails;
    }

    return stats;
}

QString StatisticsCalculator::generateBasicStatistics(const QString& filePath, const QVector<QStringList>& originalData)
{
    QString stats;
    QTextStream stream(&stats);

    stream << "===== 数据统计信息 =====\n\n";
    stream << QString("文件路径: %1\n").arg(filePath);
    stream << QString("加载时间: %1\n\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!originalData.isEmpty()) {
        stream << QString("总行数: %1\n").arg(originalData.size());
        if (originalData.size() > 0) {
            stream << QString("总列数: %1\n\n").arg(originalData[0].size());
        }
    }

    return stats;
}

QString StatisticsCalculator::generateColumnStatistics(const QVector<QStringList>& originalData)
{
    QString stats;
    QTextStream stream(&stats);

    if (originalData.isEmpty()) {
        return stats;
    }

    stream << "列信息:\n";

    const QStringList& headers = originalData[0];
    for (int i = 0; i < headers.size(); ++i) {
        stream << QString("  列 %1: %2\n").arg(i + 1).arg(headers[i]);

        QMap<QString, int> uniqueValues;
        int nonEmptyCount = 0;

        // 统计该列的数据
        for (int j = 1; j < originalData.size(); ++j) {
            if (i < originalData[j].size()) {
                QString value = originalData[j][i].trimmed();
                if (!value.isEmpty()) {
                    nonEmptyCount++;
                    uniqueValues[value]++;
                }
            }
        }

        stream << QString("    - 非空值数量: %1\n").arg(nonEmptyCount);
        stream << QString("    - 唯一值数量: %1\n").arg(uniqueValues.size());

        // 如果唯一值不多，显示所有唯一值
        if (uniqueValues.size() <= 10) {
            stream << "    - 唯一值列表: ";
            QStringList values = uniqueValues.keys();
            stream << values.join(", ") << "\n";
        }
        stream << "\n";
    }

    return stats;
}