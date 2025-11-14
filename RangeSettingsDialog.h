#pragma once

#include <QDialog>
#include <QDoubleSpinBox>
#include <QMap>
#include <QString>
#include "DataProcessor.h"

/**
 * @brief 量程设置对话框
 *
 * 提供友好的UI界面用于设置各通道的量程值：
 * - 支持6个通道独立设置（Fx, Fy, Fz, Mx, My, Mz）
 * - 提供恢复默认值功能
 * - 实时读取和应用DataProcessor中的量程
 */
class RangeSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param dataProcessor 数据处理器（读取和写入量程）
     * @param parent 父窗口
     */
    explicit RangeSettingsDialog(DataProcessor* dataProcessor, QWidget* parent = nullptr);
    ~RangeSettingsDialog();

    /**
     * @brief 获取对话框是否被接受（用户点击应用）
     */
    bool wasAccepted() const { return m_wasAccepted; }

    /**
     * @brief 获取各通道的新量程值
     */
    QMap<QString, double> getRangeValues() const;

private slots:
    void onApplyClicked();
    void onResetClicked();
    void onCancelClicked();

private:
    void setupUI();
    void loadCurrentRanges();
    void applyRangesToProcessor();

private:
    DataProcessor* m_dataProcessor;
    bool m_wasAccepted;

    // 输入控件
    QDoubleSpinBox* m_spinFx;
    QDoubleSpinBox* m_spinFy;
    QDoubleSpinBox* m_spinFz;
    QDoubleSpinBox* m_spinMx;
    QDoubleSpinBox* m_spinMy;
    QDoubleSpinBox* m_spinMz;

    // 默认量程值
    static const double DEFAULT_FX_RANGE;
    static const double DEFAULT_FY_RANGE;
    static const double DEFAULT_FZ_RANGE;
    static const double DEFAULT_MX_RANGE;
    static const double DEFAULT_MY_RANGE;
    static const double DEFAULT_MZ_RANGE;
};
