#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include "DataProcessor.h"
#include "DataExporter.h"

/**
 * @brief 导出管理器类
 *
 * 负责管理数据导出逻辑，包括：
 * - 单通道导出
 * - 全通道批量导出
 * - 性能指标汇总表生成
 * - 解耦矩阵和误差分析数据导出
 */
class ExportManager
{
public:
    ExportManager(DataProcessor* processor, DataExporter* exporter);
    ~ExportManager();

    /**
     * @brief 导出当前已处理的单通道数据
     * @param fileName 导出文件路径
     * @param processedData 重复性误差数据
     * @param linearityData 线性度数据
     * @return 是否导出成功
     */
    bool exportCurrentChannel(
        const QString& fileName,
        const QVector<QStringList>& processedData,
        const QVector<QStringList>& linearityData
    );

    /**
     * @brief 导出所有6个通道的完整数据
     * @param fileName 导出文件路径
     * @param originalData 原始数据
     * @return 是否导出成功
     */
    bool exportAllChannels(
        const QString& fileName,
        const QVector<QStringList>& originalData
    );

    /**
     * @brief 获取最后的错误信息
     */
    QString getLastError() const { return m_lastError; }

private:
    /**
     * @brief 生成性能指标汇总表
     */
    void appendPerformanceTable(
        QVector<QStringList>& output,
        const QMap<QString, ProcessResult>& channelResults,
        const DecouplingResult& decouplingResult
    );

    /**
     * @brief 添加各通道详细数据
     */
    void appendChannelDetails(
        QVector<QStringList>& output,
        const QMap<QString, ProcessResult>& channelResults
    );

    /**
     * @brief 添加解耦矩阵和误差分析数据
     */
    void appendMatrixAndErrorAnalysis(
        QVector<QStringList>& output,
        const DecouplingResult& decouplingResult
    );

    /**
     * @brief 添加单个矩阵到输出
     */
    void appendMatrixSection(
        QVector<QStringList>& output,
        const QString& title,
        const double matrix[6][6]
    );

    /**
     * @brief 添加数据表格到输出（带标题）
     */
    void appendDataSection(
        QVector<QStringList>& output,
        const QString& title,
        const QVector<QStringList>& data
    );

    /**
     * @brief 创建空行（9列）
     */
    QStringList createEmptyRow(int columnCount = 9);

    /**
     * @brief 合并重复性和线性度数据
     */
    QVector<QStringList> mergeProcessedAndLinearityData(
        const QVector<QStringList>& processedData,
        const QVector<QStringList>& linearityData
    );

private:
    DataProcessor* m_dataProcessor;
    DataExporter* m_dataExporter;
    QString m_lastError;

    // 常量
    static const QStringList CHANNEL_NAMES;
    static const QStringList ROW_LABELS;
    static const QStringList COL_LABELS;
};
