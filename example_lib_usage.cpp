/**
 * @file example_lib_usage.cpp
 * @brief DataSolverLib 使用示例
 *
 * 此文件演示如何使用 DataSolverLib 静态库来处理传感器数据。
 *
 * 编译方法：
 * 1. 在 Visual Studio 中打开 Data_Solve.sln
 * 2. 构建 DataSolverLib 项目（生成 DataSolverLib.lib）
 * 3. 创建一个新的控制台应用项目，并链接 DataSolverLib.lib
 * 4. 将此文件添加到新项目中
 *
 * 或者使用命令行编译：
 * cl.exe example_lib_usage.cpp /I. /link DataSolverLib.lib Qt5Core.lib /LIBPATH:Win32\Release
 */

#include "DataSolverLib.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char* argv[])
{
    printf("=================================================\n");
    printf("   DataSolverLib 使用示例\n");
    printf("   版本: %s\n", GetLibraryVersion());
    printf("=================================================\n\n");

    // ==================== 示例1: 处理所有6个通道 ====================
    printf("【示例1】处理所有通道并导出完整数据\n");
    printf("-------------------------------------------------\n");

    const char* inputFile = "test_data.csv";  // 输入CSV文件
    const char* outputFile = "result_all_channels.csv";  // 输出CSV文件
    char errorMsg[512];

    // 可选：设置各通道量程（如果不设置，使用默认值）
    printf("设置通道量程...\n");
    SetChannelRanges(
        1000.0,  // Fx 量程
        1000.0,  // Fy 量程
        1500.0,  // Fz 量程
        50.0,    // Mx 量程
        50.0,    // My 量程
        50.0     // Mz 量程
    );
    printf("  Fx=1000, Fy=1000, Fz=1500\n");
    printf("  Mx=50, My=50, Mz=50\n\n");

    // 处理数据文件
    printf("正在处理文件: %s\n", inputFile);
    int status = ProcessDataFile(inputFile, outputFile, errorMsg, sizeof(errorMsg));

    if (status == DS_SUCCESS) {
        printf("✓ 处理成功！\n");
        printf("  输出文件: %s\n\n", outputFile);
        printf("输出文件包含以下内容：\n");
        printf("  1. 性能指标评价表（重复性、迟滞性、线性度、I类/II类误差）\n");
        printf("  2. 各通道详细数据（Fx, Fy, Fz, Mx, My, Mz）\n");
        printf("  3. 解耦矩阵（第一、第二、第三矩阵）\n");
        printf("  4. 误差分析数据（解耦后数据、原始误差、归一化平方误差）\n");
    } else {
        printf("✗ 处理失败！\n");
        printf("  错误代码: %d\n", status);
        printf("  错误信息: %s\n", errorMsg);
    }

    printf("\n");

    // ==================== 示例2: 只处理单个通道 ====================
    printf("【示例2】只处理Fx通道\n");
    printf("-------------------------------------------------\n");

    const char* channelOutputFile = "result_fx_only.csv";  // 输出CSV文件

    printf("正在处理Fx通道...\n");
    status = ProcessChannelDataFile(inputFile, "Fx", channelOutputFile, errorMsg, sizeof(errorMsg));

    if (status == DS_SUCCESS) {
        printf("✓ 处理成功！\n");
        printf("  输出文件: %s\n\n", channelOutputFile);
        printf("输出文件包含：\n");
        printf("  - Fx通道的重复性误差数据\n");
        printf("  - Fx通道的线性度数据\n");
    } else {
        printf("✗ 处理失败！\n");
        printf("  错误代码: %d\n", status);
        printf("  错误信息: %s\n", errorMsg);
    }

    printf("\n");

    // ==================== 示例3: 自动生成输出文件名 ====================
    printf("【示例3】自动生成输出文件名\n");
    printf("-------------------------------------------------\n");

    printf("正在处理文件（自动生成输出文件名）...\n");
    // 将 outputCsvPath 设置为 NULL，库会自动生成输出文件名
    status = ProcessDataFile(inputFile, NULL, errorMsg, sizeof(errorMsg));

    if (status == DS_SUCCESS) {
        printf("✓ 处理成功！\n");
        printf("  输出文件已自动命名为: test_data_processed.csv\n");
    } else {
        printf("✗ 处理失败！\n");
        printf("  错误代码: %d\n", status);
        printf("  错误信息: %s\n", errorMsg);
    }

    printf("\n");

    // ==================== 错误处理示例 ====================
    printf("【示例4】错误处理演示\n");
    printf("-------------------------------------------------\n");

    // 尝试打开不存在的文件
    status = ProcessDataFile("nonexistent.csv", "output.csv", errorMsg, sizeof(errorMsg));
    printf("尝试打开不存在的文件...\n");
    printf("  返回代码: %d (%s)\n", status,
           status == DS_ERROR_FILE_NOT_FOUND ? "DS_ERROR_FILE_NOT_FOUND" : "其他错误");
    printf("  错误信息: %s\n\n", errorMsg);

    // 尝试使用无效的通道名
    status = ProcessChannelDataFile(inputFile, "InvalidChannel", "output.csv", errorMsg, sizeof(errorMsg));
    printf("尝试使用无效的通道名...\n");
    printf("  返回代码: %d (%s)\n", status,
           status == DS_ERROR_INVALID_PARAM ? "DS_ERROR_INVALID_PARAM" : "其他错误");
    printf("  错误信息: %s\n\n", errorMsg);

    printf("=================================================\n");
    printf("   示例程序运行完毕\n");
    printf("=================================================\n");

    return 0;
}
