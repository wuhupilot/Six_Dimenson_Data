#pragma once

#include <QString>
#include <QStringList>
#include <QVector>
#include <QMap>
#include <QSet>

struct ChannelDataPoint {
    double setValue;      // 设定值 (原fxValue)
    double measuredValue; // 测量值 (原ufxValue)
    int rowIndex;
    QString edgeType;
};

struct ProcessResult {
    QVector<QStringList> processedData;
    QVector<QStringList> linearityData;
    QString calculationDetails;
    QString lastError;

    // 关键指标
    double repeatability;  // 重复性R = 记录量 * 100
    double hysteresis;     // 迟滞性 = 第三记录量 * 100
    double linearity;      // 线性度 = MAX(ABS(差值))/量程 * 100
};

// 解耦矩阵结果
struct DecouplingResult {
    double matrix[6][6];        // 6x6 第一矩阵（解耦矩阵）
    double secondMatrix[6][6];  // 6x6 第二矩阵
    double thirdMatrix[6][6];   // 6x6 第三矩阵
    QVector<QStringList> decoupledData;     // 解耦后的数据 (Fx', Fy', Fz', Mx', My', Mz')
    QVector<QStringList> errorData;         // 原始误差数据 (Fx-Fx', Fy-Fy', ...)
    QVector<QStringList> normalizedSquaredError;  // 归一化平方误差 ((Fx-Fx')/量程)^2
    QVector<QStringList> channelStatistics;       // 各通道统计值表格数据
    double channelRanges[6];    // 各通道检测到的量程 [Fx, Fy, Fz, Mx, My, Mz]
    int channelCounts[6];       // 各通道的纯通道测试数据行数 [Fx, Fy, Fz, Mx, My, Mz]
    bool success;
    QString errorMessage;
};

// 保留旧结构体以兼容（可选）
typedef ChannelDataPoint FxDataPoint;

class DataProcessor
{
public:
    DataProcessor();
    ~DataProcessor();

    ProcessResult processData(const QVector<QStringList>& originalData);

    // 处理指定通道的数据
    ProcessResult processChannelData(const QVector<QStringList>& originalData, const QString& channelName);

    // 设置量程参数
    void setRangeParameters(double maxRange, double zeroThreshold = -1.0);

    // 设置各通道独立量程（新增）
    void setChannelRange(const QString& channelName, double range);
    void setAllChannelRanges(double fxRange, double fyRange, double fzRange,
                            double mxRange, double myRange, double mzRange);
    double getChannelRange(const QString& channelName) const;

    // 重置为默认量程
    void resetToDefaultRanges();

    // 自动检测量程参数
    void autoDetectRangeParameters(const QVector<QStringList>& originalData);
    void autoDetectRangeParameters(const QVector<QStringList>& originalData, const QString& channelName);

    // 计算解耦矩阵
    DecouplingResult calculateDecouplingMatrix(const QVector<QStringList>& originalData);

private:
    void processRepeatabilityError(
        const QVector<QStringList>& originalData,
        QVector<QStringList>& processedData,
        QString& calculationDetails,
        const QString& channelName,
        const QVector<QStringList>& decoupledData);

    void processLinearityData(
        const QVector<QStringList>& originalData,
        QVector<QStringList>& linearityData,
        QString& calculationDetails,
        const QString& channelName,
        const QVector<QStringList>& decoupledData);

    QVector<ChannelDataPoint> collectPureChannelData(
        const QVector<QStringList>& originalData,
        const QString& channelName,
        const QVector<QStringList>& decoupledData);

    // 保留旧方法名以兼容
    QVector<FxDataPoint> collectPureFxData(const QVector<QStringList>& originalData);

    QVector<QString> getOtherChannels(const QString& currentChannel);
    QVector<QString> getChannelsToCheckZero(const QString& currentChannel);
    int findColumnIndex(const QStringList& headers, const QString& columnName);
    double calculateStandardDeviation(const QVector<double>& values, double mean);
    QString generateValuesList(const QVector<double>& values);

private:
    // 量程参数
    double m_maxRange;          // 最大量程值（用于计算记录量的除数，已废弃，保留兼容）
    double m_zeroThreshold;     // 零点判断阈值
    bool m_autoDetected;        // 是否自动检测了参数

    // 各通道独立量程（新增）
    QMap<QString, double> m_channelRanges;  // 各通道量程映射表
    bool m_useManualRanges;                 // 是否使用手动设置的量程
};