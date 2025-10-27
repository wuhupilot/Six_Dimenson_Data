#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

class DataLoader
{
public:
    DataLoader();
    ~DataLoader();

    bool loadFile(const QString& filePath);
    QVector<QStringList> getData() const { return data; }
    QString getLastError() const { return lastError; }

private:
    QVector<QStringList> data;
    QString lastError;

    bool loadCSVFile(const QString& filePath);
    bool loadExcelFile(const QString& filePath);
    QStringList parseCSVLine(const QString& line);
    QString convertExcelToCSV(const QString& excelPath);
};