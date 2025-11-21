/**
 * @file SurfaceAnalyzer.cpp
 * @brief NURBS曲面分析和二次曲面兼容性判定模块实现
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "SurfaceAnalyzer.h"
#include <BRep_Tool.hxx>
#include <GeomLProp_SLProps.hxx>
#include <cmath>
#include <algorithm>

namespace BS2Quad {

// 构造函数
SurfaceAnalyzer::SurfaceAnalyzer(double tolerance)
    : m_tolerance(tolerance)
    , m_samplesU(Sampling::ADAPTIVE_SAMPLES)
    , m_samplesV(Sampling::ADAPTIVE_SAMPLES)
{
}

// 析构函数
SurfaceAnalyzer::~SurfaceAnalyzer() {
}

// 分析NURBS曲面
SurfaceAnalysisResult SurfaceAnalyzer::Analyze(
    const Handle(Geom_BSplineSurface)& surface
) {
    SurfaceAnalysisResult result;
    
    if (surface.IsNull()) {
        result.error = ErrorType::DATA_CORRUPT;
        result.message = "输入曲面为空";
        return result;
    }
    
    // 验证曲面有效性
    ErrorType validationError = ValidateSurface(surface);
    if (validationError != ErrorType::SUCCESS) {
        result.error = validationError;
        result.message = ErrorTypeToString(validationError);
        return result;
    }
    
    // 生成采样点
    GenerateSamplePoints(surface);
    
    // 按优先级检测曲面类型
    double confidence = 0.0;
    
    // 1. 检测平面
    if (IsPlanar(surface, confidence)) {
        result.type = QuadricType::PLANE;
        result.confidence = confidence;
        result.error = ErrorType::SUCCESS;
        result.message = "检测到平面";
        return result;
    }
    
    // 2. 检测圆柱面
    if (IsCylindrical(surface, confidence)) {
        result.type = QuadricType::CYLINDER;
        result.confidence = confidence;
        result.error = ErrorType::SUCCESS;
        result.message = "检测到圆柱面";
        return result;
    }
    
    // 3. 检测球面
    if (IsSpherical(surface, confidence)) {
        result.type = QuadricType::SPHERE;
        result.confidence = confidence;
        result.error = ErrorType::SUCCESS;
        result.message = "检测到球面";
        return result;
    }
    
    // 4. 检测圆锥面
    if (IsConical(surface, confidence)) {
        result.type = QuadricType::CONE;
        result.confidence = confidence;
        result.error = ErrorType::SUCCESS;
        result.message = "检测到圆锥面";
        return result;
    }
    
    // 5. 检测圆环面
    if (IsToroidal(surface, confidence)) {
        result.type = QuadricType::TORUS;
        result.confidence = confidence;
        result.error = ErrorType::SUCCESS;
        result.message = "检测到圆环面";
        return result;
    }
    
    // 无法识别为二次曲面
    result.type = QuadricType::NONE;
    result.confidence = 0.0;
    result.error = ErrorType::NON_QUADRIC;
    result.message = "无法识别为二次曲面";
    
    return result;
}

// 分析拓扑面
SurfaceAnalysisResult SurfaceAnalyzer::Analyze(const TopoDS_Face& face) {
    if (face.IsNull()) {
        SurfaceAnalysisResult result;
        result.error = ErrorType::DATA_CORRUPT;
        result.message = "输入拓扑面为空";
        return result;
    }
    
    // 提取底层几何曲面
    Handle(Geom_Surface) surface = BRep_Tool::Surface(face);
    
    // 尝试转换为B样条曲面
    Handle(Geom_BSplineSurface) bsplineSurface = 
        Handle(Geom_BSplineSurface)::DownCast(surface);
    
    if (bsplineSurface.IsNull()) {
        SurfaceAnalysisResult result;
        result.error = ErrorType::DATA_CORRUPT;
        result.message = "无法转换为B样条曲面";
        return result;
    }
    
    return Analyze(bsplineSurface);
}

// 设置误差容差
void SurfaceAnalyzer::SetTolerance(double tolerance) {
    m_tolerance = tolerance;
}

// 设置采样点数
void SurfaceAnalyzer::SetSampleSize(int samplesU, int samplesV) {
    m_samplesU = std::max(Sampling::MIN_SAMPLES_U, 
                          std::min(samplesU, Sampling::MAX_SAMPLES_U));
    m_samplesV = std::max(Sampling::MIN_SAMPLES_V,
                          std::min(samplesV, Sampling::MAX_SAMPLES_V));
}

// 验证曲面有效性
ErrorType SurfaceAnalyzer::ValidateSurface(
    const Handle(Geom_BSplineSurface)& surface
) const {
    // 检查控制点数
    int numPolesU = surface->NbUPoles();
    int numPolesV = surface->NbVPoles();
    
    if (numPolesU < 2 || numPolesV < 2) {
        return ErrorType::DATA_CORRUPT;
    }
    
    // 检查度数
    int degreeU = surface->UDegree();
    int degreeV = surface->VDegree();
    
    if (degreeU < 1 || degreeV < 1) {
        return ErrorType::DATA_CORRUPT;
    }
    
    // 检查参数域
    double uMin, uMax, vMin, vMax;
    surface->Bounds(uMin, uMax, vMin, vMax);
    
    if (uMax <= uMin || vMax <= vMin) {
        return ErrorType::OUT_OF_RANGE;
    }
    
    return ErrorType::SUCCESS;
}

// 在曲面上生成采样点
void SurfaceAnalyzer::GenerateSamplePoints(
    const Handle(Geom_BSplineSurface)& surface
) {
    m_samplePoints.clear();
    m_uParams.clear();
    m_vParams.clear();
    
    // 获取参数域
    double uMin, uMax, vMin, vMax;
    surface->Bounds(uMin, uMax, vMin, vMax);
    
    // 生成参数值
    for (int i = 0; i < m_samplesU; ++i) {
        double u = uMin + (uMax - uMin) * i / (m_samplesU - 1);
        m_uParams.push_back(u);
    }
    
    for (int j = 0; j < m_samplesV; ++j) {
        double v = vMin + (vMax - vMin) * j / (m_samplesV - 1);
        m_vParams.push_back(v);
    }
    
    // 采样点
    for (double u : m_uParams) {
        for (double v : m_vParams) {
            try {
                gp_Pnt point = surface->Value(u, v);
                m_samplePoints.push_back(point);
            } catch (...) {
                // 忽略无效参数点
            }
        }
    }
}

// 计算曲面法向量分布
std::vector<gp_Vec> SurfaceAnalyzer::ComputeNormalDistribution(
    const Handle(Geom_BSplineSurface)& surface
) {
    std::vector<gp_Vec> normals;
    
    for (double u : m_uParams) {
        for (double v : m_vParams) {
            try {
                GeomLProp_SLProps props(surface, u, v, 1, Precision::DEFAULT_TOLERANCE);
                if (props.IsNormalDefined()) {
                    normals.push_back(gp_Vec(props.Normal()));
                }
            } catch (...) {
                // 忽略无法计算法向量的点
            }
        }
    }
    
    return normals;
}

// 计算曲面曲率分布
void SurfaceAnalyzer::ComputeCurvatureDistribution(
    const Handle(Geom_BSplineSurface)& surface,
    std::vector<double>& gaussianCurvatures,
    std::vector<double>& meanCurvatures
) {
    gaussianCurvatures.clear();
    meanCurvatures.clear();
    
    for (double u : m_uParams) {
        for (double v : m_vParams) {
            try {
                GeomLProp_SLProps props(surface, u, v, 2, Precision::DEFAULT_TOLERANCE);
                
                if (props.IsCurvatureDefined()) {
                    gaussianCurvatures.push_back(props.GaussianCurvature());
                    meanCurvatures.push_back(props.MeanCurvature());
                }
            } catch (...) {
                // 忽略无法计算曲率的点
            }
        }
    }
}

// 判定是否为平面
bool SurfaceAnalyzer::IsPlanar(
    const Handle(Geom_BSplineSurface)& surface,
    double& confidence
) {
    if (m_samplePoints.size() < 3) {
        confidence = 0.0;
        return false;
    }
    
    // 计算法向量分布
    std::vector<gp_Vec> normals = ComputeNormalDistribution(surface);
    
    if (normals.empty()) {
        confidence = 0.0;
        return false;
    }
    
    // 检查法向量是否一致
    gp_Vec avgNormal(0, 0, 0);
    for (const auto& normal : normals) {
        avgNormal += normal;
    }
    avgNormal.Normalize();
    
    // 计算法向量方差
    double variance = 0.0;
    for (const auto& normal : normals) {
        double angle = std::abs(avgNormal.Angle(normal));
        variance += angle * angle;
    }
    variance /= normals.size();
    
    // 判断是否为平面：法向量方差小
    double angleStd = std::sqrt(variance);
    confidence = std::max(0.0, 1.0 - angleStd / Precision::ANGLE_TOLERANCE_RAD);
    
    return angleStd < Precision::ANGLE_TOLERANCE_RAD;
}

// 判定是否为圆柱面
bool SurfaceAnalyzer::IsCylindrical(
    const Handle(Geom_BSplineSurface)& surface,
    double& confidence
) {
    // 计算曲率分布
    std::vector<double> gaussianCurvatures, meanCurvatures;
    ComputeCurvatureDistribution(surface, gaussianCurvatures, meanCurvatures);
    
    if (gaussianCurvatures.empty()) {
        confidence = 0.0;
        return false;
    }
    
    // 圆柱面特征：高斯曲率接近0，平均曲率恒定
    double gaussianMean = 0.0;
    for (double k : gaussianCurvatures) {
        gaussianMean += std::abs(k);
    }
    gaussianMean /= gaussianCurvatures.size();
    
    // 计算平均曲率的方差
    double meanCurvMean, meanCurvStd;
    ComputeStatistics(meanCurvatures, meanCurvMean, meanCurvStd);
    
    // 判断：高斯曲率接近0，平均曲率方差小
    bool isGaussianZero = gaussianMean < Precision::CURVATURE_TOLERANCE;
    bool isMeanConstant = meanCurvStd < Precision::CURVATURE_TOLERANCE;
    
    if (isGaussianZero && isMeanConstant) {
        confidence = std::max(0.0, 1.0 - gaussianMean / Precision::CURVATURE_TOLERANCE);
        return true;
    }
    
    confidence = 0.0;
    return false;
}

// 判定是否为圆锥面
bool SurfaceAnalyzer::IsConical(
    const Handle(Geom_BSplineSurface)& surface,
    double& confidence
) {
    // 计算曲率分布
    std::vector<double> gaussianCurvatures, meanCurvatures;
    ComputeCurvatureDistribution(surface, gaussianCurvatures, meanCurvatures);
    
    if (gaussianCurvatures.empty()) {
        confidence = 0.0;
        return false;
    }
    
    // 圆锥面特征：高斯曲率为0
    double gaussianMean = 0.0;
    for (double k : gaussianCurvatures) {
        gaussianMean += std::abs(k);
    }
    gaussianMean /= gaussianCurvatures.size();
    
    // 判断：高斯曲率接近0，但平均曲率沿一个方向变化
    if (gaussianMean < Precision::CURVATURE_TOLERANCE) {
        confidence = std::max(0.0, 1.0 - gaussianMean / Precision::CURVATURE_TOLERANCE);
        return true;
    }
    
    confidence = 0.0;
    return false;
}

// 判定是否为球面
bool SurfaceAnalyzer::IsSpherical(
    const Handle(Geom_BSplineSurface)& surface,
    double& confidence
) {
    // 计算曲率分布
    std::vector<double> gaussianCurvatures, meanCurvatures;
    ComputeCurvatureDistribution(surface, gaussianCurvatures, meanCurvatures);
    
    if (gaussianCurvatures.empty()) {
        confidence = 0.0;
        return false;
    }
    
    // 球面特征：高斯曲率恒定且为正，平均曲率恒定
    double gaussianMean, gaussianStd;
    ComputeStatistics(gaussianCurvatures, gaussianMean, gaussianStd);
    
    double meanCurvMean, meanCurvStd;
    ComputeStatistics(meanCurvatures, meanCurvMean, meanCurvStd);
    
    // 判断：高斯曲率为正且恒定，平均曲率恒定
    bool isGaussianPositive = gaussianMean > Precision::CURVATURE_TOLERANCE;
    bool isGaussianConstant = gaussianStd < Precision::CURVATURE_TOLERANCE;
    bool isMeanConstant = meanCurvStd < Precision::CURVATURE_TOLERANCE;
    
    if (isGaussianPositive && isGaussianConstant && isMeanConstant) {
        confidence = std::max(0.0, 1.0 - gaussianStd / gaussianMean);
        return true;
    }
    
    confidence = 0.0;
    return false;
}

// 判定是否为圆环面
bool SurfaceAnalyzer::IsToroidal(
    const Handle(Geom_BSplineSurface)& surface,
    double& confidence
) {
    // 圆环面检测较复杂，简化实现
    // 特征：曲率在特定模式下变化
    
    std::vector<double> gaussianCurvatures, meanCurvatures;
    ComputeCurvatureDistribution(surface, gaussianCurvatures, meanCurvatures);
    
    if (gaussianCurvatures.empty()) {
        confidence = 0.0;
        return false;
    }
    
    // 简单判断：曲率分布特征
    double gaussianMean, gaussianStd;
    ComputeStatistics(gaussianCurvatures, gaussianMean, gaussianStd);
    
    // 圆环面的高斯曲率有正有负
    bool hasPositive = false;
    bool hasNegative = false;
    
    for (double k : gaussianCurvatures) {
        if (k > Precision::CURVATURE_TOLERANCE) hasPositive = true;
        if (k < -Precision::CURVATURE_TOLERANCE) hasNegative = true;
    }
    
    if (hasPositive && hasNegative) {
        confidence = 0.5;  // 中等置信度
        return true;
    }
    
    confidence = 0.0;
    return false;
}

// 检查曲面度数
bool SurfaceAnalyzer::CheckDegree(
    const Handle(Geom_BSplineSurface)& surface
) const {
    int degreeU = surface->UDegree();
    int degreeV = surface->VDegree();
    
    // 二次曲面通常度数不超过2
    return (degreeU <= 2 && degreeV <= 2);
}

// 计算采样点的统计特征
void SurfaceAnalyzer::ComputeStatistics(
    const std::vector<double>& values,
    double& mean,
    double& stddev
) const {
    if (values.empty()) {
        mean = 0.0;
        stddev = 0.0;
        return;
    }
    
    // 计算均值
    mean = 0.0;
    for (double val : values) {
        mean += val;
    }
    mean /= values.size();
    
    // 计算标准差
    double variance = 0.0;
    for (double val : values) {
        double diff = val - mean;
        variance += diff * diff;
    }
    variance /= values.size();
    stddev = std::sqrt(variance);
}

} // namespace BS2Quad

