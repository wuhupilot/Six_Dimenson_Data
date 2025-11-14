 #pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief 数据置零校准类
 *
 * 功能：找到零点（Fx=Fy=Fz=Mx=My=Mz=0的行），将每个测量循环组的第一个零点的U值作为基准，
 * 该组内所有数据的U值都减去这个基准零点的U值
 *
 * 处理逻辑：
 * 1. 找到所有零点行（Fx-Mz都≈0）
 * 2. 将数据分组：每4个零点为一个测量循环组（3次重复测量）
 * 3. 对每个测量循环组：
 *    - 使用第1个零点的U值作为基准
 *    - 该零点到第4个零点之间的所有数据行都减去基准U值
 *    - UFx' = UFx - UFx(第1个零点), UFy' = UFy - UFy(第1个零点), ...
 *
 * 示例：
 * 零点索引: [1, 20, 40, 60, 80, 100, 120, 140, ...]
 * 第1组(索引1-60): 以第1个零点(索引1)为基准
 * 第2组(索引80-140): 以第5个零点(索引80)为基准
 */
class DataZeroCalibrator
{
public:
    DataZeroCalibrator();
    ~DataZeroCalibrator();

    /**
     * @brief 执行置零校准
     * @param originalData 原始数据（第一行为表头）
     * @param zeroThreshold 零点判断阈值（默认0.1）
     * @return 校准后的数据
     */
    QVector<QStringList> calibrateZeroPoints(
        const QVector<QStringList>& originalData,
        double zeroThreshold = 0.1
    );

    /**
     * @brief 获取最后的错误信息
     */
    QString getLastError() const { return m_lastError; }

    /**
     * @brief 获取处理详细信息
     */
    QString getProcessInfo() const { return m_processInfo; }

private:
    /**
     * @brief 判断某行是否为零点（Fx-Mz都≈0）
     */
    bool isZeroPoint(
        const QStringList& row,
        const QStringList& headers,
        double threshold
    ) const;

    /**
     * @brief 查找所有零点的行索引（从1开始，0是表头）
     */
    QVector<int> findZeroPoints(
        const QVector<QStringList>& data,
        const QStringList& headers,
        double threshold
    ) const;

    /**
     * @brief 将零点分组（每4个零点为一组）
     */
    QVector<QVector<int>> groupZeroPoints(const QVector<int>& zeroPointIndices) const;

    /**
     * @brief 获取列索引
     */
    int getColumnIndex(const QStringList& headers, const QString& columnName) const;

    /**
     * @brief 判断字符串是否为数值
     */
    bool isNumeric(const QString& str) const;

    /**
     * @brief 判断数值是否接近零
     */
    bool isNearZero(double value, double threshold) const;

    /**
     * @brief 对指定行应用零点校准
     */
    QStringList applyZeroCalibration(
        const QStringList& row,
        const QStringList& zeroPointRow,
        const QStringList& headers
    ) const;

private:
    QString m_lastError;
    QString m_processInfo;

    // U值列名
    static const QStringList U_COLUMN_NAMES;
    // F值列名（用于判断零点）
    static const QStringList F_COLUMN_NAMES;
};
