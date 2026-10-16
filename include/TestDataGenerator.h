/**
 * @file TestDataGenerator.h
 * @brief 测试数据生成器
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#ifndef BS2QUAD_TEST_DATA_GENERATOR_H
#define BS2QUAD_TEST_DATA_GENERATOR_H

#include "Common.h"
#include <Geom_BSplineSurface.hxx>
#include <TopoDS_Face.hxx>
#include <string>
#include <vector>
#include <random>
#include <TColgp_Array2OfPnt.hxx>
#include <TColStd_Array2OfReal.hxx>
#include <TColStd_Array1OfReal.hxx>

namespace BS2Quad {

/**
 * @brief 测试数据配置
 */
struct TestDataConfig {
    QuadricType surfaceType;     ///< 曲面类型
    int numControlPointsU;       ///< U方向控制点数
    int numControlPointsV;       ///< V方向控制点数
    double noiseLevel;           ///< 噪声级别（0.0-1.0）
    bool exportToFile;           ///< 是否导出到文件
    std::string filename;        ///< 文件名
    
    /**
     * @brief 构造函数
     */
    TestDataConfig()
        : surfaceType(QuadricType::CYLINDER),
          numControlPointsU(10),
          numControlPointsV(10),
          noiseLevel(0.0),
          exportToFile(false),
          filename("") {}
};

/**
 * @brief 测试数据生成器
 * 
 * 生成各种类型的NURBS曲面测试数据，用于算法测试和验证
 */
class TestDataGenerator {
public:
    /**
     * @brief 构造函数
     */
    TestDataGenerator();
    
    /**
     * @brief 析构函数
     */
    ~TestDataGenerator();
    
    /**
     * @brief 生成标准圆柱面NURBS
     * @param radius 半径
     * @param height 高度
     * @param numPointsU U方向控制点数
     * @param numPointsV V方向控制点数
     * @return NURBS曲面
     */
    Handle(Geom_BSplineSurface) GenerateCylinderSurface(
        double radius,
        double height,
        int numPointsU = 10,
        int numPointsV = 10
    );
    
    /**
     * @brief 生成标准圆锥面NURBS
     * @param radius1 底面半径
     * @param radius2 顶面半径
     * @param height 高度
     * @param numPointsU U方向控制点数
     * @param numPointsV V方向控制点数
     * @return NURBS曲面
     */
    Handle(Geom_BSplineSurface) GenerateConeSurface(
        double radius1,
        double radius2,
        double height,
        int numPointsU = 10,
        int numPointsV = 10
    );
    
    /**
     * @brief 生成标准球面NURBS
     * @param radius 半径
     * @param numPointsU U方向控制点数
     * @param numPointsV V方向控制点数
     * @return NURBS曲面
     */
    Handle(Geom_BSplineSurface) GenerateSphereSurface(
        double radius,
        int numPointsU = 10,
        int numPointsV = 10
    );
    
    /**
     * @brief 生成标准平面NURBS
     * @param width 宽度
     * @param length 长度
     * @param numPointsU U方向控制点数
     * @param numPointsV V方向控制点数
     * @return NURBS曲面
     */
    Handle(Geom_BSplineSurface) GeneratePlaneSurface(
        double width,
        double length,
        int numPointsU = 5,
        int numPointsV = 5
    );
    
    /**
     * @brief 生成标准圆环面NURBS
     * @param majorRadius 主半径
     * @param minorRadius 副半径
     * @param numPointsU U方向控制点数
     * @param numPointsV V方向控制点数
     * @return NURBS曲面
     */
    Handle(Geom_BSplineSurface) GenerateTorusSurface(
        double majorRadius,
        double minorRadius,
        int numPointsU = 10,
        int numPointsV = 10
    );
    
    /**
     * @brief 生成带噪声的曲面
     * @param surface 原始曲面
     * @param noiseLevel 控制点坐标噪声幅度（绝对值，mm）
     * @return 带噪声的NURBS曲面
     */
    Handle(Geom_BSplineSurface) AddNoiseToSurface(
        const Handle(Geom_BSplineSurface)& surface,
        double noiseLevel
    );
    
    /**
     * @brief 生成复杂自由曲面（负样本）
     * @param numPointsU U方向控制点数
     * @param numPointsV V方向控制点数
     * @return NURBS曲面
     */
    Handle(Geom_BSplineSurface) GenerateFreeFormSurface(
        int numPointsU = 10,
        int numPointsV = 10
    );
    
    /**
     * @brief 批量生成测试数据
     * @param count 生成数量
     * @param outputDir 输出目录
     * @return 生成的曲面列表
     */
    std::vector<Handle(Geom_BSplineSurface)> GenerateBatchTestData(
        int count,
        const std::string& outputDir = "data/generated"
    );
    
    /**
     * @brief 导出曲面到STEP文件
     * @param surface NURBS曲面
     * @param filename 文件名
     * @return 是否成功导出
     */
    bool ExportToSTEP(
        const Handle(Geom_BSplineSurface)& surface,
        const std::string& filename
    );
    
    /**
     * @brief 导出曲面到IGES文件
     * @param surface NURBS曲面
     * @param filename 文件名
     * @return 是否成功导出
     */
    bool ExportToIGES(
        const Handle(Geom_BSplineSurface)& surface,
        const std::string& filename
    );
    
    /**
     * @brief 从STEP文件导入曲面
     * @param filename 文件名
     * @return NURBS曲面（如果失败返回NULL）
     */
    Handle(Geom_BSplineSurface) ImportFromSTEP(const std::string& filename);
    
    /**
     * @brief 设置随机种子
     * @param seed 随机种子
     */
    void SetRandomSeed(unsigned int seed);

private:
    /**
     * @brief 创建NURBS曲面
     * @param controlPoints 控制点数组
     * @param weights 权重数组
     * @param uKnots U方向节点向量
     * @param vKnots V方向节点向量
     * @param uDegree U方向度数
     * @param vDegree V方向度数
     * @return NURBS曲面
     */
    Handle(Geom_BSplineSurface) CreateBSplineSurface(
        const TColgp_Array2OfPnt& controlPoints,
        const TColStd_Array2OfReal& weights,
        const TColStd_Array1OfReal& uKnots,
        const TColStd_Array1OfReal& vKnots,
        int uDegree,
        int vDegree
    );
    
    /**
     * @brief 生成均匀节点向量
     * @param numControlPoints 控制点数
     * @param degree 度数
     * @return 节点向量数组
     */
    TColStd_Array1OfReal GenerateUniformKnots(int numControlPoints, int degree);
    
    /**
     * @brief 生成随机噪声
     * @param magnitude 噪声幅度
     * @return 噪声向量
     */
    gp_Vec GenerateRandomNoise(double magnitude);

private:
    std::mt19937 m_rng;
    unsigned int m_randomSeed;  ///< 随机种子
};

} // namespace BS2Quad

#endif // BS2QUAD_TEST_DATA_GENERATOR_H

