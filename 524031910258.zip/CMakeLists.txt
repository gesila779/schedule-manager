cmake_minimum_required(VERSION 3.12)
project(MySchedule VERSION 1.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows特定设置
if(WIN32)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/permissive->")
endif()

# 配置可执行文件
add_executable(MySchedule
    main.cpp  # 假设代码保存在main.cpp文件中
)

# 链接Windows库
if(WIN32)
    target_link_libraries(MySchedule PRIVATE winmm.lib)
endif()

# 设置输出目录
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/bin)

# 安装目标
install(TARGETS MySchedule
    RUNTIME DESTINATION bin
)

# 添加用户文档文件
configure_file(README.txt README.txt COPYONLY)

# 添加测试支持
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# 生成版本信息
include(CMakePrintHelpers)
cmake_print_variables(CMAKE_CXX_COMPILER CMAKE_CXX_COMPILER_VERSION)