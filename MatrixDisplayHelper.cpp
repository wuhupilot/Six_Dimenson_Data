#include "MatrixDisplayHelper.h"
#include <QTableWidgetItem>

#pragma execution_character_set("utf-8")

// 静态常量定义
const QStringList MatrixDisplayHelper::ROW_HEADERS = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
const QStringList MatrixDisplayHelper::COL_HEADERS = {"UFx", "UFy", "UFz", "UMx", "UMy", "UMz"};

MatrixDisplayHelper::MatrixDisplayHelper()
{
}

MatrixDisplayHelper::~MatrixDisplayHelper()
{
}

void MatrixDisplayHelper::displayDecouplingResult(
    const DecouplingResult& result,
    QTableWidget* tableDecoupling,
    QTableWidget* tableSecond,
    QTableWidget* tableThird,
    QTableWidget* tableDecoupled,
    QTableWidget* tableError,
    QTableWidget* tableNormalized,
    QTableWidget* tableStatistics)
{
    // 初始化所有表格
    initializeMatrixTable(tableDecoupling);
    initializeMatrixTable(tableSecond);
    initializeMatrixTable(tableThird);

    // 设置表头
    setupMatrixTableHeaders(tableDecoupling);
    setupMatrixTableHeaders(tableSecond);
    setupMatrixTableHeaders(tableThird);

    if (result.success) {
        // 填充三个矩阵
        fillMatrixTable(tableDecoupling, result.matrix);
        fillMatrixTable(tableSecond, result.secondMatrix);
        fillMatrixTable(tableThird, result.thirdMatrix);

        // 调整列宽
        tableDecoupling->resizeColumnsToContents();
        tableSecond->resizeColumnsToContents();
        tableThird->resizeColumnsToContents();

        // 显示其他数据表格
        displayDataInTable(tableDecoupled, result.decoupledData);
        displayDataInTable(tableError, result.errorData);
        displayDataInTable(tableNormalized, result.normalizedSquaredError);
        displayDataInTable(tableStatistics, result.channelStatistics);
    } else {
        // 显示错误信息
        displayErrorInTable(tableDecoupling, result.errorMessage);
        displayErrorInTable(tableSecond, result.errorMessage);
        displayErrorInTable(tableThird, result.errorMessage);
    }
}

QString MatrixDisplayHelper::generateMatrixInfoText(const DecouplingResult& result)
{
    QString matrixInfo = QString::fromUtf8("\n\n");
    matrixInfo += QString::fromUtf8("============================================================\n");
    matrixInfo += QString::fromUtf8("解耦矩阵计算说明\n");
    matrixInfo += QString::fromUtf8("============================================================\n\n");

    if (result.success) {
        matrixInfo += QString::fromUtf8("✓ 矩阵计算成功\n\n");

        matrixInfo += QString::fromUtf8("【第一矩阵 - 解耦矩阵】\n");
        matrixInfo += QString::fromUtf8("公式: D = U^T × F × (F^T × F)^(-1)\n");
        matrixInfo += QString::fromUtf8("说明: 将测量电压值转换为真实力/力矩值\n");
        matrixInfo += QString::fromUtf8("应用: 真实力 = 第一矩阵 × 测量电压\n\n");

        matrixInfo += QString::fromUtf8("【第二矩阵】\n");
        matrixInfo += QString::fromUtf8("公式: (D^T × D)^(-1) × D^T\n");
        matrixInfo += QString::fromUtf8("说明: Moore-Penrose伪逆矩阵\n");
        matrixInfo += QString::fromUtf8("应用: 用于反向校准和误差分析\n\n");

        matrixInfo += QString::fromUtf8("【第三矩阵】\n");
        matrixInfo += QString::fromUtf8("公式: F^T × U × (U^T × U)^(-1)\n");
        matrixInfo += QString::fromUtf8("说明: 从设定力值计算期望测量值\n");
        matrixInfo += QString::fromUtf8("应用: 期望电压 = 第三矩阵^(-1) × 设定力\n\n");

        matrixInfo += QString::fromUtf8("计算步骤:\n");
        matrixInfo += QString::fromUtf8("1. 从原始数据中提取U矩阵(测量值)和F矩阵(设定值)\n");
        matrixInfo += QString::fromUtf8("2. 计算各矩阵的转置 (U^T, F^T)\n");
        matrixInfo += QString::fromUtf8("3. 进行矩阵乘法运算\n");
        matrixInfo += QString::fromUtf8("4. 使用高斯-若尔当消元法计算逆矩阵\n");
        matrixInfo += QString::fromUtf8("5. 完成最终的矩阵乘法得到三个解耦矩阵\n\n");

        matrixInfo += QString::fromUtf8("注: 详细的矩阵数值请查看【解耦矩阵】标签页\n\n");

        // 量程信息
        matrixInfo += QString::fromUtf8("============================================================\n");
        matrixInfo += QString::fromUtf8("各通道量程 (用于误差归一化)\n");
        matrixInfo += QString::fromUtf8("============================================================\n\n");

        QStringList channelNames = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
        for (int i = 0; i < 6; ++i) {
            matrixInfo += QString::fromUtf8("%1 量程: %2\n")
                .arg(channelNames[i])
                .arg(result.channelRanges[i], 0, 'f', 8);
        }

        matrixInfo += QString::fromUtf8("\n说明: 使用手动设置的量程值（可在界面上方的【各通道量程设置】区域修改）\n\n");

        // 各通道纯通道测试数据行数统计
        matrixInfo += QString::fromUtf8("============================================================\n");
        matrixInfo += QString::fromUtf8("各通道纯通道测试数据行数统计\n");
        matrixInfo += QString::fromUtf8("============================================================\n\n");

        for (int i = 0; i < 6; ++i) {
            matrixInfo += QString::fromUtf8("%1 通道: %2 行\n")
                .arg(channelNames[i])
                .arg(result.channelCounts[i]);
        }

        matrixInfo += QString::fromUtf8("\n说明: 统计归一化平方误差数据中各通道的纯通道测试数据行数\n\n");

        // 误差计算过程说明
        matrixInfo += QString::fromUtf8("============================================================\n");
        matrixInfo += QString::fromUtf8("误差分析计算过程\n");
        matrixInfo += QString::fromUtf8("============================================================\n\n");

        matrixInfo += QString::fromUtf8("【步骤1: 计算原始误差】\n");
        matrixInfo += QString::fromUtf8("  公式: 误差 = 设定值 - 解耦后值\n");
        matrixInfo += QString::fromUtf8("  例如: Fx误差 = Fx - Fx'\n");
        matrixInfo += QString::fromUtf8("  结果: 查看【误差分析(原始)】标签页\n\n");

        matrixInfo += QString::fromUtf8("【步骤2: 归一化平方误差】\n");
        matrixInfo += QString::fromUtf8("  公式: 归一化平方误差 = ((误差 / 量程)²)\n");
        matrixInfo += QString::fromUtf8("  目的: 消除量纲影响，使各通道误差可比\n");
        matrixInfo += QString::fromUtf8("  结果: 查看【归一化平方误差】标签页\n\n");

        matrixInfo += QString::fromUtf8("【后续步骤】\n");
        matrixInfo += QString::fromUtf8("  - I类误差: 基于归一化平方误差计算\n");
        matrixInfo += QString::fromUtf8("  - II类误差: 基于归一化平方误差计算\n");
    } else {
        matrixInfo += QString::fromUtf8("✗ 矩阵计算失败\n\n");
        matrixInfo += QString::fromUtf8("错误信息: ") + result.errorMessage + "\n\n";
        matrixInfo += QString::fromUtf8("可能原因:\n");
        matrixInfo += QString::fromUtf8("- 数据不足（至少需要6行有效数据）\n");
        matrixInfo += QString::fromUtf8("- 缺少必需的通道列（Fx-Mz, UFx-UMz）\n");
        matrixInfo += QString::fromUtf8("- 矩阵奇异，无法求逆\n");
    }

    return matrixInfo;
}

void MatrixDisplayHelper::displayDataInTable(QTableWidget* table, const QVector<QStringList>& data)
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

void MatrixDisplayHelper::fillMatrixTable(QTableWidget* table, const double matrix[6][6])
{
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            QTableWidgetItem* item = new QTableWidgetItem(
                QString::number(matrix[i][j], 'f', 8)
            );
            item->setTextAlignment(Qt::AlignCenter);
            table->setItem(i, j, item);
        }
    }
}

void MatrixDisplayHelper::setupMatrixTableHeaders(QTableWidget* table)
{
    table->setHorizontalHeaderLabels(COL_HEADERS);
    table->setVerticalHeaderLabels(ROW_HEADERS);
}

void MatrixDisplayHelper::displayErrorInTable(QTableWidget* table, const QString& errorMessage)
{
    QTableWidgetItem* errorItem = new QTableWidgetItem(
        QString::fromUtf8("计算失败: ") + errorMessage
    );
    errorItem->setTextAlignment(Qt::AlignCenter);
    table->setItem(0, 0, errorItem);
    table->setSpan(0, 0, 6, 6);
}

void MatrixDisplayHelper::initializeMatrixTable(QTableWidget* table)
{
    table->clear();
    table->setRowCount(6);
    table->setColumnCount(6);
}
