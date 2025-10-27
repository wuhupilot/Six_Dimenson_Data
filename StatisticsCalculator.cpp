#include "StatisticsCalculator.h"
#include <QTextStream>
#include <QMap>
#include <QDateTime>

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

    stream << QString::fromUtf8("===== 数据统计信息 =====\n\n");
    stream << QString::fromUtf8("文件路径: %1\n").arg(filePath);
    stream << QString::fromUtf8("加载时间: %1\n\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!originalData.isEmpty()) {
        stream << QString::fromUtf8("总行数: %1\n").arg(originalData.size());
        if (originalData.size() > 0) {
            stream << QString::fromUtf8("总列数: %1\n\n").arg(originalData[0].size());
        }
    }

    if (!calculationDetails.isEmpty()) {
        stream << calculationDetails;
    }

    return stats;
}

QString StatisticsCalculator::generateBasicStatistics(const QString& filePath, const QVector<QStringList>& originalData)
{
    QString stats;
    QTextStream stream(&stats);

    stream << QString::fromUtf8("===== 数据统计信息 =====\n\n");
    stream << QString::fromUtf8("文件路径: %1\n").arg(filePath);
    stream << QString::fromUtf8("加载时间: %1\n\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!originalData.isEmpty()) {
        stream << QString::fromUtf8("总行数: %1\n").arg(originalData.size());
        if (originalData.size() > 0) {
            stream << QString::fromUtf8("总列数: %1\n\n").arg(originalData[0].size());
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

    stream << QString::fromUtf8("列信息:\n");

    const QStringList& headers = originalData[0];
    for (int i = 0; i < headers.size(); ++i) {
        stream << QString::fromUtf8("  列 %1: %2\n").arg(i + 1).arg(headers[i]);

        QMap<QString, int> uniqueValues;
        int nonEmptyCount = 0;

        for (int j = 1; j < originalData.size(); ++j) {
            if (i < originalData[j].size()) {
                QString value = originalData[j][i].trimmed();
                if (!value.isEmpty()) {
                    nonEmptyCount++;
                    uniqueValues[value]++;
                }
            }
        }

        stream << QString::fromUtf8("    - 非空值数量: %1\n").arg(nonEmptyCount);
        stream << QString::fromUtf8("    - 唯一值数量: %1\n").arg(uniqueValues.size());

        if (uniqueValues.size() <= 10) {
            stream << QString::fromUtf8("    - 唯一值列表: ");
            QStringList values = uniqueValues.keys();
            stream << values.join(", ") << "\n";
        }
        stream << "\n";
    }

    return stats;
}