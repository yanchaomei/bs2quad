cmake_minimum_required(VERSION 3.15)
project(BS2Quad VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 设置构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 编译选项
if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        add_compile_options(-O3)
    endif()
endif()

# 查找OpenCASCADE
# OpenCASCADE的安装路径可能需要手动指定
# 例如：cmake -DCMAKE_PREFIX_PATH=/path/to/opencascade ..
find_package(OpenCASCADE 7.7.0 REQUIRED)

if(OpenCASCADE_FOUND)
    message(STATUS "OpenCASCADE found: ${OpenCASCADE_DIR}")
    message(STATUS "OpenCASCADE version: ${OpenCASCADE_VERSION}")
    include_directories(${OpenCASCADE_INCLUDE_DIR})
else()
    message(FATAL_ERROR "OpenCASCADE not found! Please install OpenCASCADE 7.7.0 or later.")
endif()

# 查找OpenMP（用于多线程优化）
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    message(STATUS "OpenMP found")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# 源文件
set(BS2QUAD_SOURCES
    src/BS2QuadCore.cpp
    src/SurfaceAnalyzer.cpp
    src/QuadricFitter.cpp
    src/CylinderFitter.cpp
    src/ConeFitter.cpp
    src/SphereFitter.cpp
    src/PlaneFitter.cpp
    src/TorusFitter.cpp
    src/AccuracyValidator.cpp
    src/ExceptionHandler.cpp
    src/TestDataGenerator.cpp
)

# 头文件
set(BS2QUAD_HEADERS
    include/BS2QuadCore.h
    include/SurfaceAnalyzer.h
    include/QuadricFitter.h
    include/CylinderFitter.h
    include/ConeFitter.h
    include/SphereFitter.h
    include/PlaneFitter.h
    include/TorusFitter.h
    include/AccuracyValidator.h
    include/ExceptionHandler.h
    include/TestDataGenerator.h
    include/Common.h
)

# 创建主库
add_library(bs2quad_lib STATIC ${BS2QUAD_SOURCES} ${BS2QUAD_HEADERS})

# 链接OpenCASCADE库
target_link_libraries(bs2quad_lib
    TKernel
    TKMath
    TKG3d
    TKGeomBase
    TKGeomAlgo
    TKBRep
    TKTopAlgo
    TKPrim
    TKBO
    TKIGES
    TKSTEP
)

if(OpenMP_CXX_FOUND)
    target_link_libraries(bs2quad_lib OpenMP::OpenMP_CXX)
endif()

# 创建主程序
add_executable(bs2quad_main src/main.cpp)
target_link_libraries(bs2quad_main bs2quad_lib)

# 测试
enable_testing()

# 查找Google Test
find_package(GTest)
if(GTest_FOUND)
    message(STATUS "Google Test found")
    
    # 单元测试
    add_executable(unit_tests
        tests/unit_tests/test_surface_analyzer.cpp
        tests/unit_tests/test_quadric_fitter.cpp
        tests/unit_tests/test_accuracy_validator.cpp
        tests/unit_tests/test_exception_handler.cpp
    )
    target_link_libraries(unit_tests bs2quad_lib GTest::GTest GTest::Main)
    add_test(NAME UnitTests COMMAND unit_tests)
    
    # 性能测试
    add_executable(performance_tests
        tests/performance_tests/test_conversion_performance.cpp
        tests/performance_tests/test_cad_operations.cpp
        tests/performance_tests/test_stability.cpp
    )
    target_link_libraries(performance_tests bs2quad_lib GTest::GTest GTest::Main)
    add_test(NAME PerformanceTests COMMAND performance_tests)
else()
    message(WARNING "Google Test not found. Tests will not be built.")
endif()

# 安装规则
install(TARGETS bs2quad_lib bs2quad_main
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    RUNTIME DESTINATION bin
)

install(FILES ${BS2QUAD_HEADERS} DESTINATION include/bs2quad)

# 文档生成（使用Doxygen）
find_package(Doxygen)
if(Doxygen_FOUND)
    set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
    set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)
    
    if(EXISTS ${DOXYGEN_IN})
        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)
        add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
        )
    endif()
endif()

# 打印配置信息
message(STATUS "==================================")
message(STATUS "BS2Quad Configuration Summary")
message(STATUS "==================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ standard: C++${CMAKE_CXX_STANDARD}")
message(STATUS "OpenCASCADE: ${OpenCASCADE_VERSION}")
message(STATUS "OpenMP: ${OpenMP_CXX_FOUND}")
message(STATUS "GTest: ${GTest_FOUND}")
message(STATUS "Doxygen: ${Doxygen_FOUND}")
message(STATUS "==================================")

