/**
 * @file test_exception_handler.cpp
 * @brief 异常处理器单元测试
 * @author BS2Quad Team
 * @date 2025-11-21
 */

#include "ExceptionHandler.h"
#include "TestDataGenerator.h"
#include <gtest/gtest.h>

using namespace BS2Quad;

/**
 * @brief 异常处理器测试套件
 */
class ExceptionHandlerTest : public ::testing::Test {
protected:
    void SetUp() override {
        handler = std::make_unique<ExceptionHandler>();
        generator = std::make_unique<TestDataGenerator>();
    }
    
    std::unique_ptr<ExceptionHandler> handler;
    std::unique_ptr<TestDataGenerator> generator;
};

TEST_F(ExceptionHandlerTest, DetectNullSurface) {
    Handle(Geom_BSplineSurface) nullSurface;
    
    ExceptionInfo info = handler->DetectExceptions(nullSurface);
    
    EXPECT_EQ(info.errorType, ErrorType::DATA_CORRUPT);
    EXPECT_FALSE(info.canSimplify);
}

TEST_F(ExceptionHandlerTest, DetectFreeFormSurface) {
    auto freeForm = generator->GenerateFreeFormSurface(10, 10);
    
    ExceptionInfo info = handler->DetectExceptions(freeForm);
    
    EXPECT_EQ(info.errorType, ErrorType::NON_QUADRIC);
    EXPECT_TRUE(info.canSimplify);
}

TEST_F(ExceptionHandlerTest, GenerateErrorReport) {
    ExceptionInfo info;
    info.errorType = ErrorType::HIGH_ERROR;
    info.message = "测试错误";
    info.canSimplify = true;
    
    std::string report = handler->GenerateErrorReport(info);
    
    EXPECT_FALSE(report.empty());
    EXPECT_NE(report.find("HIGH_ERROR"), std::string::npos);
}

// 主函数

