#pragma once

#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTextEdit>
#include "DataProcessor.h"

/**
 * @brief 矩阵显示助手类
 *
 * 负责处理解耦矩阵的UI显示逻辑：
 * - 填充矩阵表格
 * - 显示解耦后数据
 * - 显示误差分析数据
 * - 生成矩阵说明文本
 */
class MatrixDisplayHelper
{
public:
    MatrixDisplayHelper();
    ~MatrixDisplayHelper();

    /**
     * @brief 显示解耦矩阵计算结果到UI
     * @param result 解耦矩阵计算结果
     * @param tableDecoupling 第一矩阵表格控件
     * @param tableSecond 第二矩阵表格控件
     * @param tableThird 第三矩阵表格控件
     * @param tableDecoupled 解耦后数据表格控件
     * @param tableError 误差分析表格控件
     * @param tableNormalized 归一化误差表格控件
     * @param tableStatistics 通道统计表格控件
     */
    void displayDecouplingResult(
        const DecouplingResult& result,
        QTableWidget* tableDecoupling,
        QTableWidget* tableSecond,
        QTableWidget* tableThird,
        QTableWidget* tableDecoupled,
        QTableWidget* tableError,
        QTableWidget* tableNormalized,
        QTableWidget* tableStatistics
    );

    /**
     * @brief 生成矩阵计算说明文本（追加到统计信息）
     * @param result 解耦矩阵计算结果
     * @return 格式化的说明文本
     */
    QString generateMatrixInfoText(const DecouplingResult& result);

    /**
     * @brief 在表格中显示数据
     * @param table 目标表格控件
     * @param data 数据（第一行为表头）
     */
    void displayDataInTable(QTableWidget* table, const QVector<QStringList>& data);

private:
    /**
     * @brief 填充单个矩阵到表格
     */
    void fillMatrixTable(QTableWidget* table, const double matrix[6][6]);

    /**
     * @brief 设置矩阵表格的表头
     */
    void setupMatrixTableHeaders(QTableWidget* table);

    /**
     * @brief 在表格中显示错误信息
     */
    void displayErrorInTable(QTableWidget* table, const QString& errorMessage);

    /**
     * @brief 初始化矩阵表格
     */
    void initializeMatrixTable(QTableWidget* table);

private:
    // 常量
    static const QStringList ROW_HEADERS;
    static const QStringList COL_HEADERS;
};
