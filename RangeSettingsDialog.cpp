#include "RangeSettingsDialog.h"
#include <QVBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QDialogButtonBox>

#pragma execution_character_set("utf-8")

// 默认量程常量定义
const double RangeSettingsDialog::DEFAULT_FX_RANGE = 1000.0;
const double RangeSettingsDialog::DEFAULT_FY_RANGE = 1000.0;
const double RangeSettingsDialog::DEFAULT_FZ_RANGE = 1500.0;
const double RangeSettingsDialog::DEFAULT_MX_RANGE = 50.0;
const double RangeSettingsDialog::DEFAULT_MY_RANGE = 50.0;
const double RangeSettingsDialog::DEFAULT_MZ_RANGE = 50.0;

RangeSettingsDialog::RangeSettingsDialog(DataProcessor* dataProcessor, QWidget* parent)
    : QDialog(parent)
    , m_dataProcessor(dataProcessor)
    , m_wasAccepted(false)
{
    setupUI();
    loadCurrentRanges();
}

RangeSettingsDialog::~RangeSettingsDialog()
{
}

void RangeSettingsDialog::setupUI()
{
    setWindowTitle(QString::fromUtf8("设置各通道量程"));
    setMinimumWidth(450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // 说明文字
    QLabel* infoLabel = new QLabel(QString::fromUtf8(
        "设置各通道的最大量程值，用于归一化计算：\n"
        "• 默认量程: Fx=1000, Fy=1000, Fz=1500, Mx=My=Mz=50\n"
        "• 量程设置立即生效，应用于后续所有计算"
    ), this);
    infoLabel->setWordWrap(true);
    infoLabel->setStyleSheet("QLabel { padding: 10px; background-color: #f0f0f0; border-radius: 5px; }");
    mainLayout->addWidget(infoLabel);

    // 输入框网格
    QGridLayout* gridLayout = new QGridLayout();

    // 创建6个通道的输入框
    m_spinFx = new QDoubleSpinBox(this);
    m_spinFy = new QDoubleSpinBox(this);
    m_spinFz = new QDoubleSpinBox(this);
    m_spinMx = new QDoubleSpinBox(this);
    m_spinMy = new QDoubleSpinBox(this);
    m_spinMz = new QDoubleSpinBox(this);

    // 设置范围和格式
    QList<QDoubleSpinBox*> spinBoxes = {m_spinFx, m_spinFy, m_spinFz, m_spinMx, m_spinMy, m_spinMz};
    for (QDoubleSpinBox* spinBox : spinBoxes) {
        spinBox->setMinimum(0.1);
        spinBox->setMaximum(100000.0);
        spinBox->setSingleStep(10.0);
        spinBox->setDecimals(1);
        spinBox->setMinimumWidth(120);
    }

    // 布局（3列×2行）
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Fx 量程:"), this), 0, 0);
    gridLayout->addWidget(m_spinFx, 0, 1);
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Fy 量程:"), this), 0, 2);
    gridLayout->addWidget(m_spinFy, 0, 3);
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Fz 量程:"), this), 0, 4);
    gridLayout->addWidget(m_spinFz, 0, 5);

    gridLayout->addWidget(new QLabel(QString::fromUtf8("Mx 量程:"), this), 1, 0);
    gridLayout->addWidget(m_spinMx, 1, 1);
    gridLayout->addWidget(new QLabel(QString::fromUtf8("My 量程:"), this), 1, 2);
    gridLayout->addWidget(m_spinMy, 1, 3);
    gridLayout->addWidget(new QLabel(QString::fromUtf8("Mz 量程:"), this), 1, 4);
    gridLayout->addWidget(m_spinMz, 1, 5);

    mainLayout->addLayout(gridLayout);

    // 按钮区域
    QDialogButtonBox* buttonBox = new QDialogButtonBox(this);
    QPushButton* applyButton = buttonBox->addButton(QString::fromUtf8("应用"), QDialogButtonBox::AcceptRole);
    QPushButton* resetButton = buttonBox->addButton(QString::fromUtf8("恢复默认"), QDialogButtonBox::ResetRole);
    QPushButton* cancelButton = buttonBox->addButton(QString::fromUtf8("取消"), QDialogButtonBox::RejectRole);

    mainLayout->addWidget(buttonBox);

    // 连接信号
    connect(applyButton, &QPushButton::clicked, this, &RangeSettingsDialog::onApplyClicked);
    connect(resetButton, &QPushButton::clicked, this, &RangeSettingsDialog::onResetClicked);
    connect(cancelButton, &QPushButton::clicked, this, &RangeSettingsDialog::onCancelClicked);
}

void RangeSettingsDialog::loadCurrentRanges()
{
    if (!m_dataProcessor) {
        return;
    }

    m_spinFx->setValue(m_dataProcessor->getChannelRange("Fx"));
    m_spinFy->setValue(m_dataProcessor->getChannelRange("Fy"));
    m_spinFz->setValue(m_dataProcessor->getChannelRange("Fz"));
    m_spinMx->setValue(m_dataProcessor->getChannelRange("Mx"));
    m_spinMy->setValue(m_dataProcessor->getChannelRange("My"));
    m_spinMz->setValue(m_dataProcessor->getChannelRange("Mz"));
}

void RangeSettingsDialog::applyRangesToProcessor()
{
    if (!m_dataProcessor) {
        return;
    }

    m_dataProcessor->setAllChannelRanges(
        m_spinFx->value(),
        m_spinFy->value(),
        m_spinFz->value(),
        m_spinMx->value(),
        m_spinMy->value(),
        m_spinMz->value()
    );
}

QMap<QString, double> RangeSettingsDialog::getRangeValues() const
{
    QMap<QString, double> ranges;
    ranges["Fx"] = m_spinFx->value();
    ranges["Fy"] = m_spinFy->value();
    ranges["Fz"] = m_spinFz->value();
    ranges["Mx"] = m_spinMx->value();
    ranges["My"] = m_spinMy->value();
    ranges["Mz"] = m_spinMz->value();
    return ranges;
}

void RangeSettingsDialog::onApplyClicked()
{
    applyRangesToProcessor();
    m_wasAccepted = true;
    accept();
}

void RangeSettingsDialog::onResetClicked()
{
    m_spinFx->setValue(DEFAULT_FX_RANGE);
    m_spinFy->setValue(DEFAULT_FY_RANGE);
    m_spinFz->setValue(DEFAULT_FZ_RANGE);
    m_spinMx->setValue(DEFAULT_MX_RANGE);
    m_spinMy->setValue(DEFAULT_MY_RANGE);
    m_spinMz->setValue(DEFAULT_MZ_RANGE);
}

void RangeSettingsDialog::onCancelClicked()
{
    m_wasAccepted = false;
    reject();
}
