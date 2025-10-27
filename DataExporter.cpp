#include "DataExporter.h"
#include <QFile>
#include <QTextStream>
#include <QProcess>
#include <QDir>

#pragma execution_character_set("utf-8")

DataExporter::DataExporter()
{
}

DataExporter::~DataExporter()
{
}

bool DataExporter::exportToCSV(const QString& filePath, const QVector<QStringList>& data)
{
    lastError.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        lastError = QString::fromUtf8("无法创建文件: %1").arg(filePath);
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");
    stream.setGenerateByteOrderMark(true);  // 添加UTF-8 BOM,确保Excel正确识别编码

    for (const QStringList& row : data) {
        QStringList escapedRow;
        for (const QString& field : row) {
            QString escaped = field;
            if (escaped.contains(',') || escaped.contains('"') || escaped.contains('\n')) {
                escaped = '"' + escaped.replace('"', "\"\"") + '"';
            }
            escapedRow.append(escaped);
        }
        stream << escapedRow.join(',') << "\n";
    }

    file.close();
    return true;
}

bool DataExporter::exportToExcel(const QString& filePath, const QVector<QStringList>& data)
{
    lastError.clear();

    // 先导出为临时CSV文件
    QString csvPath = QDir::temp().filePath("temp_export.csv");
    if (!exportToCSV(csvPath, data)) {
        return false;
    }

    // 转换为Excel
    QString result = convertCSVToExcel(csvPath, filePath);
    QFile::remove(csvPath);

    if (result.isEmpty()) {
        lastError = QString::fromUtf8("无法转换为Excel格式，请检查是否安装了Python和pandas库");
        return false;
    }

    return result == "Success";
}

QString DataExporter::convertCSVToExcel(const QString& csvPath, const QString& excelPath)
{
    QString pythonScript = QString(
        "import sys\n"
        "try:\n"
        "    import pandas as pd\n"
        "    df = pd.read_csv('%1', encoding='utf-8')\n"
        "    df.to_excel('%2', index=False)\n"
        "    print('Success')\n"
        "except Exception as e:\n"
        "    print('Error: ' + str(e))\n"
    ).arg(csvPath).arg(excelPath);

    QFile scriptFile(QDir::temp().filePath("export.py"));
    if (scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&scriptFile);
        out << pythonScript;
        scriptFile.close();

        QProcess process;
        process.start("python", QStringList() << scriptFile.fileName());
        process.waitForFinished(10000);

        QString output = process.readAllStandardOutput();
        scriptFile.remove();

        if (output.contains("Success")) {
            return "Success";
        }
    }

    return QString();
}