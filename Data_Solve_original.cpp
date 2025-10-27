#include "Data_Solve.h"
#include <QTextStream>
#include <QDebug>
#include <QProcess>
#include <QDir>
#include <QDateTime>
#include <algorithm>
#include <numeric>
#include <cmath>
#pragma execution_character_set("utf-8")
Data_Solve::Data_Solve(QWidget *parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    connect(ui.btnLoadData, &QPushButton::clicked, this, &Data_Solve::onLoadDataClicked);
    connect(ui.btnProcessData, &QPushButton::clicked, this, &Data_Solve::onProcessDataClicked);
    connect(ui.btnExportData, &QPushButton::clicked, this, &Data_Solve::onExportDataClicked);
    connect(ui.btnClearData, &QPushButton::clicked, this, &Data_Solve::onClearDataClicked);

    connect(ui.actionOpen, &QAction::triggered, this, &Data_Solve::onActionOpenTriggered);
    connect(ui.actionSave, &QAction::triggered, this, &Data_Solve::onActionSaveTriggered);
    connect(ui.actionExit, &QAction::triggered, this, &Data_Solve::onActionExitTriggered);
    connect(ui.actionAbout, &QAction::triggered, this, &Data_Solve::onActionAboutTriggered);

    updateStatus("程序启动完成，请加载数据文件");
}

Data_Solve::~Data_Solve()
{
}

void Data_Solve::onLoadDataClicked()
{
    QString fileName = QFileDialog::getOpenFileName(this,
        "选择数据文件",
        QDir::currentPath(),
        "Excel Files (*.xlsx *.xls);;CSV Files (*.csv);;All Files (*)");

    if (!fileName.isEmpty()) {
        currentFilePath = fileName;
        bool success = false;

        if (fileName.endsWith(".xlsx") || fileName.endsWith(".xls")) {
            updateStatus("正在尝试加载Excel文件...");
            QString csvPath = convertExcelToCSV(fileName);
            if (!csvPath.isEmpty()) {
                success = loadCSVFile(csvPath);
                QFile::remove(csvPath);
            }
        } else if (fileName.endsWith(".csv")) {
            success = loadCSVFile(fileName);
        } else {
            success = loadCSVFile(fileName);
        }

        if (success) {
            displayDataInTable(ui.tableOriginalData, originalData);
            ui.btnProcessData->setEnabled(true);
            ui.btnExportData->setEnabled(true);
            updateStatus(QString("Successfully loaded %1 rows of data").arg(originalData.size()));
            calculateStatistics();
        } else {
            showError("Load Failed", "Cannot load data file, please check file format");
        }
    }
}

bool Data_Solve::loadCSVFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    originalData.clear();
    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    while (!stream.atEnd()) {
        QString line = stream.readLine();
        if (!line.isEmpty()) {
            QStringList fields = parseCSVLine(line);
            originalData.append(fields);
        }
    }

    file.close();
    return !originalData.isEmpty();
}

QStringList Data_Solve::parseCSVLine(const QString& line)
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

QString Data_Solve::convertExcelToCSV(const QString& excelPath)
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
        QString error = process.readAllStandardError();

        scriptFile.remove();

        if (output.contains("Success") && QFile::exists(tempPath)) {
            return tempPath;
        }
    }

    updateStatus("无法直接读取Excel文件，请尝试将其另存为CSV格式");
    return QString();
}

bool Data_Solve::loadExcelFile(const QString& filePath)
{
    QString csvPath = convertExcelToCSV(filePath);
    if (csvPath.isEmpty()) {
        return false;
    }

    bool success = loadCSVFile(csvPath);
    QFile::remove(csvPath);
    return success;
}

void Data_Solve::displayDataInTable(QTableWidget* table, const QVector<QStringList>& data)
{
    if (data.isEmpty()) {
        return;
    }

    table->clear();
    table->setRowCount(data.size() - 1);

    if (data.size() > 0) {
        QStringList headers = data[0];
        table->setColumnCount(headers.size());
        table->setHorizontalHeaderLabels(headers);

        for (int i = 1; i < data.size(); ++i) {
            const QStringList& row = data[i];
            for (int j = 0; j < row.size() && j < headers.size(); ++j) {
                QTableWidgetItem* item = new QTableWidgetItem(row[j]);
                table->setItem(i - 1, j, item);
            }
        }
    }

    table->resizeColumnsToContents();
}

void Data_Solve::onProcessDataClicked()
{
    updateStatus("正在处理数据...");
    processData();
    processLinearityData();
    displayDataInTable(ui.tableProcessedData, processedData);
    displayDataInTable(ui.tableLinearityData, linearityData);
    ui.tabWidget->setCurrentIndex(1);
    updateStatus("数据处理完成");
}

void Data_Solve::processData()
{
    processedData.clear();

    if (originalData.size() <= 1) {
        updateStatus("没有数据可处理");
        return;
    }

    // 标准差计算函数
    auto calculateStandardDeviation = [](const QVector<double>& values, double mean) -> double {
        if (values.size() <= 1) return 0.0;

        double sumSquares = 0.0;
        for (double val : values) {
            double diff = val - mean;
            sumSquares += diff * diff;
        }

        return sqrt(sumSquares / (values.size() - 1));
    };

    QStringList headers = originalData[0];

    // 找到所有相关列的索引
    int fxColumnIndex = -1;
    int ufxColumnIndex = -1;
    int fyColumnIndex = -1;
    int fzColumnIndex = -1;
    int mxColumnIndex = -1;
    int myColumnIndex = -1;
    int mzColumnIndex = -1;

    for (int i = 0; i < headers.size(); ++i) {
        QString header = headers[i];
        if (header.compare("Fx", Qt::CaseInsensitive) == 0) {
            fxColumnIndex = i;
        } else if (header.compare("UFx", Qt::CaseInsensitive) == 0) {
            ufxColumnIndex = i;
        } else if (header.compare("Fy", Qt::CaseInsensitive) == 0) {
            fyColumnIndex = i;
        } else if (header.compare("Fz", Qt::CaseInsensitive) == 0) {
            fzColumnIndex = i;
        } else if (header.compare("Mx", Qt::CaseInsensitive) == 0) {
            mxColumnIndex = i;
        } else if (header.compare("My", Qt::CaseInsensitive) == 0) {
            myColumnIndex = i;
        } else if (header.compare("Mz", Qt::CaseInsensitive) == 0) {
            mzColumnIndex = i;
        }
    }

    updateStatus(QString("找到列索引 - Fx:%1 UFx:%2 Fy:%3 Fz:%4 Mx:%5 My:%6 Mz:%7")
                .arg(fxColumnIndex).arg(ufxColumnIndex).arg(fyColumnIndex)
                .arg(fzColumnIndex).arg(mxColumnIndex).arg(myColumnIndex).arg(mzColumnIndex));

    if (fxColumnIndex == -1 || ufxColumnIndex == -1) {
        // 显示所有列名进行调试
        QString debugInfo = "列名列表: ";
        for (int i = 0; i < headers.size(); ++i) {
            debugInfo += QString("[%1]%2 ").arg(i).arg(headers[i]);
        }
        updateStatus(debugInfo);
        processedData = originalData;
        return;
    }

    // 第一步：收集纯Fx测试的数据
    struct FxDataPoint {
        double fxValue;
        double ufxValue;
        int rowIndex;
        QString edgeType; // "上升", "下降", "平稳"
    };

    QVector<FxDataPoint> pureFxData;

    updateStatus("开始收集纯Fx数据...");

    // 收集所有纯Fx测试的数据点
    for (int i = 1; i < originalData.size(); ++i) {
        const QStringList& row = originalData[i];
        if (fxColumnIndex >= row.size() || ufxColumnIndex >= row.size()) continue;

        bool fxOk, ufxOk;
        double fxValue = row[fxColumnIndex].toDouble(&fxOk);
        double ufxValue = row[ufxColumnIndex].toDouble(&ufxOk);

        if (!fxOk || !ufxOk) continue;

        // 检查是否是纯Fx测试
        bool isPureFxTest = true;
        QVector<int> otherColumns = {fyColumnIndex, fzColumnIndex, mxColumnIndex, myColumnIndex, mzColumnIndex};

        for (int colIndex : otherColumns) {
            if (colIndex != -1 && colIndex < row.size()) {
                bool ok;
                double value = row[colIndex].toDouble(&ok);
                if (ok && qAbs(value) > 0.1) {
                    isPureFxTest = false;
                    break;
                }
            }
        }

        if (isPureFxTest) {
            // 判断边沿类型
            QString edgeType = "平稳";
            if (i > 1 && i < originalData.size() - 1) {
                // 获取前一个和后一个Fx值
                bool prevOk = false, nextOk = false;
                double prevFx = -999, nextFx = -999;

                if (fxColumnIndex < originalData[i-1].size()) {
                    prevFx = originalData[i-1][fxColumnIndex].toDouble(&prevOk);
                }
                if (fxColumnIndex < originalData[i+1].size()) {
                    nextFx = originalData[i+1][fxColumnIndex].toDouble(&nextOk);
                }

                if (prevOk && nextOk) {
                    if (prevFx < fxValue && fxValue < nextFx) {
                        edgeType = "上升";
                    } else if (prevFx > fxValue && fxValue > nextFx) {
                        edgeType = "下降";
                    } else if (prevFx < fxValue && fxValue > nextFx) {
                        edgeType = "上升+下降";
                    } else if (prevFx > fxValue && fxValue < nextFx) {
                        edgeType = "下降+上升";
                    }
                }
            }

            pureFxData.append({fxValue, ufxValue, i, edgeType});
        }
    }

    updateStatus(QString("收集到 %1 个纯Fx数据点").arg(pureFxData.size()));

    // 第二步：分别处理正值循环和负值循环
    QMap<double, QVector<FxDataPoint>> positiveFxGroups; // 正值循环
    QMap<double, QVector<FxDataPoint>> negativeFxGroups; // 负值循环

    for (int i = 0; i < pureFxData.size(); ++i) {
        const FxDataPoint& point = pureFxData[i];

        if (qAbs(point.fxValue) < 0.1) { // 是0点，需要根据邻近值判断
            // 检查相邻的点(i-1或i+1)
            bool foundPositive = false, foundNegative = false;

            // 检查前一个点
            if (i > 0 && qAbs(pureFxData[i-1].fxValue) > 0.1) {
                if (pureFxData[i-1].fxValue > 0) foundPositive = true;
                else foundNegative = true;
            }

            // 检查后一个点（如果前面没找到非0值）
            if (!foundPositive && !foundNegative && i < pureFxData.size() - 1 && qAbs(pureFxData[i+1].fxValue) > 0.1) {
                if (pureFxData[i+1].fxValue > 0) foundPositive = true;
                else foundNegative = true;
            }

            // 根据找到的非0值决定分组
            if (foundPositive) {
                positiveFxGroups[0].append(point);
            } else if (foundNegative) {
                negativeFxGroups[0].append(point);
            }
        } else if (point.fxValue > 0) {
            positiveFxGroups[point.fxValue].append(point);
        } else {
            negativeFxGroups[point.fxValue].append(point);
        }
    }

    // 用于收集标准差的容器
    QVector<double> positiveStdDevs; // 正值循环的所有标准差
    QVector<double> negativeStdDevs; // 负值循环的所有标准差

    // 用于收集所有差值的容器
    QVector<double> allAvgDifferences; // 所有正行程平均值-反行程平均值

    // 创建详细数据表头
    QStringList detailHeaders;
    detailHeaders << "循环类型" << "Fx设定值" << "上升沿平均值" << "上升沿标准差" << "上升沿数值列表" << "下降沿平均值" << "下降沿标准差" << "下降沿数值列表" << "|正行程平均值-反行程平均值|";
    processedData.append(detailHeaders);

    // 处理正值循环
    updateStatus(QString("处理正值循环，共 %1 个设定值").arg(positiveFxGroups.size()));
    for (auto it = positiveFxGroups.begin(); it != positiveFxGroups.end(); ++it) {
        double fxValue = it.key();
        QVector<FxDataPoint> points = it.value();

        if (qAbs(fxValue) < 0.1) { // 特殊处理0点
            // 对0点按顺序分组：1,2,3为上升沿，2,3,4为下降沿
            QVector<double> risingValues;
            QVector<double> fallingValues;

            if (points.size() >= 4) {
                // 1,2,3号0点为上升沿
                for (int i = 0; i < 3; ++i) {
                    risingValues.append(points[i].ufxValue);
                }
                // 2,3,4号0点为下降沿
                for (int i = 1; i < 4; ++i) {
                    fallingValues.append(points[i].ufxValue);
                }
            } else {
                // 如果不足4个0点，按原逻辑处理
                for (int i = 0; i < points.size(); ++i) {
                    if (i < 3) {
                        risingValues.append(points[i].ufxValue);
                    } else {
                        fallingValues.append(points[i].ufxValue);
                    }
                }
            }

            // 计算平均值
            double risingAvg = 0.0;
            double fallingAvg = 0.0;

            if (!risingValues.isEmpty()) {
                double sum = 0.0;
                for (double val : risingValues) sum += val;
                risingAvg = sum / risingValues.size();
            }

            if (!fallingValues.isEmpty()) {
                double sum = 0.0;
                for (double val : fallingValues) sum += val;
                fallingAvg = sum / fallingValues.size();
            }

            // 计算标准差
            double risingStdDev = calculateStandardDeviation(risingValues, risingAvg);
            double fallingStdDev = calculateStandardDeviation(fallingValues, fallingAvg);

            // 收集标准差用于计算标准偏差
            if (!risingValues.isEmpty()) positiveStdDevs.append(risingStdDev);
            if (!fallingValues.isEmpty()) positiveStdDevs.append(fallingStdDev);

            // 生成数值列表字符串
            QString risingValuesList = "";
            if (!risingValues.isEmpty()) {
                QStringList risingStrList;
                for (double val : risingValues) {
                    risingStrList.append(QString::number(val, 'f', 4));
                }
                risingValuesList = "[" + risingStrList.join(", ") + "]";
            }

            QString fallingValuesList = "";
            if (!fallingValues.isEmpty()) {
                QStringList fallingStrList;
                for (double val : fallingValues) {
                    fallingStrList.append(QString::number(val, 'f', 4));
                }
                fallingValuesList = "[" + fallingStrList.join(", ") + "]";
            }

            // 计算正行程平均值-反行程平均值（取绝对值）
            QString avgDifference = "";
            if (!risingValues.isEmpty() && !fallingValues.isEmpty()) {
                double difference = qAbs(risingAvg - fallingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else if (!risingValues.isEmpty()) {
                double difference = qAbs(risingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else if (!fallingValues.isEmpty()) {
                double difference = qAbs(fallingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else {
                avgDifference = "无";
            }

            QStringList resultRow;
            resultRow << "正值循环"
                      << QString::number(fxValue, 'f', 1)
                      << (risingValues.isEmpty() ? "无" : QString::number(risingAvg, 'f', 4))
                      << (risingValues.isEmpty() ? "无" : QString::number(risingStdDev, 'f', 4))
                      << (risingValues.isEmpty() ? "无" : risingValuesList)
                      << (fallingValues.isEmpty() ? "无" : QString::number(fallingAvg, 'f', 4))
                      << (fallingValues.isEmpty() ? "无" : QString::number(fallingStdDev, 'f', 4))
                      << (fallingValues.isEmpty() ? "无" : fallingValuesList)
                      << avgDifference; // 正行程平均值-反行程平均值

            processedData.append(resultRow);
        } else {
            // 非0点按边沿类型分组
            QVector<double> risingValues;
            QVector<double> fallingValues;

            for (const FxDataPoint& point : points) {
                if (point.edgeType.contains("上升")) {
                    risingValues.append(point.ufxValue);
                }
                if (point.edgeType.contains("下降")) {
                    fallingValues.append(point.ufxValue);
                }
            }

            // 计算平均值
            double risingAvg = 0.0;
            double fallingAvg = 0.0;

            if (!risingValues.isEmpty()) {
                double sum = 0.0;
                for (double val : risingValues) sum += val;
                risingAvg = sum / risingValues.size();
            }

            if (!fallingValues.isEmpty()) {
                double sum = 0.0;
                for (double val : fallingValues) sum += val;
                fallingAvg = sum / fallingValues.size();
            }

            // 计算标准差
            double risingStdDev = calculateStandardDeviation(risingValues, risingAvg);
            double fallingStdDev = calculateStandardDeviation(fallingValues, fallingAvg);

            // 生成数值列表字符串
            QString risingValuesList = "";
            if (!risingValues.isEmpty()) {
                QStringList risingStrList;
                for (double val : risingValues) {
                    risingStrList.append(QString::number(val, 'f', 4));
                }
                risingValuesList = "[" + risingStrList.join(", ") + "]";
            }

            QString fallingValuesList = "";
            if (!fallingValues.isEmpty()) {
                QStringList fallingStrList;
                for (double val : fallingValues) {
                    fallingStrList.append(QString::number(val, 'f', 4));
                }
                fallingValuesList = "[" + fallingStrList.join(", ") + "]";
            }

            // 收集标准差用于计算标准偏差
            if (!risingValues.isEmpty()) positiveStdDevs.append(risingStdDev);
            if (!fallingValues.isEmpty()) positiveStdDevs.append(fallingStdDev);

            // 计算正行程平均值-反行程平均值（取绝对值）
            QString avgDifference = "";
            if (!risingValues.isEmpty() && !fallingValues.isEmpty()) {
                double difference = qAbs(risingAvg - fallingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else if (!risingValues.isEmpty()) {
                double difference = qAbs(risingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else if (!fallingValues.isEmpty()) {
                double difference = qAbs(fallingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else {
                avgDifference = "无";
            }

            QStringList resultRow;
            resultRow << "正值循环"
                      << QString::number(fxValue, 'f', 1)
                      << (risingValues.isEmpty() ? "无" : QString::number(risingAvg, 'f', 4))
                      << (risingValues.isEmpty() ? "无" : QString::number(risingStdDev, 'f', 4))
                      << (risingValues.isEmpty() ? "无" : risingValuesList)
                      << (fallingValues.isEmpty() ? "无" : QString::number(fallingAvg, 'f', 4))
                      << (fallingValues.isEmpty() ? "无" : QString::number(fallingStdDev, 'f', 4))
                      << (fallingValues.isEmpty() ? "无" : fallingValuesList)
                      << avgDifference; // 正行程平均值-反行程平均值

            processedData.append(resultRow);
        }
    }

    // 处理负值循环
    updateStatus(QString("处理负值循环，共 %1 个设定值").arg(negativeFxGroups.size()));
    for (auto it = negativeFxGroups.begin(); it != negativeFxGroups.end(); ++it) {
        double fxValue = it.key();
        QVector<FxDataPoint> points = it.value();

        if (qAbs(fxValue) < 0.1) { // 特殊处理0点
            // 对0点按顺序分组：1,2,3为上升沿，2,3,4为下降沿
            QVector<double> risingValues;
            QVector<double> fallingValues;

            if (points.size() >= 4) {
                // 1,2,3号0点为上升沿
                for (int i = 0; i < 3; ++i) {
                    risingValues.append(points[i].ufxValue);
                }
                // 2,3,4号0点为下降沿
                for (int i = 1; i < 4; ++i) {
                    fallingValues.append(points[i].ufxValue);
                }
            } else {
                // 如果不足4个0点，按原逻辑处理
                for (int i = 0; i < points.size(); ++i) {
                    if (i < 3) {
                        risingValues.append(points[i].ufxValue);
                    } else {
                        fallingValues.append(points[i].ufxValue);
                    }
                }
            }

            // 计算平均值
            double risingAvg = 0.0;
            double fallingAvg = 0.0;

            if (!risingValues.isEmpty()) {
                double sum = 0.0;
                for (double val : risingValues) sum += val;
                risingAvg = sum / risingValues.size();
            }

            if (!fallingValues.isEmpty()) {
                double sum = 0.0;
                for (double val : fallingValues) sum += val;
                fallingAvg = sum / fallingValues.size();
            }

            // 计算标准差
            double risingStdDev = calculateStandardDeviation(risingValues, risingAvg);
            double fallingStdDev = calculateStandardDeviation(fallingValues, fallingAvg);

            // 收集标准差用于计算标准偏差
            if (!risingValues.isEmpty()) negativeStdDevs.append(risingStdDev);
            if (!fallingValues.isEmpty()) negativeStdDevs.append(fallingStdDev);

            // 生成数值列表字符串
            QString risingValuesList = "";
            if (!risingValues.isEmpty()) {
                QStringList risingStrList;
                for (double val : risingValues) {
                    risingStrList.append(QString::number(val, 'f', 4));
                }
                risingValuesList = "[" + risingStrList.join(", ") + "]";
            }

            QString fallingValuesList = "";
            if (!fallingValues.isEmpty()) {
                QStringList fallingStrList;
                for (double val : fallingValues) {
                    fallingStrList.append(QString::number(val, 'f', 4));
                }
                fallingValuesList = "[" + fallingStrList.join(", ") + "]";
            }

            // 计算正行程平均值-反行程平均值（取绝对值）
            QString avgDifference = "";
            if (!risingValues.isEmpty() && !fallingValues.isEmpty()) {
                double difference = qAbs(risingAvg - fallingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else if (!risingValues.isEmpty()) {
                double difference = qAbs(risingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else if (!fallingValues.isEmpty()) {
                double difference = qAbs(fallingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else {
                avgDifference = "无";
            }

            QStringList resultRow;
            resultRow << "负值循环"
                      << QString::number(fxValue, 'f', 1)
                      << (risingValues.isEmpty() ? "无" : QString::number(risingAvg, 'f', 4))
                      << (risingValues.isEmpty() ? "无" : QString::number(risingStdDev, 'f', 4))
                      << (risingValues.isEmpty() ? "无" : risingValuesList)
                      << (fallingValues.isEmpty() ? "无" : QString::number(fallingAvg, 'f', 4))
                      << (fallingValues.isEmpty() ? "无" : QString::number(fallingStdDev, 'f', 4))
                      << (fallingValues.isEmpty() ? "无" : fallingValuesList)
                      << avgDifference; // 正行程平均值-反行程平均值

            processedData.append(resultRow);
        } else {
            // 非0点按边沿类型分组
            QVector<double> risingValues;
            QVector<double> fallingValues;

            for (const FxDataPoint& point : points) {
                if (point.edgeType.contains("上升")) {
                    risingValues.append(point.ufxValue);
                }
                if (point.edgeType.contains("下降")) {
                    fallingValues.append(point.ufxValue);
                }
            }

            // 计算平均值
            double risingAvg = 0.0;
            double fallingAvg = 0.0;

            if (!risingValues.isEmpty()) {
                double sum = 0.0;
                for (double val : risingValues) sum += val;
                risingAvg = sum / risingValues.size();
            }

            if (!fallingValues.isEmpty()) {
                double sum = 0.0;
                for (double val : fallingValues) sum += val;
                fallingAvg = sum / fallingValues.size();
            }

            // 计算标准差
            double risingStdDev = calculateStandardDeviation(risingValues, risingAvg);
            double fallingStdDev = calculateStandardDeviation(fallingValues, fallingAvg);

            // 生成数值列表字符串
            QString risingValuesList = "";
            if (!risingValues.isEmpty()) {
                QStringList risingStrList;
                for (double val : risingValues) {
                    risingStrList.append(QString::number(val, 'f', 4));
                }
                risingValuesList = "[" + risingStrList.join(", ") + "]";
            }

            QString fallingValuesList = "";
            if (!fallingValues.isEmpty()) {
                QStringList fallingStrList;
                for (double val : fallingValues) {
                    fallingStrList.append(QString::number(val, 'f', 4));
                }
                fallingValuesList = "[" + fallingStrList.join(", ") + "]";
            }

            // 收集标准差用于计算标准偏差
            if (!risingValues.isEmpty()) negativeStdDevs.append(risingStdDev);
            if (!fallingValues.isEmpty()) negativeStdDevs.append(fallingStdDev);

            // 计算正行程平均值-反行程平均值（取绝对值）
            QString avgDifference = "";
            if (!risingValues.isEmpty() && !fallingValues.isEmpty()) {
                double difference = qAbs(risingAvg - fallingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else if (!risingValues.isEmpty()) {
                double difference = qAbs(risingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else if (!fallingValues.isEmpty()) {
                double difference = qAbs(fallingAvg);
                avgDifference = QString::number(difference, 'f', 4);
                allAvgDifferences.append(difference); // 收集差值用于计算MAX
            } else {
                avgDifference = "无";
            }

            QStringList resultRow;
            resultRow << "负值循环"
                      << QString::number(fxValue, 'f', 1)
                      << (risingValues.isEmpty() ? "无" : QString::number(risingAvg, 'f', 4))
                      << (risingValues.isEmpty() ? "无" : QString::number(risingStdDev, 'f', 4))
                      << (risingValues.isEmpty() ? "无" : risingValuesList)
                      << (fallingValues.isEmpty() ? "无" : QString::number(fallingAvg, 'f', 4))
                      << (fallingValues.isEmpty() ? "无" : QString::number(fallingStdDev, 'f', 4))
                      << (fallingValues.isEmpty() ? "无" : fallingValuesList)
                      << avgDifference; // 正行程平均值-反行程平均值

            processedData.append(resultRow);
        }
    }

    // 计算标准偏差 - 使用用户提供的公式：SQRT(SUMSQ(标准差值)/COUNT(标准差个数))
    updateStatus("开始计算标准偏差...");

    // 显示收集到的标准差数据
    QString positiveStdListStr = "";
    if (!positiveStdDevs.isEmpty()) {
        QStringList positiveStdList;
        for (double val : positiveStdDevs) {
            positiveStdList.append(QString::number(val, 'f', 4));
        }
        positiveStdListStr = "[" + positiveStdList.join(", ") + "]";
    }

    QString negativeStdListStr = "";
    if (!negativeStdDevs.isEmpty()) {
        QStringList negativeStdList;
        for (double val : negativeStdDevs) {
            negativeStdList.append(QString::number(val, 'f', 4));
        }
        negativeStdListStr = "[" + negativeStdList.join(", ") + "]";
    }

    // 计算正值循环标准偏差
    double positiveStdDeviation = 0.0;
    QString positiveCalcProcess = "";
    if (!positiveStdDevs.isEmpty()) {
        double sumSquares = 0.0;
        for (double val : positiveStdDevs) {
            sumSquares += val * val; // 平方和
        }
        positiveStdDeviation = sqrt(sumSquares / positiveStdDevs.size());
        positiveCalcProcess = QString("SQRT(%1/%2) = %3")
                    .arg(sumSquares, 0, 'f', 6)
                    .arg(positiveStdDevs.size())
                    .arg(positiveStdDeviation, 0, 'f', 4);
    }

    // 计算负值循环标准偏差
    double negativeStdDeviation = 0.0;
    QString negativeCalcProcess = "";
    if (!negativeStdDevs.isEmpty()) {
        double sumSquares = 0.0;
        for (double val : negativeStdDevs) {
            sumSquares += val * val; // 平方和
        }
        negativeStdDeviation = sqrt(sumSquares / negativeStdDevs.size());
        negativeCalcProcess = QString("SQRT(%1/%2) = %3")
                    .arg(sumSquares, 0, 'f', 6)
                    .arg(negativeStdDevs.size())
                    .arg(negativeStdDeviation, 0, 'f', 4);
    }

    // 计算新的记录量：2 * MAX(正量程标准偏差, 反量程标准偏差) / 210
    double maxStdDeviation = qMax(positiveStdDeviation, negativeStdDeviation);
    double recordValue = 2.0 * maxStdDeviation / 210.0;
    QString recordCalcProcess = QString("2 * MAX(%1, %2) / 210 = 2 * %3 / 210 = %4")
                .arg(positiveStdDeviation, 0, 'f', 4)
                .arg(negativeStdDeviation, 0, 'f', 4)
                .arg(maxStdDeviation, 0, 'f', 4)
                .arg(recordValue, 0, 'f', 6);

    // 计算第二个记录量last：2 * 记录量 / 210
    double lastValue = 2.0 * recordValue / 210.0;
    QString lastCalcProcess = QString("2 * %1 / 210 = %2")
                .arg(recordValue, 0, 'f', 6)
                .arg(lastValue, 0, 'f', 8);

    // 计算第三个记录量：MAX(所有正行程平均值-反行程平均值) / 210
    double maxAvgDifference = 0.0;
    double thirdRecordValue = 0.0;
    QString thirdRecordCalcProcess = "";

    if (!allAvgDifferences.isEmpty()) {
        maxAvgDifference = *std::max_element(allAvgDifferences.begin(), allAvgDifferences.end());
        thirdRecordValue = maxAvgDifference / 210.0;

        // 生成差值列表字符串用于显示
        QStringList diffList;
        for (double diff : allAvgDifferences) {
            diffList.append(QString::number(diff, 'f', 4));
        }
        QString diffListStr = "[" + diffList.join(", ") + "]";

        thirdRecordCalcProcess = QString("绝对值差值列表: %1\nMAX = %2\n%2 / 210 = %3")
                    .arg(diffListStr)
                    .arg(maxAvgDifference, 0, 'f', 4)
                    .arg(thirdRecordValue, 0, 'f', 6);
    } else {
        thirdRecordCalcProcess = "无可用的差值数据";
    }

    // 添加标准偏差行到表格末尾
    QStringList positiveStdRow;
    positiveStdRow << "正循环标准偏差" << QString::number(positiveStdDeviation, 'f', 4) << "" << "" << "" << "" << "" << "" << "";
    processedData.append(positiveStdRow);

    QStringList negativeStdRow;
    negativeStdRow << "负循环标准偏差" << QString::number(negativeStdDeviation, 'f', 4) << "" << "" << "" << "" << "" << "" << "";
    processedData.append(negativeStdRow);

    // 添加记录量行
    QStringList recordRow;
    recordRow << "记录量" << QString::number(recordValue, 'f', 6) << "" << "" << "" << "" << "" << "" << "";
    processedData.append(recordRow);

    // 添加Last记录量行
    QStringList lastRecordRow;
    lastRecordRow << "Last记录量" << QString::number(lastValue, 'f', 8) << "" << "" << "" << "" << "" << "" << "";
    processedData.append(lastRecordRow);

    // 添加第三记录量行
    QStringList thirdRecordRow;
    thirdRecordRow << "第三记录量" << QString::number(thirdRecordValue, 'f', 6) << "" << "" << "" << "" << "" << "" << "";
    processedData.append(thirdRecordRow);

    // 将计算过程显示在统计信息中
    QString calcDetails = QString(
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
        "公式说明:\n"
        "- 标准偏差 = SQRT(SUMSQ(标准差值)/COUNT(标准差个数))\n"
        "- 记录量 = 2 * MAX(正量程标准偏差, 反量程标准偏差) / 210\n"
        "- Last记录量 = 2 * 记录量 / 210\n"
        "- 第三记录量 = MAX(|所有正行程平均值-反行程平均值|) / 210"
    ).arg(positiveStdListStr)
     .arg(positiveStdDevs.size())
     .arg(positiveCalcProcess)
     .arg(negativeStdListStr)
     .arg(negativeStdDevs.size())
     .arg(negativeCalcProcess)
     .arg(recordCalcProcess)
     .arg(recordValue, 0, 'f', 6)
     .arg(lastCalcProcess)
     .arg(lastValue, 0, 'f', 8)
     .arg(thirdRecordCalcProcess)
     .arg(thirdRecordValue, 0, 'f', 6);

    ui.textStatistics->setPlainText(calcDetails);

    updateStatus(QString("处理完成，正值循环 %1 个设定值，负值循环 %2 个设定值")
                .arg(positiveFxGroups.size()).arg(negativeFxGroups.size()));
}

void Data_Solve::processLinearityData()
{
    linearityData.clear();

    if (originalData.size() <= 1) {
        updateStatus("没有数据可处理线性度分析");
        return;
    }

    // 创建线性度表头
    QStringList linearityHeaders;
    linearityHeaders << "平均电压值" << "标准压力值" << "Y=ax+b" << "标准压力值-最小二乘法" << "数值列表";
    linearityData.append(linearityHeaders);

    updateStatus("开始处理线性度数据...");

    QStringList headers = originalData[0];

    // 找到所有相关列的索引
    int fxColumnIndex = -1;
    int ufxColumnIndex = -1;
    int fyColumnIndex = -1, fzColumnIndex = -1;
    int mxColumnIndex = -1, myColumnIndex = -1, mzColumnIndex = -1;

    for (int i = 0; i < headers.size(); ++i) {
        QString header = headers[i];
        if (header.compare("Fx", Qt::CaseInsensitive) == 0) {
            fxColumnIndex = i;
        } else if (header.compare("UFx", Qt::CaseInsensitive) == 0) {
            ufxColumnIndex = i;
        } else if (header.compare("Fy", Qt::CaseInsensitive) == 0) {
            fyColumnIndex = i;
        } else if (header.compare("Fz", Qt::CaseInsensitive) == 0) {
            fzColumnIndex = i;
        } else if (header.compare("Mx", Qt::CaseInsensitive) == 0) {
            mxColumnIndex = i;
        } else if (header.compare("My", Qt::CaseInsensitive) == 0) {
            myColumnIndex = i;
        } else if (header.compare("Mz", Qt::CaseInsensitive) == 0) {
            mzColumnIndex = i;
        }
    }

    if (fxColumnIndex == -1 || ufxColumnIndex == -1) {
        updateStatus("未找到Fx或UFx列，无法处理线性度数据");
        return;
    }

    // 定义数据点结构
    struct FxDataPoint {
        double fxValue;
        double ufxValue;
        int rowIndex;
    };

    // 第一步：收集纯Fx测试数据
    QVector<FxDataPoint> pureFxData;

    for (int i = 1; i < originalData.size(); ++i) {
        const QStringList& row = originalData[i];
        if (fxColumnIndex >= row.size() || ufxColumnIndex >= row.size()) continue;

        bool fxOk, ufxOk;
        double fxValue = row[fxColumnIndex].toDouble(&fxOk);
        double ufxValue = row[ufxColumnIndex].toDouble(&ufxOk);

        if (!fxOk || !ufxOk) continue;

        // 检查是否为纯Fx测试（其他分量应该接近0）
        bool isPureFxTest = true;
        QVector<int> otherColumns = {fyColumnIndex, fzColumnIndex, mxColumnIndex, myColumnIndex, mzColumnIndex};

        for (int colIndex : otherColumns) {
            if (colIndex != -1 && colIndex < row.size()) {
                bool ok;
                double value = row[colIndex].toDouble(&ok);
                if (ok && qAbs(value) > 0.1) { // 如果其他分量不为0
                    isPureFxTest = false;
                    break;
                }
            }
        }

        if (isPureFxTest) {
            FxDataPoint point;
            point.fxValue = fxValue;
            point.ufxValue = ufxValue;
            point.rowIndex = i;
            pureFxData.append(point);
        }
    }

    updateStatus(QString("收集到 %1 个纯Fx数据点").arg(pureFxData.size()));

    // 第二步：分别处理正值循环和负值循环
    QMap<double, QVector<FxDataPoint>> positiveFxGroups; // 正值循环
    QMap<double, QVector<FxDataPoint>> negativeFxGroups; // 负值循环

    for (int i = 0; i < pureFxData.size(); ++i) {
        const FxDataPoint& point = pureFxData[i];

        if (qAbs(point.fxValue) < 0.1) { // 是0点，需要根据邻近值判断
            // 检查相邻的点(i-1或i+1)
            bool foundPositive = false, foundNegative = false;

            // 检查前一个点
            if (i > 0 && qAbs(pureFxData[i-1].fxValue) > 0.1) {
                if (pureFxData[i-1].fxValue > 0) foundPositive = true;
                else foundNegative = true;
            }

            // 检查后一个点（如果前面没找到非0值）
            if (!foundPositive && !foundNegative && i < pureFxData.size() - 1 && qAbs(pureFxData[i+1].fxValue) > 0.1) {
                if (pureFxData[i+1].fxValue > 0) foundPositive = true;
                else foundNegative = true;
            }

            // 根据找到的非0值决定分组
            if (foundPositive) {
                positiveFxGroups[0].append(point);
            } else if (foundNegative) {
                negativeFxGroups[0].append(point);
            }
        } else if (point.fxValue > 0) {
            positiveFxGroups[point.fxValue].append(point);
        } else {
            negativeFxGroups[point.fxValue].append(point);
        }
    }

    // 第三步：分别处理正负循环数据，0点要分开作为不同的设定值
    QMap<QString, QVector<double>> allValues; // 存储每个设定值标识的原始数值
    QMap<QString, double> finalAverages; // 使用字符串标识来区分正负循环的0点
    QVector<double> allFxValues, allUfxAverages;

    // 第四步：处理正循环数据
    for (auto it = positiveFxGroups.begin(); it != positiveFxGroups.end(); ++it) {
        double fxValue = it.key();
        QVector<FxDataPoint> points = it.value();

        if (points.isEmpty()) continue;

        // 收集所有原始数值
        QVector<double> rawValues;
        for (const FxDataPoint& point : points) {
            rawValues.append(point.ufxValue);
        }

        // 计算平均值
        double sum = 0.0;
        for (double val : rawValues) {
            sum += val;
        }
        double average = sum / rawValues.size();

        // 创建唯一标识
        QString key;
        if (qAbs(fxValue) < 0.1) {
            key = "0(正)"; // 正循环的0点
        } else {
            key = QString::number(fxValue, 'f', 1);
        }

        allValues[key] = rawValues;
        finalAverages[key] = average;
        allFxValues.append(fxValue);
        allUfxAverages.append(average);
    }

    // 第五步：处理负循环数据
    for (auto it = negativeFxGroups.begin(); it != negativeFxGroups.end(); ++it) {
        double fxValue = it.key();
        QVector<FxDataPoint> points = it.value();

        if (points.isEmpty()) continue;

        // 收集所有原始数值
        QVector<double> rawValues;
        for (const FxDataPoint& point : points) {
            rawValues.append(point.ufxValue);
        }

        // 计算平均值
        double sum = 0.0;
        for (double val : rawValues) {
            sum += val;
        }
        double average = sum / rawValues.size();

        // 创建唯一标识
        QString key;
        if (qAbs(fxValue) < 0.1) {
            key = "0(负)"; // 负循环的0点
        } else {
            key = QString::number(fxValue, 'f', 1);
        }

        allValues[key] = rawValues;
        finalAverages[key] = average;
        allFxValues.append(fxValue);
        allUfxAverages.append(average);
    }

    // 第五步：先计算SUMPRODUCT和其他统计量
    double sumProduct = 0.0;              // 所有原始数值*对应Fx的乘积和
    QStringList sumProductDetailsList;    // SUMPRODUCT计算过程详情列表

    // 收集所有Fx值和对应的UFx值（用于最小二乘法计算）
    QVector<double> allFxForRegression;
    QVector<double> allUfxForRegression;

    // 从正循环收集数据并计算SUMPRODUCT
    for (auto it = positiveFxGroups.begin(); it != positiveFxGroups.end(); ++it) {
        double fxValue = it.key();
        QVector<FxDataPoint> points = it.value();
        for (const FxDataPoint& point : points) {
            allFxForRegression.append(fxValue);
            allUfxForRegression.append(point.ufxValue);

            double product = point.ufxValue * fxValue;
            sumProduct += product;
            sumProductDetailsList.append(QString("(%1 × %2)")
                                        .arg(QString::number(point.ufxValue, 'f', 4))
                                        .arg(QString::number(fxValue, 'f', 1)));
        }
    }

    // 从负循环收集数据并计算SUMPRODUCT
    for (auto it = negativeFxGroups.begin(); it != negativeFxGroups.end(); ++it) {
        double fxValue = it.key();
        QVector<FxDataPoint> points = it.value();
        for (const FxDataPoint& point : points) {
            allFxForRegression.append(fxValue);
            allUfxForRegression.append(point.ufxValue);

            double product = point.ufxValue * fxValue;
            sumProduct += product;
            sumProductDetailsList.append(QString("(%1 × %2)")
                                        .arg(QString::number(point.ufxValue, 'f', 4))
                                        .arg(QString::number(fxValue, 'f', 1)));
        }
    }

    // 第六步：使用您指定的公式计算最小二乘法参数
    double a = 0.0, b = 0.0;

    if (allFxForRegression.size() >= 2) {
        // 计算各项统计量
        int count_fx = allFxForRegression.size();                    // COUNT(所有Fx)
        double sum_fx = 0.0;                                         // SUM(所有Fx)
        double sum_ufx = 0.0;                                        // SUM(所有UFx)
        double sumsq_fx = 0.0;                                       // SUMSQ(所有Fx)

        for (int i = 0; i < allFxForRegression.size(); ++i) {
            sum_fx += allFxForRegression[i];
            sum_ufx += allUfxForRegression[i];
            sumsq_fx += allFxForRegression[i] * allFxForRegression[i];
        }

        // 计算UFx的平方和
        double sumsq_ufx = 0.0;                                     // SUMSQ(所有UFx)
        for (int i = 0; i < allUfxForRegression.size(); ++i) {
            sumsq_ufx += allUfxForRegression[i] * allUfxForRegression[i];
        }

        // 计算标准压力值总和（线性度表格总计行的数据）
        double totalStandardPressureSum = 0.0;
        QSet<double> uniqueFxValues;
        for (int i = 0; i < allFxForRegression.size(); ++i) {
            uniqueFxValues.insert(allFxForRegression[i]);
        }
        for (double fx : uniqueFxValues) {
            totalStandardPressureSum += fx;
        }

        // 使用修正后的公式计算a
        // a = (COUNT(UFx)*SUMPRODUCT(UFx,Fx) - SUM(UFx)*总计行数据) / (COUNT(UFx)*SUMSQ(UFx) - SUM(UFx)*SUM(UFx))
        double numerator = count_fx * sumProduct - sum_ufx * totalStandardPressureSum;
        double denominator = count_fx * sumsq_ufx - sum_ufx * sum_ufx;

        if (qAbs(denominator) > 0.0001) {
            a = numerator / denominator;

            // 使用新的b系数公式
            // b = (SUMSQ(UFx)*总计行数据 - SUM(UFx)*SUMPRODUCT) / (COUNT(UFx)*SUMSQ(UFx) - SUM(UFx)*SUM(UFx))
            double b_numerator = sumsq_ufx * totalStandardPressureSum - sum_ufx * sumProduct;
            double b_denominator = count_fx * sumsq_ufx - sum_ufx * sum_ufx;

            if (qAbs(b_denominator) > 0.0001) {
                b = b_numerator / b_denominator;
            }
        }

        // 计算b系数的中间值
        double b_numerator = sumsq_ufx * totalStandardPressureSum - sum_ufx * sumProduct;
        double b_denominator = count_fx * sumsq_ufx - sum_ufx * sum_ufx;

        // 存储计算过程详情
        QString regressionDetails = QString(
            "\n===== 最小二乘法计算详情 =====\n\n"
            "COUNT(所有UFx) = %1\n"
            "SUM(所有UFx) = %2\n"
            "SUMSQ(所有UFx) = %3\n"
            "SUMPRODUCT(UFx,Fx) = %4\n"
            "总计行数据(标准压力值总和) = %5\n\n"
            "计算a的公式:\n"
            "a = (COUNT(UFx)*SUMPRODUCT(UFx,Fx) - SUM(UFx)*总计行数据) / (COUNT(UFx)*SUMSQ(UFx) - SUM(UFx)*SUM(UFx))\n"
            "a = (%1 * %4 - %2 * %5) / (%1 * %3 - %2 * %2)\n"
            "a = (%6 - %7) / (%8 - %9)\n"
            "a = %10 / %11\n"
            "a = %12\n\n"
            "计算b的公式:\n"
            "b = (SUMSQ(UFx)*总计行数据 - SUM(UFx)*SUMPRODUCT) / (COUNT(UFx)*SUMSQ(UFx) - SUM(UFx)*SUM(UFx))\n"
            "b = (%3 * %5 - %2 * %4) / (%1 * %3 - %2 * %2)\n"
            "b = (%13 - %14) / (%8 - %9)\n"
            "b = %15 / %11\n"
            "b = %16\n"
        ).arg(count_fx)
         .arg(QString::number(sum_ufx, 'f', 4))
         .arg(QString::number(sumsq_ufx, 'f', 4))
         .arg(QString::number(sumProduct, 'f', 4))
         .arg(QString::number(totalStandardPressureSum, 'f', 4))
         .arg(QString::number(count_fx * sumProduct, 'f', 4))
         .arg(QString::number(sum_ufx * totalStandardPressureSum, 'f', 4))
         .arg(QString::number(count_fx * sumsq_ufx, 'f', 4))
         .arg(QString::number(sum_ufx * sum_ufx, 'f', 4))
         .arg(QString::number(numerator, 'f', 6))
         .arg(QString::number(denominator, 'f', 6))
         .arg(QString::number(a, 'f', 6))
         .arg(QString::number(sumsq_ufx * totalStandardPressureSum, 'f', 4))
         .arg(QString::number(sum_ufx * sumProduct, 'f', 4))
         .arg(QString::number(b_numerator, 'f', 6))
         .arg(QString::number(b, 'f', 6));

        // 将计算详情存储到后面使用
        QString currentStats = ui.textStatistics->toPlainText();
        ui.textStatistics->setPlainText(currentStats + regressionDetails);
    }

    // 第七步：生成最终结果
    double totalStandardPressure = 0.0;  // 标准压力值总和
    QVector<double> allDifferences;       // 收集所有的"标准压力值-最小二乘法"差值

    // 生成显示结果
    for (auto it = finalAverages.begin(); it != finalAverages.end(); ++it) {
        QString keyStr = it.key();
        double avgVoltage = it.value();

        // 解析标准压力值
        double standardPressure = 0.0;
        if (keyStr == "0(正)" || keyStr == "0(负)") {
            standardPressure = 0.0;
        } else {
            standardPressure = keyStr.toDouble();
        }

        // 累加标准压力值总和（只累加一次每个设定值）
        if (!keyStr.contains("(")) { // 不是0(正)或0(负)的情况
            totalStandardPressure += standardPressure;
        } else if (keyStr == "0(正)") { // 只对0(正)累加一次0值
            totalStandardPressure += standardPressure;
        }

        // Y=ax+b（使用最小二乘法计算的理论值）
        // 这里x是平均电压值(avgVoltage)，y是标准压力值
        double theoreticalValue = a * avgVoltage + b;

        // 标准压力值-最小二乘法（差值）
        double difference = standardPressure - theoreticalValue;
        allDifferences.append(difference);  // 收集差值用于后续计算

        // 生成数值列表字符串（按照重复性误差的格式）
        QString valuesList = "";
        if (allValues.contains(keyStr)) {
            QVector<double> values = allValues[keyStr];
            if (!values.isEmpty()) {
                QStringList valuesStrList;
                for (double val : values) {
                    valuesStrList.append(QString::number(val, 'f', 4));
                }
                valuesList = "[" + valuesStrList.join(", ") + "]";
            }
        }

        QStringList resultRow;
        resultRow << QString::number(avgVoltage, 'f', 4)
                  << keyStr  // 显示带有正负循环标识的标准压力值
                  << QString::number(theoreticalValue, 'f', 4)
                  << QString::number(difference, 'f', 4)
                  << (valuesList.isEmpty() ? "无" : valuesList);

        linearityData.append(resultRow);
    }

    // 计算MAX(ABS(所有标准压力值-最小二乘法))/210
    double maxAbsDifference = 0.0;
    if (!allDifferences.isEmpty()) {
        for (double diff : allDifferences) {
            double absDiff = qAbs(diff);
            if (absDiff > maxAbsDifference) {
                maxAbsDifference = absDiff;
            }
        }
    }
    double maxDifferenceResult = maxAbsDifference / 210.0;

    // 添加总计行
    QStringList totalRow1;
    totalRow1 << "总计"
              << QString::number(totalStandardPressure, 'f', 4)
              << ""
              << ""
              << "";
    linearityData.append(totalRow1);

    // 添加乘积和行
    QStringList totalRow2;
    totalRow2 << "SUMPRODUCT"
              << QString::number(sumProduct, 'f', 4)
              << ""
              << ""
              << "";
    linearityData.append(totalRow2);

    // 添加MAX(ABS(差值))/210行
    QStringList totalRow3;
    totalRow3 << "MAX(ABS(差值))/210"
              << QString::number(maxAbsDifference, 'f', 4)
              << ""
              << QString::number(maxDifferenceResult, 'f', 6)
              << "";
    linearityData.append(totalRow3);

    // 输出计算过程详情到状态信息
    QString sumProductDetailsStr = sumProductDetailsList.join(" + ");

    // 生成差值列表字符串用于显示
    QStringList diffList;
    for (double diff : allDifferences) {
        diffList.append(QString::number(diff, 'f', 4));
    }
    QString diffListStr = "[" + diffList.join(", ") + "]";

    QString calculationDetails = QString(
        "\n===== 线性度计算详情 =====\n\n"
        "标准压力值总和: %1\n\n"
        "SUMPRODUCT计算过程:\n"
        "每个原始测量值(UFx) × 对应的设定值(Fx)：\n%2\n\n"
        "SUMPRODUCT总和: %3\n\n"
        "最小二乘法拟合直线: Y = %4x + %5\n\n"
        "===== 线性度记录量计算 =====\n"
        "所有差值列表(标准压力值-最小二乘法): %6\n"
        "MAX(ABS(差值)) = %7\n"
        "MAX(ABS(差值))/210 = %7/210 = %8\n"
    ).arg(QString::number(totalStandardPressure, 'f', 4))
     .arg(sumProductDetailsStr)
     .arg(QString::number(sumProduct, 'f', 4))
     .arg(QString::number(a, 'f', 6))
     .arg(QString::number(b, 'f', 6))
     .arg(diffListStr)
     .arg(QString::number(maxAbsDifference, 'f', 4))
     .arg(QString::number(maxDifferenceResult, 'f', 6));

    // 直接更新统计信息显示
    QString currentStats = ui.textStatistics->toPlainText();
    ui.textStatistics->setPlainText(currentStats + calculationDetails);

    updateStatus(QString("线性度处理完成，正循环 %1 个设定值，负循环 %2 个设定值，拟合直线: Y = %3x + %4")
                .arg(positiveFxGroups.size())
                .arg(negativeFxGroups.size())
                .arg(QString::number(a, 'f', 6))
                .arg(QString::number(b, 'f', 6)));
}

void Data_Solve::calculateStatistics()
{
    QString stats;
    QTextStream stream(&stats);

    stream << "===== 数据统计信息 =====\n\n";
    stream << QString("文件路径: %1\n").arg(currentFilePath);
    stream << QString("加载时间: %1\n\n").arg(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (!originalData.isEmpty()) {
        stream << QString("总行数: %1\n").arg(originalData.size());
        if (originalData.size() > 0) {
            stream << QString("总列数: %1\n\n").arg(originalData[0].size());

            stream << "列信息:\n";
            for (int i = 0; i < originalData[0].size(); ++i) {
                stream << QString("  列 %1: %2\n").arg(i + 1).arg(originalData[0][i]);

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

                stream << QString("    - 非空值数量: %1\n").arg(nonEmptyCount);
                stream << QString("    - 唯一值数量: %1\n").arg(uniqueValues.size());

                if (uniqueValues.size() <= 10) {
                    stream << "    - 唯一值列表: ";
                    QStringList values = uniqueValues.keys();
                    stream << values.join(", ") << "\n";
                }
                stream << "\n";
            }
        }
    }

    // 线性度计算详情已直接在processLinearityData中添加到统计信息中

    ui.textStatistics->setPlainText(stats);
}

void Data_Solve::onExportDataClicked()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出数据",
        QDir::currentPath(),
        "CSV Files (*.csv);;Excel Files (*.xlsx);;All Files (*)");

    if (!fileName.isEmpty()) {
        bool success = false;

        if (fileName.endsWith(".csv")) {
            success = exportToCSV(fileName);
        } else if (fileName.endsWith(".xlsx")) {
            success = exportToExcel(fileName);
        } else {
            fileName += ".csv";
            success = exportToCSV(fileName);
        }

        if (success) {
            showInfo("导出成功", QString("数据已成功导出到:\n%1").arg(fileName));
        } else {
            showError("导出失败", "无法导出数据，请检查文件路径和权限");
        }
    }
}

bool Data_Solve::exportToCSV(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return false;
    }

    QTextStream stream(&file);
    stream.setCodec("UTF-8");

    const QVector<QStringList>& dataToExport = processedData.isEmpty() ? originalData : processedData;

    for (const QStringList& row : dataToExport) {
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

bool Data_Solve::exportToExcel(const QString& filePath)
{
    QString csvPath = QDir::temp().filePath("temp_export.csv");
    if (!exportToCSV(csvPath)) {
        return false;
    }

    QString pythonScript = QString(
        "import sys\n"
        "try:\n"
        "    import pandas as pd\n"
        "    df = pd.read_csv('%1', encoding='utf-8')\n"
        "    df.to_excel('%2', index=False)\n"
        "    print('Success')\n"
        "except Exception as e:\n"
        "    print('Error: ' + str(e))\n"
    ).arg(csvPath).arg(filePath);

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
        QFile::remove(csvPath);

        return output.contains("Success");
    }

    QFile::remove(csvPath);
    return false;
}

void Data_Solve::onClearDataClicked()
{
    if (!originalData.isEmpty() || !processedData.isEmpty()) {
        int ret = QMessageBox::question(this, "确认清空",
            "确定要清空所有数据吗？",
            QMessageBox::Yes | QMessageBox::No);

        if (ret == QMessageBox::Yes) {
            clearAllData();
            updateStatus("数据已清空");
        }
    }
}

void Data_Solve::clearAllData()
{
    originalData.clear();
    processedData.clear();
    linearityData.clear();
    currentFilePath.clear();

    ui.tableOriginalData->clear();
    ui.tableOriginalData->setRowCount(0);
    ui.tableOriginalData->setColumnCount(0);

    ui.tableProcessedData->clear();
    ui.tableProcessedData->setRowCount(0);
    ui.tableProcessedData->setColumnCount(0);

    ui.tableLinearityData->clear();
    ui.tableLinearityData->setRowCount(0);
    ui.tableLinearityData->setColumnCount(0);

    ui.textStatistics->clear();

    ui.btnProcessData->setEnabled(false);
    ui.btnExportData->setEnabled(false);
}

void Data_Solve::onActionOpenTriggered()
{
    onLoadDataClicked();
}

void Data_Solve::onActionSaveTriggered()
{
    onExportDataClicked();
}

void Data_Solve::onActionExitTriggered()
{
    close();
}

void Data_Solve::onActionAboutTriggered()
{
    QMessageBox::about(this, "关于",
        "数据处理工具 v1.0\n\n"
        "这是一个用于处理Excel和CSV数据的工具\n"
        "支持数据加载、处理、统计和导出功能\n\n"
        "使用Qt 5.15.2开发");
}

void Data_Solve::updateStatus(const QString& message)
{
    ui.lblStatus->setText(message);
    ui.statusBar->showMessage(message, 3000);
}

void Data_Solve::showError(const QString& title, const QString& message)
{
    QMessageBox::critical(this, title, message);
}

void Data_Solve::showInfo(const QString& title, const QString& message)
{
    QMessageBox::information(this, title, message);
}