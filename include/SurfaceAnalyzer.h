/**
 * @file SurfaceAnalyzer.h
 * @brief NURBS曲面分析和二次曲面兼容性判定模块
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_SURFACE_ANALYZER_H
#define BS2QUAD_SURFACE_ANALYZER_H

#include "Common.h"
#include <Geom_BSplineSurface.hxx>
#include <TopoDS_Face.hxx>
#include <TColgp_Array2OfPnt.hxx>
#include <vector>

namespace BS2Quad {

/**
 * @brief NURBS曲面分析器
 * 
 * 负责分析NURBS曲面的几何特征，判定其是否可以转换为二次曲面
 */
class SurfaceAnalyzer {
public:
    /**
     * @brief 构造函数
     * @param tolerance 几何误差容差
     */
    explicit SurfaceAnalyzer(double tolerance = Precision::DEFAULT_TOLERANCE);
    
    /**
     * @brief 析构函数
     */
    ~SurfaceAnalyzer();
    
    /**
     * @brief 分析NURBS曲面
     * @param surface NURBS B样条曲面
     * @return 分析结果，包含曲面类型和置信度
     */
    SurfaceAnalysisResult Analyze(const Handle(Geom_BSplineSurface)& surface);
    
    /**
     * @brief 分析拓扑面
     * @param face 拓扑面
     * @return 分析结果
     */
    SurfaceAnalysisResult Analyze(const TopoDS_Face& face);
    
    /**
     * @brief 设置误差容差
     * @param tolerance 新的容差值
     */
    void SetTolerance(double tolerance);
    
    /**
     * @brief 获取当前容差
     * @return 当前容差值
     */
    double GetTolerance() const { return m_tolerance; }
    
    /**
     * @brief 设置采样点数
     * @param samplesU U方向采样点数
     * @param samplesV V方向采样点数
     */
    void SetSampleSize(int samplesU, int samplesV);
    
    /**
     * @brief 获取采样点集合
     * @return 采样点数组
     */
    const std::vector<gp_Pnt>& GetSamplePoints() const { return m_samplePoints; }

private:
    /**
     * @brief 验证曲面有效性
     * @param surface NURBS曲面
     * @return 错误类型
     */
    ErrorType ValidateSurface(const Handle(Geom_BSplineSurface)& surface) const;
    
    /**
     * @brief 在曲面上生成采样点
     * @param surface NURBS曲面
     */
    void GenerateSamplePoints(const Handle(Geom_BSplineSurface)& surface);
    
    /**
     * @brief 计算曲面法向量分布
     * @param surface NURBS曲面
     * @return 法向量数组
     */
    std::vector<gp_Vec> ComputeNormalDistribution(const Handle(Geom_BSplineSurface)& surface);
    
    /**
     * @brief 计算曲面曲率分布
     * @param surface NURBS曲面
     * @param gaussianCurvatures 输出：高斯曲率数组
     * @param meanCurvatures 输出：平均曲率数组
     */
    void ComputeCurvatureDistribution(
        const Handle(Geom_BSplineSurface)& surface,
        std::vector<double>& gaussianCurvatures,
        std::vector<double>& meanCurvatures
    );
    
    /**
     * @brief 判定是否为平面
     * @param surface NURBS曲面
     * @param confidence 输出：置信度
     * @return 是否为平面
     */
    bool IsPlanar(const Handle(Geom_BSplineSurface)& surface, double& confidence);
    
    /**
     * @brief 判定是否为圆柱面
     * @param surface NURBS曲面
     * @param confidence 输出：置信度
     * @return 是否为圆柱面
     */
    bool IsCylindrical(const Handle(Geom_BSplineSurface)& surface, double& confidence);
    
    /**
     * @brief 判定是否为圆锥面
     * @param surface NURBS曲面
     * @param confidence 输出：置信度
     * @return 是否为圆锥面
     */
    bool IsConical(const Handle(Geom_BSplineSurface)& surface, double& confidence);
    
    /**
     * @brief 判定是否为球面
     * @param surface NURBS曲面
     * @param confidence 输出：置信度
     * @return 是否为球面
     */
    bool IsSpherical(const Handle(Geom_BSplineSurface)& surface, double& confidence);
    
    /**
     * @brief 判定是否为圆环面
     * @param surface NURBS曲面
     * @param confidence 输出：置信度
     * @return 是否为圆环面
     */
    bool IsToroidal(const Handle(Geom_BSplineSurface)& surface, double& confidence);
    
    /**
     * @brief 检查曲面度数
     * @param surface NURBS曲面
     * @return 是否符合二次曲面度数要求
     */
    bool CheckDegree(const Handle(Geom_BSplineSurface)& surface) const;
    
    /**
     * @brief 计算采样点的统计特征
     * @param values 数值数组
     * @param mean 输出：均值
     * @param stddev 输出：标准差
     */
    void ComputeStatistics(const std::vector<double>& values, double& mean, double& stddev) const;

private:
    double m_tolerance;                 ///< 几何误差容差
    int m_samplesU;                     ///< U方向采样点数
    int m_samplesV;                     ///< V方向采样点数
    std::vector<gp_Pnt> m_samplePoints; ///< 采样点集合
    std::vector<double> m_uParams;      ///< U参数数组
    std::vector<double> m_vParams;      ///< V参数数组
};

} // namespace BS2Quad

#endif // BS2QUAD_SURFACE_ANALYZER_H

