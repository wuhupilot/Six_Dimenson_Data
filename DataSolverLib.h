#pragma once

#ifdef DATASOLVERLIB_EXPORTS
#define DATASOLVERLIB_API __declspec(dllexport)
#else
#define DATASOLVERLIB_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 数据处理结果状态码
 */
typedef enum {
    DS_SUCCESS = 0,              // 成功
    DS_ERROR_FILE_NOT_FOUND = 1, // 输入文件不存在
    DS_ERROR_INVALID_FORMAT = 2, // 文件格式无效
    DS_ERROR_PROCESSING = 3,     // 数据处理失败
    DS_ERROR_EXPORT = 4,         // 导出失败
    DS_ERROR_INVALID_PARAM = 5   // 无效参数
} DataSolverStatus;

/**
 * @brief 处理单个CSV文件，导出所有6个通道的完整数据
 *
 * @param inputCsvPath 输入CSV文件的完整路径（UTF-8编码）
 * @param outputCsvPath 输出CSV文件的完整路径（UTF-8编码），如果为NULL则自动生成
 * @param errorMessage 错误信息缓冲区（输出参数），可以为NULL
 * @param errorMessageSize 错误信息缓冲区大小
 * @return DataSolverStatus 状态码
 *
 * 功能说明：
 * - 加载输入CSV文件
 * - 自动处理所有6个通道（Fx, Fy, Fz, Mx, My, Mz）
 * - 导出包含以下内容的CSV文件：
 *   1. 性能指标评价表（重复性、迟滞性、线性度、I类误差、II类误差）
 *   2. 各通道详细数据（重复性误差、线性度数据）
 *   3. 解耦矩阵（第一、第二、第三矩阵）
 *   4. 误差分析数据（解耦后数据、原始误差、归一化平方误差）
 *
 * 示例：
 *   char error[512];
 *   int status = ProcessDataFile("C:\\input.csv", "C:\\output.csv", error, 512);
 *   if (status != DS_SUCCESS) {
 *       printf("Error: %s\n", error);
 *   }
 */
DATASOLVERLIB_API int ProcessDataFile(
    const char* inputCsvPath,
    const char* outputCsvPath,
    char* errorMessage,
    int errorMessageSize
);

/**
 * @brief 处理CSV文件（带预处理）：平均 → 置零校准 → 数据处理 → 导出
 *
 * @param inputCsvPath 输入CSV文件路径（UTF-8编码）
 * @param outputCsvPath 输出CSV文件路径（UTF-8编码）
 * @param averageGroupSize 平均分组大小（0表示不进行平均）
 * @param zeroThreshold 零点判断阈值（默认0.1）
 * @param enableZeroCalibration 是否启用置零校准（1=启用，0=不启用）
 * @param errorMessage 错误信息缓冲区（输出参数），可以为NULL
 * @param errorMessageSize 错误信息缓冲区大小
 * @return DataSolverStatus 状态码
 *
 * 处理流程：
 * 1. 加载CSV文件
 * 2. 数据平均（如果averageGroupSize > 0）
 * 3. 置零校准（如果enableZeroCalibration = 1）
 * 4. 处理所有6个通道
 * 5. 导出完整结果
 *
 * 示例：
 *   char error[512];
 *   // 每5行平均，阈值0.1，启用置零校准
 *   int status = ProcessDataFileWithPreprocessing(
 *       "C:\\input.csv", "C:\\output.csv", 5, 0.1, 1, error, 512);
 */
DATASOLVERLIB_API int ProcessDataFileWithPreprocessing(
    const char* inputCsvPath,
    const char* outputCsvPath,
    int averageGroupSize,
    double zeroThreshold,
    int enableZeroCalibration,
    char* errorMessage,
    int errorMessageSize
);

/**
 * @brief 处理CSV文件，只导出指定通道的数据
 *
 * @param inputCsvPath 输入CSV文件的完整路径（UTF-8编码）
 * @param channelName 通道名称（"Fx", "Fy", "Fz", "Mx", "My", "Mz"）
 * @param outputCsvPath 输出CSV文件的完整路径（UTF-8编码），如果为NULL则自动生成
 * @param errorMessage 错误信息缓冲区（输出参数），可以为NULL
 * @param errorMessageSize 错误信息缓冲区大小
 * @return DataSolverStatus 状态码
 *
 * 功能说明：
 * - 加载输入CSV文件
 * - 仅处理指定的单个通道
 * - 导出该通道的重复性误差和线性度数据
 */
DATASOLVERLIB_API int ProcessChannelDataFile(
    const char* inputCsvPath,
    const char* channelName,
    const char* outputCsvPath,
    char* errorMessage,
    int errorMessageSize
);

/**
 * @brief 设置各通道的量程值
 *
 * @param fxRange Fx通道量程
 * @param fyRange Fy通道量程
 * @param fzRange Fz通道量程
 * @param mxRange Mx通道量程
 * @param myRange My通道量程
 * @param mzRange Mz通道量程
 *
 * 说明：
 * - 必须在调用ProcessDataFile之前设置
 * - 默认值：Fx=1000, Fy=1000, Fz=1500, Mx=50, My=50, Mz=50
 */
DATASOLVERLIB_API void SetChannelRanges(
    double fxRange,
    double fyRange,
    double fzRange,
    double mxRange,
    double myRange,
    double mzRange
);

/**
 * @brief 获取库的版本信息
 *
 * @return 版本字符串（例如："1.0.0"）
 */
DATASOLVERLIB_API const char* GetLibraryVersion();

#ifdef __cplusplus
}
#endif
