#include "DataLoader.h"
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDir>
#include <QDebug>

#pragma execution_character_set("utf-8")

DataLoader::DataLoader()
{
}

DataLoader::~DataLoader()
{
}

bool DataLoader::loadFile(const QString& filePath)
{
    data.clear();
    lastError.clear();

    if (filePath.isEmpty()) {
        lastError = "文件路径为空";
        return false;
    }

    bool success = false;
    if (filePath.endsWith(".xlsx") || filePath.endsWith(".xls")) {
        success = loadExcelFile(filePath);
    } else if (filePath.endsWith(".csv")) {
        success = loadCSVFile(filePath);
    } else {
        success = loadCSVFile(filePath);
    }

    if (!success && lastError.isEmpty()) {
        lastError = "无法加载文件，请检查文件格式";
    }

    return success;
}

bool DataLoader::loadCSVFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        lastError = QString("无法打开文件: %1").arg(filePath);
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (!line.isEmpty()) {
            QStringList fields = parseCSVLine(line);
            data.append(fields);
        }
    }

    file.close();
    return !data.isEmpty();
}

bool DataLoader::loadExcelFile(const QString& filePath)
{
    QString csvPath = convertExcelToCSV(filePath);
    if (csvPath.isEmpty()) {
        lastError = "无法转换Excel文件，请尝试将其另存为CSV格式";
        return false;
    }

    bool success = loadCSVFile(csvPath);
    QFile::remove(csvPath);
    return success;
}

QStringList DataLoader::parseCSVLine(const QString& line)
{
    QStringList fields;
    QString field;
    bool inQuotes = false;

    for (int i = 0; i < line.length(); ++i) {
        QChar ch = line[i];

        if (ch == '"') {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"') {
                field += '"';
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == ',' && !inQuotes) {
            fields.append(field.trimmed());
            field.clear();
        } else {
            field += ch;
        }
    }

    fields.append(field.trimmed());
    return fields;
}

QString DataLoader::convertExcelToCSV(const QString& excelPath)
{
    QString tempPath = QDir::temp().filePath("temp_data.csv");

    QString pythonScript = QString(
        "import sys\n"
        "try:\n"
        "    import pandas as pd\n"
        "    df = pd.read_excel('%1', sheet_name=0)\n"
        "    df.to_csv('%2', index=False, encoding='utf-8')\n"
        "    print('Success')\n"
        "except Exception as e:\n"
        "    print('Error: ' + str(e))\n"
    ).arg(excelPath).arg(tempPath);

    QFile scriptFile(QDir::temp().filePath("convert.py"));
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);
        out << pythonScript;
        scriptFile.close();

        QProcess process;
        process.start("python", QStringList() << scriptFile.fileName());
        process.waitForFinished(10000);

        QString output = process.readAllStandardOutput();
        scriptFile.remove();

        if (output.contains("Success") && QFile::exists(tempPath)) {
            return tempPath;
        }
    }

    return QString();
}