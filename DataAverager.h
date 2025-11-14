#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

/**
 * @brief 数据平均处理类
 *
 * 提供数据分组平均功能：
 * - 按指定行数（默认5行）进行分组
 * - 计算每组的平均值
 * - 保留表头
 * - 支持数值列自动识别
 */
class DataAverager
{
public:
    DataAverager();
    ~DataAverager();

    /**
     * @brief 对数据进行分组平均
     * @param originalData 原始数据（第一行为表头）
     * @param groupSize 每组行数（默认5）
     * @return 平均后的数据
     */
    QVector<QStringList> averageData(
        const QVector<QStringList>& originalData,
        int groupSize = 5
    );

    /**
     * @brief 获取最后的错误信息
     */
    QString getLastError() const { return m_lastError; }

    /**
     * @brief 获取处理统计信息
     */
    QString getProcessInfo() const { return m_processInfo; }

private:
    /**
     * @brief 判断字符串是否为数值
     */
    bool isNumeric(const QString& str) const;

    /**
     * @brief 计算一组数据的平均值
     * @param values 字符串数值列表
     * @return 平均值的字符串表示
     */
    QString calculateAverage(const QVector<QString>& values) const;

    /**
     * @brief 处理单个分组
     * @param group 分组数据（不含表头）
     * @param headers 表头
     * @return 平均后的单行数据
     */
    QStringList processGroup(
        const QVector<QStringList>& group,
        const QStringList& headers
    ) const;

private:
    QString m_lastError;
    QString m_processInfo;
};
