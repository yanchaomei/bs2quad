/**
 * @file Common.h
 * @brief 公共类型定义和常量
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_COMMON_H
#define BS2QUAD_COMMON_H

#include <string>
#include <vector>
#include <memory>
#include <cmath>
#include <limits>
#include <stdexcept>

// OpenCASCADE核心头文件
#include <Standard_TypeDef.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>
#include <gp_Dir.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Ax3.hxx>

namespace BS2Quad {

/**
 * @brief 二次曲面类型枚举
 */
enum class QuadricType {
    NONE = 0,        ///< 无法转换或未知类型
    PLANE,           ///< 平面
    CYLINDER,        ///< 圆柱面
    CONE,            ///< 圆锥面
    SPHERE,          ///< 球面
    TORUS            ///< 圆环面
};

/**
 * @brief 转换为字符串
 */
inline std::string QuadricTypeToString(QuadricType type) {
    switch (type) {
        case QuadricType::PLANE: return "Plane";
        case QuadricType::CYLINDER: return "Cylinder";
        case QuadricType::CONE: return "Cone";
        case QuadricType::SPHERE: return "Sphere";
        case QuadricType::TORUS: return "Torus";
        default: return "None";
    }
}

/**
 * @brief 异常类型枚举
 */
enum class ErrorType {
    SUCCESS = 0,          ///< 成功
    DATA_CORRUPT,         ///< 数据损坏
    OUT_OF_RANGE,         ///< 参数越界
    NON_QUADRIC,          ///< 非二次曲面
    HIGH_ERROR,           ///< 误差过大
    DEGENERATE_SURFACE,   ///< 退化曲面
    UNKNOWN_ERROR         ///< 未知错误
};

/**
 * @brief 转换为字符串
 */
inline std::string ErrorTypeToString(ErrorType type) {
    switch (type) {
        case ErrorType::SUCCESS: return "Success";
        case ErrorType::DATA_CORRUPT: return "Data Corrupt";
        case ErrorType::OUT_OF_RANGE: return "Out of Range";
        case ErrorType::NON_QUADRIC: return "Non-Quadric Surface";
        case ErrorType::HIGH_ERROR: return "HIGH_ERROR";
        case ErrorType::DEGENERATE_SURFACE: return "Degenerate Surface";
        default: return "Unknown Error";
    }
}

/**
 * @brief 精度常量
 */
namespace Precision {
    constexpr double DEFAULT_TOLERANCE = 1.0e-5;      ///< 默认几何误差容差（mm）
    constexpr double ANGLE_TOLERANCE = 0.1;           ///< G1连续性角度容差（度）
    constexpr double ANGLE_TOLERANCE_RAD = 0.1 * M_PI / 180.0; ///< G1连续性角度容差（弧度）
    constexpr double ZERO_TOLERANCE = 1.0e-10;        ///< 零值判断容差
    constexpr double CURVATURE_TOLERANCE = 1.0e-6;    ///< 曲率容差
}

/**
 * @brief 性能指标常量
 */
namespace Performance {
    constexpr int MAX_CONVERSION_TIME_MS = 10;        ///< 最大转换时间（毫秒）
    constexpr double MIN_PERFORMANCE_GAIN = 0.5;      ///< 最小性能提升比例（50%）
    constexpr int STABILITY_TEST_COUNT = 1000;        ///< 稳定性测试曲面数量
}

/**
 * @brief 采样参数
 */
namespace Sampling {
    constexpr int MIN_SAMPLES_U = 10;                 ///< U方向最小采样点数
    constexpr int MIN_SAMPLES_V = 10;                 ///< V方向最小采样点数
    constexpr int MAX_SAMPLES_U = 100;                ///< U方向最大采样点数
    constexpr int MAX_SAMPLES_V = 100;                ///< V方向最大采样点数
    constexpr int ADAPTIVE_SAMPLES = 50;              ///< 自适应采样点数
}

/**
 * @brief 曲面分析结果
 */
struct SurfaceAnalysisResult {
    QuadricType type;                 ///< 识别的曲面类型
    double confidence;                ///< 置信度（0-1）
    ErrorType error;                  ///< 错误类型
    std::string message;              ///< 详细信息
    
    /**
     * @brief 构造函数
     */
    SurfaceAnalysisResult() 
        : type(QuadricType::NONE), 
          confidence(0.0), 
          error(ErrorType::SUCCESS),
          message("") {}
    
    /**
     * @brief 判断是否成功
     */
    bool IsSuccess() const { return error == ErrorType::SUCCESS && type != QuadricType::NONE; }
};

/**
 * @brief 精度验证结果
 */
struct AccuracyResult {
    double maxDistance;               ///< 最大几何距离误差（mm）
    double avgDistance;               ///< 平均距离误差（mm）
    double rmsDistance;               ///< RMS距离误差（mm）
    double hausdorffDistance;         ///< Hausdorff距离（边界）（mm）
    double g1ContinuityError;         ///< G1连续性误差（度）
    bool meetsRequirements;           ///< 是否满足精度要求
    bool validationComplete = false;  ///< 所请求检查是否完整运行；失败不能解释为零误差
    bool surfaceChecked = false;
    bool boundaryChecked = false;
    bool normalChecked = false;       ///< 原面到转换面法向保持，区别于相邻面 G1
    bool adjacencyChecked = false;
    bool maxDistanceCertified = false; ///< 是否具有独立全参数域上界
    double certifiedMaxDistance = std::numeric_limits<double>::infinity();
    double normalDeviationDegrees = std::numeric_limits<double>::infinity();
    size_t sampleCount = 0;
    size_t failedSampleCount = 0;
    std::string message;
    
    /**
     * @brief 构造函数
     */
    AccuracyResult()
        : maxDistance(std::numeric_limits<double>::infinity()),
          avgDistance(std::numeric_limits<double>::infinity()),
          rmsDistance(std::numeric_limits<double>::infinity()),
          hausdorffDistance(std::numeric_limits<double>::infinity()),
          g1ContinuityError(std::numeric_limits<double>::infinity()),
          meetsRequirements(false) {}
};

/**
 * @brief 性能对比结果
 */
struct PerformanceResult {
    bool projectionComplete = false;
    bool intersectionComplete = false;
    bool booleanComplete = false;
    std::string message;
    double nurbsProjectionTime;       ///< NURBS投影运算时间（ms）
    double quadricProjectionTime;     ///< 二次曲面投影运算时间（ms）
    double nurbsIntersectionTime;     ///< NURBS求交运算时间（ms）
    double quadricIntersectionTime;   ///< 二次曲面求交运算时间（ms）
    double nurbsBooleanTime;          ///< NURBS布尔运算时间（ms）
    double quadricBooleanTime;        ///< 二次曲面布尔运算时间（ms）
    
    /**
     * @brief 计算投影性能提升
     */
    double GetProjectionSpeedup() const {
        return (projectionComplete && std::isfinite(nurbsProjectionTime) && std::isfinite(quadricProjectionTime) && nurbsProjectionTime>0 && quadricProjectionTime>0) ?
            (nurbsProjectionTime-quadricProjectionTime)/nurbsProjectionTime : std::numeric_limits<double>::quiet_NaN();
    }
    
    /**
     * @brief 计算求交性能提升
     */
    double GetIntersectionSpeedup() const {
        return (intersectionComplete && std::isfinite(nurbsIntersectionTime) && std::isfinite(quadricIntersectionTime) && nurbsIntersectionTime>0 && quadricIntersectionTime>0) ?
            (nurbsIntersectionTime-quadricIntersectionTime)/nurbsIntersectionTime : std::numeric_limits<double>::quiet_NaN();
    }
    
    /**
     * @brief 计算布尔运算性能提升
     */
    double GetBooleanSpeedup() const {
        return (booleanComplete && std::isfinite(nurbsBooleanTime) && std::isfinite(quadricBooleanTime) && nurbsBooleanTime>0 && quadricBooleanTime>0) ?
            (nurbsBooleanTime-quadricBooleanTime)/nurbsBooleanTime : std::numeric_limits<double>::quiet_NaN();
    }
    
    /**
     * @brief 构造函数
     */
    PerformanceResult()
        : nurbsProjectionTime(std::numeric_limits<double>::infinity()),
          quadricProjectionTime(std::numeric_limits<double>::infinity()),
          nurbsIntersectionTime(std::numeric_limits<double>::infinity()),
          quadricIntersectionTime(std::numeric_limits<double>::infinity()),
          nurbsBooleanTime(std::numeric_limits<double>::infinity()),
          quadricBooleanTime(std::numeric_limits<double>::infinity()) {}
};

} // namespace BS2Quad

#endif // BS2QUAD_COMMON_H

