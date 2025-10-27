// 矩阵运算辅助函数
#include "DataProcessor.h"
#include <QtMath>
#include <QString>
#include <QVector>

#pragma execution_character_set("utf-8")

// 矩阵转置：A^T
void transposeMatrix(const QVector<QVector<double>>& input, QVector<QVector<double>>& output)
{
    if (input.isEmpty()) return;

    int rows = input.size();
    int cols = input[0].size();

    output.resize(cols);
    for (int i = 0; i < cols; ++i) {
        output[i].resize(rows);
        for (int j = 0; j < rows; ++j) {
            output[i][j] = input[j][i];
        }
    }
}

// 矩阵乘法：C = A × B
bool multiplyMatrix(const QVector<QVector<double>>& A, const QVector<QVector<double>>& B, QVector<QVector<double>>& C)
{
    if (A.isEmpty() || B.isEmpty()) return false;
    if (A[0].size() != B.size()) return false;  // A的列数必须等于B的行数

    int rowsA = A.size();
    int colsA = A[0].size();
    int colsB = B[0].size();

    C.resize(rowsA);
    for (int i = 0; i < rowsA; ++i) {
        C[i].resize(colsB);
        for (int j = 0; j < colsB; ++j) {
            C[i][j] = 0.0;
            for (int k = 0; k < colsA; ++k) {
                C[i][j] += A[i][k] * B[k][j];
            }
        }
    }
    return true;
}

// 矩阵求逆（使用高斯-若尔当消元法）
bool inverseMatrix(const QVector<QVector<double>>& input, QVector<QVector<double>>& output)
{
    if (input.isEmpty()) return false;

    int n = input.size();
    if (n != input[0].size()) return false;  // 必须是方阵

    // 创建增广矩阵 [A | I]
    QVector<QVector<double>> augmented(n);
    for (int i = 0; i < n; ++i) {
        augmented[i].resize(2 * n);
        for (int j = 0; j < n; ++j) {
            augmented[i][j] = input[i][j];
            augmented[i][j + n] = (i == j) ? 1.0 : 0.0;
        }
    }

    // 高斯-若尔当消元
    for (int i = 0; i < n; ++i) {
        // 找到主元
        int maxRow = i;
        double maxVal = qAbs(augmented[i][i]);
        for (int k = i + 1; k < n; ++k) {
            if (qAbs(augmented[k][i]) > maxVal) {
                maxVal = qAbs(augmented[k][i]);
                maxRow = k;
            }
        }

        // 检查是否奇异
        if (maxVal < 1e-10) {
            return false;  // 矩阵不可逆
        }

        // 交换行
        if (maxRow != i) {
            augmented[i].swap(augmented[maxRow]);
        }

        // 归一化主元行
        double pivot = augmented[i][i];
        for (int j = 0; j < 2 * n; ++j) {
            augmented[i][j] /= pivot;
        }

        // 消元
        for (int k = 0; k < n; ++k) {
            if (k != i) {
                double factor = augmented[k][i];
                for (int j = 0; j < 2 * n; ++j) {
                    augmented[k][j] -= factor * augmented[i][j];
                }
            }
        }
    }

    // 提取逆矩阵
    output.resize(n);
    for (int i = 0; i < n; ++i) {
        output[i].resize(n);
        for (int j = 0; j < n; ++j) {
            output[i][j] = augmented[i][j + n];
        }
    }

    return true;
}

// 辅助函数：判断某一行数据属于哪个通道的纯通道测试
// 返回通道索引（0-5对应Fx-Mz），如果不属于任何纯通道测试则返回-1
static int determineChannelType(const QStringList& dataRow, const QVector<int>& channelIndices)
{
    // 定义需要检查为0的通道（考虑允许的伴随通道）
    // Fx, Fy, Fz: 其他所有通道都应该为0
    // Mx, My, Mz: Fx, Fy, Fz 可以有值（力矩测试时会产生力的分量）
    static const QVector<QVector<int>> channelsToCheckZero = {
        {1, 2, 3, 4, 5},  // Fx (idx=0): 检查 Fy, Fz, Mx, My, Mz
        {0, 2, 3, 4, 5},  // Fy (idx=1): 检查 Fx, Fz, Mx, My, Mz
        {0, 1, 3, 4, 5},  // Fz (idx=2): 检查 Fx, Fy, Mx, My, Mz
        {4, 5},           // Mx (idx=3): 检查 My, Mz (Fx, Fy, Fz 允许有值)
        {3, 5},           // My (idx=4): 检查 Mx, Mz (Fx, Fy, Fz 允许有值)
        {3, 4}            // Mz (idx=5): 检查 Mx, My (Fx, Fy, Fz 允许有值)
    };

    const double zeroThreshold = 0.0001;

    // 对每个通道，检查该行数据是否满足该通道的纯通道测试条件
    // 只统计非零数据，0点数据不归类（稍后根据量程情况统一添加）
    for (int channelIdx = 0; channelIdx < 6; ++channelIdx) {
        if (channelIndices[channelIdx] == -1) continue;

        // 获取当前通道的设定值
        double currentChannelValue = 0.0;
        if (channelIndices[channelIdx] < dataRow.size()) {
            bool ok;
            currentChannelValue = dataRow[channelIndices[channelIdx]].toDouble(&ok);
            if (!ok) continue;
        }

        // 如果当前通道的值接近0，跳过（0点数据不归类）
        if (qAbs(currentChannelValue) < zeroThreshold) continue;

        // 检查需要为0的通道是否都接近0
        bool isPureChannelTest = true;
        for (int checkIdx : channelsToCheckZero[channelIdx]) {
            if (channelIndices[checkIdx] == -1) continue;

            if (channelIndices[checkIdx] < dataRow.size()) {
                bool ok;
                double checkValue = dataRow[channelIndices[checkIdx]].toDouble(&ok);
                if (ok && qAbs(checkValue) > zeroThreshold) {
                    isPureChannelTest = false;
                    break;
                }
            }
        }

        // 如果满足纯通道测试条件，返回该通道索引
        if (isPureChannelTest) {
            return channelIdx;
        }
    }

    // 不属于任何纯通道测试（包括0点数据）
    return -1;
}

DecouplingResult DataProcessor::calculateDecouplingMatrix(const QVector<QStringList>& originalData)
{
    DecouplingResult result;
    result.success = false;

    // 初始化矩阵为0
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            result.matrix[i][j] = 0.0;
        }
        result.channelRanges[i] = 0.0;  // 初始化量程为0
        result.channelCounts[i] = 0;    // 初始化计数为0
    }

    if (originalData.size() <= 1) {
        result.errorMessage = QString::fromUtf8("没有数据可处理");
        return result;
    }

    QStringList headers = originalData[0];

    // 查找所有通道的列索引
    QStringList channels = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
    QStringList measureChannels = {"UFx", "UFy", "UFz", "UMx", "UMy", "UMz"};

    QVector<int> setIndices(6);
    QVector<int> measureIndices(6);

    for (int i = 0; i < 6; ++i) {
        setIndices[i] = findColumnIndex(headers, channels[i]);
        measureIndices[i] = findColumnIndex(headers, measureChannels[i]);

        if (setIndices[i] == -1 || measureIndices[i] == -1) {
            result.errorMessage = QString::fromUtf8("未找到必需的列: %1 或 %2")
                .arg(channels[i]).arg(measureChannels[i]);
            return result;
        }
    }

    // 收集数据到U矩阵和F矩阵
    QVector<QVector<double>> U_matrix;  // 测量值矩阵
    QVector<QVector<double>> F_matrix;  // 设定值矩阵

    for (int i = 1; i < originalData.size(); ++i) {
        const QStringList& row = originalData[i];

        QVector<double> u_row(6);
        QVector<double> f_row(6);
        bool validRow = true;

        for (int j = 0; j < 6; ++j) {
            if (measureIndices[j] >= row.size() || setIndices[j] >= row.size()) {
                validRow = false;
                break;
            }

            bool uOk, fOk;
            u_row[j] = row[measureIndices[j]].toDouble(&uOk);
            f_row[j] = row[setIndices[j]].toDouble(&fOk);

            if (!uOk || !fOk) {
                validRow = false;
                break;
            }
        }

        if (validRow) {
            U_matrix.append(u_row);
            F_matrix.append(f_row);
        }
    }

    if (U_matrix.isEmpty() || F_matrix.isEmpty()) {
        result.errorMessage = QString::fromUtf8("没有有效的数据行");
        return result;
    }

    if (U_matrix.size() < 6) {
        result.errorMessage = QString::fromUtf8("数据行数不足（至少需要6行）");
        return result;
    }

    // 计算解耦矩阵: D = U^T × F × (F^T × F)^(-1)
    QVector<QVector<double>> U_T, F_T;
    QVector<QVector<double>> FT_F, FT_F_inv;
    QVector<QVector<double>> F_times_inv;
    QVector<QVector<double>> decoupling;

    // 1. 计算 F^T
    transposeMatrix(F_matrix, F_T);

    // 2. 计算 F^T × F
    if (!multiplyMatrix(F_T, F_matrix, FT_F)) {
        result.errorMessage = QString::fromUtf8("矩阵乘法失败: F^T × F");
        return result;
    }

    // 3. 计算 (F^T × F)^(-1)
    if (!inverseMatrix(FT_F, FT_F_inv)) {
        result.errorMessage = QString::fromUtf8("矩阵不可逆: F^T × F");
        return result;
    }

    // 4. 计算 F × (F^T × F)^(-1)
    if (!multiplyMatrix(F_matrix, FT_F_inv, F_times_inv)) {
        result.errorMessage = QString::fromUtf8("矩阵乘法失败: F × (F^T × F)^(-1)");
        return result;
    }

    // 5. 计算 U^T
    transposeMatrix(U_matrix, U_T);

    // 6. 计算最终的解耦矩阵: U^T × F × (F^T × F)^(-1)
    if (!multiplyMatrix(U_T, F_times_inv, decoupling)) {
        result.errorMessage = QString::fromUtf8("矩阵乘法失败: U^T × F × (F^T × F)^(-1)");
        return result;
    }

    // 检查结果矩阵维度
    if (decoupling.size() != 6 || decoupling[0].size() != 6) {
        result.errorMessage = QString::fromUtf8("解耦矩阵维度错误");
        return result;
    }

    // 复制第一矩阵结果
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            result.matrix[i][j] = decoupling[i][j];
        }
    }

    // 计算第二矩阵: (D^T × D)^(-1) × D^T
    QVector<QVector<double>> D_T;
    QVector<QVector<double>> DT_D, DT_D_inv;
    QVector<QVector<double>> secondMatrix;

    // 1. 计算 D^T (第一矩阵的转置)
    transposeMatrix(decoupling, D_T);

    // 2. 计算 D^T × D
    if (!multiplyMatrix(D_T, decoupling, DT_D)) {
        result.errorMessage = QString::fromUtf8("第二矩阵计算失败: D^T × D");
        result.success = false;
        return result;
    }

    // 3. 计算 (D^T × D)^(-1)
    if (!inverseMatrix(DT_D, DT_D_inv)) {
        result.errorMessage = QString::fromUtf8("第二矩阵计算失败: (D^T × D)不可逆");
        result.success = false;
        return result;
    }

    // 4. 计算 (D^T × D)^(-1) × D^T
    if (!multiplyMatrix(DT_D_inv, D_T, secondMatrix)) {
        result.errorMessage = QString::fromUtf8("第二矩阵计算失败: (D^T × D)^(-1) × D^T");
        result.success = false;
        return result;
    }

    // 检查第二矩阵维度
    if (secondMatrix.size() != 6 || secondMatrix[0].size() != 6) {
        result.errorMessage = QString::fromUtf8("第二矩阵维度错误");
        result.success = false;
        return result;
    }

    // 复制第二矩阵结果
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            result.secondMatrix[i][j] = secondMatrix[i][j];
        }
    }

    // 计算第三矩阵: F^T × U × (U^T × U)^(-1)
    QVector<QVector<double>> U_T_new, F_T_new;
    QVector<QVector<double>> UT_U, UT_U_inv;
    QVector<QVector<double>> U_times_inv;
    QVector<QVector<double>> thirdMatrix;

    // 1. 计算 U^T
    transposeMatrix(U_matrix, U_T_new);

    // 2. 计算 U^T × U
    if (!multiplyMatrix(U_T_new, U_matrix, UT_U)) {
        result.errorMessage = QString::fromUtf8("第三矩阵计算失败: U^T × U");
        result.success = false;
        return result;
    }

    // 3. 计算 (U^T × U)^(-1)
    if (!inverseMatrix(UT_U, UT_U_inv)) {
        result.errorMessage = QString::fromUtf8("第三矩阵计算失败: (U^T × U)不可逆");
        result.success = false;
        return result;
    }

    // 4. 计算 U × (U^T × U)^(-1)
    if (!multiplyMatrix(U_matrix, UT_U_inv, U_times_inv)) {
        result.errorMessage = QString::fromUtf8("第三矩阵计算失败: U × (U^T × U)^(-1)");
        result.success = false;
        return result;
    }

    // 5. 计算 F^T
    transposeMatrix(F_matrix, F_T_new);

    // 6. 计算最终的第三矩阵: F^T × U × (U^T × U)^(-1)
    if (!multiplyMatrix(F_T_new, U_times_inv, thirdMatrix)) {
        result.errorMessage = QString::fromUtf8("第三矩阵计算失败: F^T × U × (U^T × U)^(-1)");
        result.success = false;
        return result;
    }

    // 检查第三矩阵维度
    if (thirdMatrix.size() != 6 || thirdMatrix[0].size() != 6) {
        result.errorMessage = QString::fromUtf8("第三矩阵维度错误");
        result.success = false;
        return result;
    }

    // 复制第三矩阵结果
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            result.thirdMatrix[i][j] = thirdMatrix[i][j];
        }
    }

    // 计算解耦后数据: Fx', Fy', Fz', Mx', My', Mz'
    // 公式: 解耦后数据 = TRANSPOSE(MMULT(第二矩阵, TRANSPOSE(U矩阵)))
    // 即: F' = (secondMatrix × U^T)^T = U × secondMatrix^T

    QVector<QVector<double>> secondMatrix_T;
    QVector<QVector<double>> decoupledMatrix;

    // 1. 计算第二矩阵的转置
    QVector<QVector<double>> secondMatrixVec(6);
    for (int i = 0; i < 6; ++i) {
        secondMatrixVec[i].resize(6);
        for (int j = 0; j < 6; ++j) {
            secondMatrixVec[i][j] = secondMatrix[i][j];
        }
    }
    transposeMatrix(secondMatrixVec, secondMatrix_T);

    // 2. 计算 U × secondMatrix^T
    if (!multiplyMatrix(U_matrix, secondMatrix_T, decoupledMatrix)) {
        result.errorMessage = QString::fromUtf8("解耦数据计算失败: U × secondMatrix^T");
        result.success = false;
        return result;
    }

    // 3. 生成解耦后数据表格
    result.decoupledData.clear();

    // 添加表头
    QStringList header;
    header << QString::fromUtf8("Fx'") << QString::fromUtf8("Fy'") << QString::fromUtf8("Fz'")
           << QString::fromUtf8("Mx'") << QString::fromUtf8("My'") << QString::fromUtf8("Mz'");
    result.decoupledData.append(header);

    // 添加数据行
    for (int i = 0; i < decoupledMatrix.size(); ++i) {
        QStringList row;
        for (int j = 0; j < 6; ++j) {
            row << QString::number(decoupledMatrix[i][j], 'f', 8);
        }
        result.decoupledData.append(row);
    }

    // 4. 计算I类误差和II类误差：Fx-Fx', Fy-Fy', Fz-Fz', Mx-Mx', My-My', Mz-Mz'
    result.errorData.clear();

    // 添加表头
    QStringList errorHeader;
    errorHeader << QString::fromUtf8("Fx-Fx'") << QString::fromUtf8("Fy-Fy'") << QString::fromUtf8("Fz-Fz'")
                << QString::fromUtf8("Mx-Mx'") << QString::fromUtf8("My-My'") << QString::fromUtf8("Mz-Mz'");
    result.errorData.append(errorHeader);

    // 从原始数据中找到Fx, Fy, Fz, Mx, My, Mz列的索引
    if (originalData.size() > 1) {
        QStringList headers = originalData[0];
        QStringList channelNames = {"Fx", "Fy", "Fz", "Mx", "My", "Mz"};
        QVector<int> channelIndices(6);

        for (int i = 0; i < 6; ++i) {
            channelIndices[i] = findColumnIndex(headers, channelNames[i]);
        }

        // 使用手动设置的量程（如果已设置），否则自动检测
        QVector<double> channelRanges(6, 0.0);

        // 先尝试使用手动设置的量程
        channelRanges[0] = getChannelRange("Fx");
        channelRanges[1] = getChannelRange("Fy");
        channelRanges[2] = getChannelRange("Fz");
        channelRanges[3] = getChannelRange("Mx");
        channelRanges[4] = getChannelRange("My");
        channelRanges[5] = getChannelRange("Mz");

        // 保存量程到结果中（用于显示）
        for (int i = 0; i < 6; ++i) {
            result.channelRanges[i] = channelRanges[i];
        }

        // 计算每一行的误差
        for (int i = 1; i < originalData.size() && i < decoupledMatrix.size() + 1; ++i) {
            const QStringList& dataRow = originalData[i];
            QStringList errorRow;

            for (int j = 0; j < 6; ++j) {
                double setValue = 0.0;
                double decoupledValue = 0.0;

                // 获取设定值 (Fx, Fy, Fz...)
                if (channelIndices[j] != -1 && channelIndices[j] < dataRow.size()) {
                    bool ok;
                    setValue = dataRow[channelIndices[j]].toDouble(&ok);
                    if (!ok) setValue = 0.0;
                }

                // 获取解耦后的值 (Fx', Fy', Fz'...)
                if (i - 1 < decoupledMatrix.size() && j < 6) {
                    decoupledValue = decoupledMatrix[i - 1][j];
                }

                // 计算误差：设定值 - 解耦后的值
                double error = setValue - decoupledValue;
                errorRow << QString::number(error, 'f', 8);
            }

            result.errorData.append(errorRow);
        }

        // 5. 统计各通道的纯通道测试数据行数（需要先统计，用于后续归一化平方误差的通道分类）
        // 先统计非零数据
        QVector<bool> hasPositive(6, false);  // 记录每个通道是否有正量程数据
        QVector<bool> hasNegative(6, false);  // 记录每个通道是否有负量程数据

        for (int i = 1; i < originalData.size(); ++i) {
            const QStringList& dataRow = originalData[i];

            // 使用辅助函数判断当前行属于哪个通道
            int channelIdx = determineChannelType(dataRow, channelIndices);

            // 如果属于某个纯通道测试，计数+1
            if (channelIdx >= 0 && channelIdx < 6) {
                result.channelCounts[channelIdx]++;

                // 记录正负量程情况
                if (channelIndices[channelIdx] < dataRow.size()) {
                    bool ok;
                    double value = dataRow[channelIndices[channelIdx]].toDouble(&ok);
                    if (ok) {
                        if (value > 0.0001) hasPositive[channelIdx] = true;
                        if (value < -0.0001) hasNegative[channelIdx] = true;
                    }
                }
            }
        }

        // 根据量程情况添加0点个数
        for (int i = 0; i < 6; ++i) {
            if (hasPositive[i] && hasNegative[i]) {
                // 既有正量程又有负量程：加8个0点
                result.channelCounts[i] += 8;
            } else if (hasPositive[i] || hasNegative[i]) {
                // 只有单量程（只有正或只有负）：加4个0点
                result.channelCounts[i] += 4;
            }
            // 如果该通道没有任何数据，count保持为0
        }

        // 6. 计算归一化平方误差：((Fx-Fx')/量程)^2
        result.normalizedSquaredError.clear();

        // 添加表头（增加通道分类列）
        QStringList normalizedHeader;
        normalizedHeader << QString::fromUtf8("通道分类")
                        << QString::fromUtf8("((Fx-Fx')/量程)²") << QString::fromUtf8("((Fy-Fy')/量程)²")
                        << QString::fromUtf8("((Fz-Fz')/量程)²") << QString::fromUtf8("((Mx-Mx')/量程)²")
                        << QString::fromUtf8("((My-My')/量程)²") << QString::fromUtf8("((Mz-Mz')/量程)²");
        result.normalizedSquaredError.append(normalizedHeader);

        // 计算累计count，用于判断每行属于哪个通道
        // 数据按通道顺序排列：Fx -> Fy -> Fz -> Mx -> My -> Mz
        QVector<int> cumulativeCounts(7, 0);  // 累计count [0, Fx结束, Fy结束, Fz结束, Mx结束, My结束, Mz结束]
        for (int i = 0; i < 6; ++i) {
            cumulativeCounts[i + 1] = cumulativeCounts[i] + result.channelCounts[i];
        }

        // 计算归一化平方误差（跳过表头）
        for (int i = 1; i < result.errorData.size(); ++i) {
            const QStringList& errorRow = result.errorData[i];

            // 根据行号和累计count判断属于哪个通道
            int rowIndex = i - 1;  // 从0开始的行索引（去掉表头）
            QString belongsToChannel = QString::fromUtf8("未知");
            for (int ch = 0; ch < 6; ++ch) {
                if (rowIndex >= cumulativeCounts[ch] && rowIndex < cumulativeCounts[ch + 1]) {
                    belongsToChannel = channelNames[ch];
                    break;
                }
            }

            QStringList normalizedRow;
            normalizedRow << belongsToChannel;  // 添加通道分类

            for (int j = 0; j < 6; ++j) {
                double error = 0.0;
                if (j < errorRow.size()) {
                    bool ok;
                    error = errorRow[j].toDouble(&ok);
                    if (!ok) error = 0.0;
                }

                // 归一化并平方
                double normalizedSquared = 0.0;
                if (channelRanges[j] > 0.0001) {  // 避免除以0
                    double normalized = error / channelRanges[j];
                    normalizedSquared = normalized * normalized;
                }

                normalizedRow << QString::number(normalizedSquared, 'e', 6);
            }

            result.normalizedSquaredError.append(normalizedRow);
        }

        // 7. 为每个通道计算归一化平方误差的统计值
        // 公式：sqrt(sum(该通道该列的所有值) / (count - 1))
        // 对每个通道的6列（Fx', Fy', Fz', Mx', My', Mz'）分别计算
        result.channelStatistics.clear();

        // 添加表头
        QStringList statisticsHeader;
        statisticsHeader << QString::fromUtf8("通道")
                        << QString::fromUtf8("((Fx-Fx')/量程)²统计值")
                        << QString::fromUtf8("((Fy-Fy')/量程)²统计值")
                        << QString::fromUtf8("((Fz-Fz')/量程)²统计值")
                        << QString::fromUtf8("((Mx-Mx')/量程)²统计值")
                        << QString::fromUtf8("((My-My')/量程)²统计值")
                        << QString::fromUtf8("((Mz-Mz')/量程)²统计值");
        result.channelStatistics.append(statisticsHeader);

        // 为每个通道计算统计值
        for (int ch = 0; ch < 6; ++ch) {
            QStringList channelStatRow;
            channelStatRow << channelNames[ch];  // 通道名称

            // 计算该通道在6个误差列的统计值
            for (int col = 0; col < 6; ++col) {
                double sum = 0.0;

                // 遍历归一化平方误差数据，累加该通道该列的值
                for (int row = 1; row < result.normalizedSquaredError.size(); ++row) {
                    const QStringList& dataRow = result.normalizedSquaredError[row];

                    // 判断该行是否属于当前通道
                    int rowIndex = row - 1;
                    if (rowIndex >= cumulativeCounts[ch] && rowIndex < cumulativeCounts[ch + 1]) {
                        int dataColIndex = col + 1;  // +1 因为第一列是通道分类
                        if (dataColIndex < dataRow.size()) {
                            bool ok;
                            double value = dataRow[dataColIndex].toDouble(&ok);
                            if (ok) {
                                sum += value;
                            }
                        }
                    }
                }

                // 计算统计值
                double statisticValue = 0.0;
                if (result.channelCounts[ch] > 1) {
                    statisticValue = qSqrt(sum / (result.channelCounts[ch] - 1));
                }

                channelStatRow << QString::number(statisticValue, 'f', 8);
            }

            result.channelStatistics.append(channelStatRow);
        }

        // 8. 计算I类误差
        // I类误差 = sqrt(sum(对角线元素的平方) / 5)
        // 对角线元素：Fx行Fx列, Fy行Fy列, Fz行Fz列, Mx行Mx列, My行My列, Mz行Mz列
        double sumSquared = 0.0;
        for (int i = 0; i < 6; ++i) {
            // 对角线元素：第i个通道在第i列的统计值
            // result.channelStatistics[i+1][i+1]，+1是因为第0行是表头，第0列是通道名
            if (i + 1 < result.channelStatistics.size()) {
                const QStringList& row = result.channelStatistics[i + 1];
                if (i + 1 < row.size()) {
                    bool ok;
                    double value = row[i + 1].toDouble(&ok);
                    if (ok) {
                        sumSquared += value * value;
                    }
                }
            }
        }

        double typeIError = qSqrt(sumSquared / 5.0) * 100.0;  // 转换为百分比

        // 添加I类误差到表格
        QStringList typeIErrorRow;
        typeIErrorRow << QString::fromUtf8("I类误差");
        typeIErrorRow << QString::number(typeIError, 'f', 8);
        // 其他列填充空白
        for (int i = 1; i < 6; ++i) {
            typeIErrorRow << "";
        }
        result.channelStatistics.append(typeIErrorRow);

        // 9. 计算6个II类误差中间值
        // 对每一列（共6列），计算：sqrt(sum(该列所有非对角线值的平方) / 5)
        // 例如Fx列：sqrt((2行1列² + 3行1列² + 4行1列² + 5行1列² + 6行1列²) / 5)
        QStringList typeIIIntermediateRow;
        typeIIIntermediateRow << QString::fromUtf8("II类误差中间值");

        for (int col = 0; col < 6; ++col) {  // 对所有6列进行处理
            double sumSquared = 0.0;

            // 遍历该列的所有6行，排除对角线元素
            for (int row = 0; row < 6; ++row) {
                // 跳过对角线元素（Fx行Fx列、Fy行Fy列等）
                if (row == col) continue;

                // 获取值：result.channelStatistics[row+1][col+1]
                if (row + 1 < result.channelStatistics.size()) {
                    const QStringList& dataRow = result.channelStatistics[row + 1];
                    if (col + 1 < dataRow.size()) {
                        bool ok;
                        double value = dataRow[col + 1].toDouble(&ok);
                        if (ok) {
                            sumSquared += value * value;
                        }
                    }
                }
            }

            double intermediateValue = qSqrt(sumSquared / 5.0);
            typeIIIntermediateRow << QString::number(intermediateValue, 'f', 8);
        }

        result.channelStatistics.append(typeIIIntermediateRow);

        // 10. 计算II类误差值
        // II类误差 = sqrt(sum(6个II类误差中间值的平方) / 5)
        double typeIISumSquared = 0.0;
        for (int i = 1; i < typeIIIntermediateRow.size(); ++i) {  // 跳过第一列（标签列）
            bool ok;
            double value = typeIIIntermediateRow[i].toDouble(&ok);
            if (ok) {
                typeIISumSquared += value * value;
            }
        }

        double typeIIError = qSqrt(typeIISumSquared / 5.0) * 100.0;  // 转换为百分比

        // 添加II类误差到表格
        QStringList typeIIErrorRow;
        typeIIErrorRow << QString::fromUtf8("II类误差");
        typeIIErrorRow << QString::number(typeIIError, 'f', 8);
        // 其他列填充空白
        for (int i = 1; i < 6; ++i) {
            typeIIErrorRow << "";
        }
        result.channelStatistics.append(typeIIErrorRow);
    }

    result.success = true;
    return result;
}
