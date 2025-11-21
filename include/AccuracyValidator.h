/**
 * @file AccuracyValidator.h
 * @brief 精度验证和性能对比模块
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_ACCURACY_VALIDATOR_H
#define BS2QUAD_ACCURACY_VALIDATOR_H

#include "Common.h"
#include <Geom_Surface.hxx>
#include <Geom_BSplineSurface.hxx>
#include <TopoDS_Face.hxx>
#include <TopoDS_Edge.hxx>
#include <vector>

namespace BS2Quad {

/**
 * @brief 精度验证器
 * 
 * 负责验证转换后二次曲面的精度，并对比NURBS曲面和二次曲面的性能
 */
class AccuracyValidator {
public:
    /**
     * @brief 构造函数
     * @param tolerance 几何误差容差
     */
    explicit AccuracyValidator(double tolerance = Precision::DEFAULT_TOLERANCE);
    
    /**
     * @brief 析构函数
     */
    ~AccuracyValidator();
    
    /**
     * @brief 验证转换精度
     * @param nurbsSurface 原始NURBS曲面
     * @param quadricSurface 转换后的二次曲面
     * @return 精度验证结果
     */
    AccuracyResult ValidateAccuracy(
        const Handle(Geom_BSplineSurface)& nurbsSurface,
        const Handle(Geom_Surface)& quadricSurface
    );
    
    /**
     * @brief 验证拓扑面转换精度
     * @param nurbsFace 原始NURBS拓扑面
     * @param quadricFace 转换后的二次曲面拓扑面
     * @return 精度验证结果
     */
    AccuracyResult ValidateAccuracy(
        const TopoDS_Face& nurbsFace,
        const TopoDS_Face& quadricFace
    );
    
    /**
     * @brief 性能对比测试
     * @param nurbsSurface NURBS曲面
     * @param quadricSurface 二次曲面
     * @return 性能对比结果
     */
    PerformanceResult ComparePerformance(
        const Handle(Geom_BSplineSurface)& nurbsSurface,
        const Handle(Geom_Surface)& quadricSurface
    );
    
    /**
     * @brief 性能对比测试（拓扑面）
     * @param nurbsFace NURBS拓扑面
     * @param quadricFace 二次曲面拓扑面
     * @return 性能对比结果
     */
    PerformanceResult ComparePerformance(
        const TopoDS_Face& nurbsFace,
        const TopoDS_Face& quadricFace
    );
    
    /**
     * @brief 设置容差
     * @param tolerance 新的容差值
     */
    void SetTolerance(double tolerance) { m_tolerance = tolerance; }
    
    /**
     * @brief 获取容差
     * @return 当前容差值
     */
    double GetTolerance() const { return m_tolerance; }
    
    /**
     * @brief 设置性能测试迭代次数
     * @param iterations 迭代次数
     */
    void SetPerformanceIterations(int iterations) { m_perfIterations = iterations; }

private:
    /**
     * @brief 计算最大几何距离误差
     * @param nurbsSurface NURBS曲面
     * @param quadricSurface 二次曲面
     * @param samplePoints 采样点数组
     * @return 最大距离
     */
    double ComputeMaxDistance(
        const Handle(Geom_BSplineSurface)& nurbsSurface,
        const Handle(Geom_Surface)& quadricSurface,
        const std::vector<gp_Pnt>& samplePoints
    );
    
    /**
     * @brief 计算平均距离误差
     * @param nurbsSurface NURBS曲面
     * @param quadricSurface 二次曲面
     * @param samplePoints 采样点数组
     * @return 平均距离
     */
    double ComputeAvgDistance(
        const Handle(Geom_BSplineSurface)& nurbsSurface,
        const Handle(Geom_Surface)& quadricSurface,
        const std::vector<gp_Pnt>& samplePoints
    );
    
    /**
     * @brief 计算RMS距离误差
     * @param nurbsSurface NURBS曲面
     * @param quadricSurface 二次曲面
     * @param samplePoints 采样点数组
     * @return RMS距离
     */
    double ComputeRMSDistance(
        const Handle(Geom_BSplineSurface)& nurbsSurface,
        const Handle(Geom_Surface)& quadricSurface,
        const std::vector<gp_Pnt>& samplePoints
    );
    
    /**
     * @brief 计算Hausdorff距离（边界曲线）
     * @param nurbsFace NURBS拓扑面
     * @param quadricFace 二次曲面拓扑面
     * @return Hausdorff距离
     */
    double ComputeHausdorffDistance(
        const TopoDS_Face& nurbsFace,
        const TopoDS_Face& quadricFace
    );
    
    /**
     * @brief 计算边界曲线的Hausdorff距离
     * @param edge1 边界曲线1
     * @param edge2 边界曲线2
     * @return Hausdorff距离
     */
    double ComputeEdgeHausdorffDistance(
        const TopoDS_Edge& edge1,
        const TopoDS_Edge& edge2
    );
    
    /**
     * @brief 验证G1连续性
     * @param nurbsFace NURBS拓扑面
     * @param quadricFace 二次曲面拓扑面
     * @return G1连续性误差（法向量夹角，度）
     */
    double ValidateG1Continuity(
        const TopoDS_Face& nurbsFace,
        const TopoDS_Face& quadricFace
    );
    
    /**
     * @brief 在曲面上采样点
     * @param surface 曲面
     * @param samplesU U方向采样数
     * @param samplesV V方向采样数
     * @return 采样点数组
     */
    std::vector<gp_Pnt> SampleSurface(
        const Handle(Geom_Surface)& surface,
        int samplesU = Sampling::ADAPTIVE_SAMPLES,
        int samplesV = Sampling::ADAPTIVE_SAMPLES
    );
    
    /**
     * @brief 测试投影运算性能
     * @param surface 曲面
     * @param testPoints 测试点集
     * @return 平均耗时（毫秒）
     */
    double BenchmarkProjection(
        const Handle(Geom_Surface)& surface,
        const std::vector<gp_Pnt>& testPoints
    );
    
    /**
     * @brief 测试求交运算性能
     * @param surface 曲面
     * @return 平均耗时（毫秒）
     */
    double BenchmarkIntersection(const Handle(Geom_Surface)& surface);
    
    /**
     * @brief 测试布尔运算性能
     * @param face 拓扑面
     * @return 平均耗时（毫秒）
     */
    double BenchmarkBoolean(const TopoDS_Face& face);
    
    /**
     * @brief 获取当前时间（毫秒）
     * @return 时间戳
     */
    double GetTimeInMilliseconds() const;

private:
    double m_tolerance;       ///< 几何误差容差
    int m_perfIterations;     ///< 性能测试迭代次数
    int m_samplesU;           ///< U方向采样点数
    int m_samplesV;           ///< V方向采样点数
};

} // namespace BS2Quad

#endif // BS2QUAD_ACCURACY_VALIDATOR_H

