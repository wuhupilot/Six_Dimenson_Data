#pragma once
#pragma execution_character_set("utf-8")

#include <QString>
#include <QStringList>
#include <QVector>
#include <QDateTime>

class StatisticsCalculator
{
public:
    StatisticsCalculator();
    ~StatisticsCalculator();

    QString generateStatistics(
        const QString& filePath,
        const QVector<QStringList>& originalData,
        const QString& calculationDetails
    );

private:
    QString generateBasicStatistics(const QString& filePath, const QVector<QStringList>& originalData);
    QString generateColumnStatistics(const QVector<QStringList>& originalData);
};