cmake_minimum_required(VERSION 3.1.0)
project(HLSL2ALL VERSION 0.1.0)
# C++17
set (CMAKE_CXX_STANDARD 17)
# Option to enable or disable HLSL Test
option(ENABLE_HLSL2ALL_TEST "Enable building of the HLSLTEST target" ON)

#add debug prefix
if(NOT CMAKE_DEBUG_POSTFIX)
  set(CMAKE_DEBUG_POSTFIX d)
endif()
#flags
if(MSVC)
  set(CMAKE_CXX_FLAGS_RELEASE "/Ox")
  set(CMAKE_CXX_FLAGS_DEBUG " /Zi /Od /MDd")
elseif(APPLE)
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3")
else()
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fPIC -pthread")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -fPIC -pthread")
endif()
# HLSL2ALL
set(SOURCE_FILES source/SourceToSpirv.cpp source/SpirvToSource.cpp)
set(HEADER_FILES include/HLSL2ALL/HLSL2ALL.h include/HLSL2ALL/HLSL2ALLConfig.h include/HLSL2ALL/SourceToSpirv.h include/HLSL2ALL/SpirvToSource.h)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${HEADER_FILES} ) 
add_definitions(-DHLSL2ALL_LIBRARY_EXPORT)
target_include_directories(${PROJECT_NAME} PUBLIC include)
# macOS, rpath
if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_WITH_INSTALL_RPATH 1 INSTALL_NAME_DIR "@rpath")
endif()

# GLSLANG
set(GLSLANG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/glslang")

# FLAGS
#if(MSVC)
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /DENABLE_OPT" )
#else()
#  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DENABLE_OPT" )
#endif()

add_subdirectory(${GLSLANG_DIR})
set(glslang_libraries glslang HLSL OSDependent OGLCompiler SPIRV glslang-default-resource-limits)

target_link_libraries(${PROJECT_NAME} PRIVATE ${glslang_libraries})
target_include_directories(${PROJECT_NAME} PUBLIC ${GLSLANG_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC "${GLSLANG_DIR}/glslang/Include")
target_include_directories(${PROJECT_NAME} PUBLIC "${GLSLANG_DIR}/glslang/Public")
target_include_directories(${PROJECT_NAME} PUBLIC "${GLSLANG_DIR}/hlsl")
target_include_directories(${PROJECT_NAME} PUBLIC "${GLSLANG_DIR}/SPIRV")
target_include_directories(${PROJECT_NAME} PUBLIC "${GLSLANG_DIR}/OGLCompilersDLL")
target_include_directories(${PROJECT_NAME} PUBLIC "${GLSLANG_DIR}/glslang/OSDependent")

# SPIRVCROSS
set(SPIRVCROSS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/SPIRV-Cross")
add_subdirectory(${SPIRVCROSS_DIR})
set(spirv_cross_libraries spirv-cross-core spirv-cross-reflect spirv-cross-util spirv-cross-glsl spirv-cross-hlsl)

target_link_libraries(${PROJECT_NAME} PRIVATE ${spirv_cross_libraries})
target_include_directories(${PROJECT_NAME} PUBLIC ${SPIRVCROSS_DIR})

# HLSL Test
if(ENABLE_HLSL2ALL_TEST)
  add_executable(HLSLTEST  tests/main.cpp)
  target_link_libraries(HLSLTEST PRIVATE ${PROJECT_NAME})
  target_include_directories(HLSLTEST PUBLIC include)
endif()