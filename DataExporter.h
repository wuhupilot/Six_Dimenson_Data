#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class DataExporter
{
public:
    DataExporter();
    ~DataExporter();

    bool exportToCSV(const QString& filePath, const QVector<QStringList>& data);
    bool exportToExcel(const QString& filePath, const QVector<QStringList>& data);
    QString getLastError() const { return lastError; }

private:
    QString lastError;

    QString convertCSVToExcel(const QString& csvPath, const QString& excelPath);
};