cmake_minimum_required(VERSION 3.14...3.31.0)
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
  set(CMAKE_CXX_FLAGS_RELEASE "/O2 /Ob2 /DNDEBUG /MD")
  set(CMAKE_CXX_FLAGS_DEBUG " /Zi /Od /MDd")
elseif(APPLE)
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
else()
  set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -fPIC -pthread")
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG -fPIC -pthread")
endif()
# HLSL2ALL
set(SOURCE_FILES source/SourceToSpirv.cpp source/SpirvToSource.cpp source/SourceParser.cpp)
set(HEADER_FILES include/HLSL2ALL/HLSL2ALL.h include/HLSL2ALL/HLSL2ALLConfig.h include/HLSL2ALL/SourceToSpirv.h include/HLSL2ALL/SpirvToSource.h source/SourceParser.h)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} ${HEADER_FILES} ) 
add_definitions(-DHLSL2ALL_LIBRARY_EXPORT)
target_include_directories(${PROJECT_NAME} PUBLIC include)
# macOS, rpath
if(APPLE)
  set_target_properties(${PROJECT_NAME} PROPERTIES BUILD_WITH_INSTALL_RPATH 1 INSTALL_NAME_DIR "@rpath")
endif()

# GLSLANG
set(GLSLANG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/glslang")

# SPIRV-Tools (and its SPIRV-Headers): the optimizer of glslang, which "legalizes" the SPIR-V
# made from HLSL for Vulkan (e.g. no store of textures passed to functions). Only the library
# (no executables, no tests, no install); its tables are generated with Python 3.
set(SPIRV_HEADERS_SKIP_EXAMPLES ON CACHE BOOL "" FORCE)
set(SPIRV_HEADERS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(SPIRV_WERROR OFF CACHE BOOL "" FORCE)
set(SKIP_SPIRV_TOOLS_INSTALL ON CACHE BOOL "" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/SPIRV-Headers")
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/SPIRV-Tools")
# built only if linked: glslang needs SPIRV-Tools-opt (and its SPIRV-Tools-static) only
foreach(spirv_tools_unused SPIRV-Tools-shared SPIRV-Tools-link SPIRV-Tools-lint SPIRV-Tools-diff SPIRV-Tools-reduce SPIRV-Tools-fuzz)
  if(TARGET ${spirv_tools_unused})
    set_target_properties(${spirv_tools_unused} PROPERTIES EXCLUDE_FROM_ALL ON)
  endif()
endforeach()

# glslang build options (set before add_subdirectory). ENABLE_OPT: glslang links SPIRV-Tools-opt
# (the target above) and runs it on the SPIR-V the caller asks to optimize
set(ENABLE_OPT ON CACHE BOOL "" FORCE)              # the optimizer (SPIRV-Tools-opt)
set(ENABLE_HLSL ON  CACHE BOOL "" FORCE)            # we need the HLSL front-end
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)

add_subdirectory(${GLSLANG_DIR})
# Modern glslang (>= 2021) merged HLSL and OGLCompiler into the glslang target,
# so only these targets remain; the rest are pulled in transitively.
set(glslang_libraries glslang SPIRV glslang-default-resource-limits)

target_link_libraries(${PROJECT_NAME} PRIVATE ${glslang_libraries})
target_include_directories(${PROJECT_NAME} PUBLIC ${GLSLANG_DIR})
target_include_directories(${PROJECT_NAME} PUBLIC "${GLSLANG_DIR}/glslang/Include")
target_include_directories(${PROJECT_NAME} PUBLIC "${GLSLANG_DIR}/glslang/Public")
target_include_directories(${PROJECT_NAME} PUBLIC "${GLSLANG_DIR}/SPIRV")

# SPIRVCROSS
set(SPIRVCROSS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/SPIRV-Cross")
add_subdirectory(${SPIRVCROSS_DIR})
set(spirv_cross_libraries spirv-cross-core spirv-cross-reflect spirv-cross-util spirv-cross-glsl spirv-cross-hlsl spirv-cross-msl)

target_link_libraries(${PROJECT_NAME} PRIVATE ${spirv_cross_libraries})
target_include_directories(${PROJECT_NAME} PUBLIC ${SPIRVCROSS_DIR})

# HLSL Test
if(ENABLE_HLSL2ALL_TEST)
  add_executable(HLSLTEST  tests/main.cpp)
  target_link_libraries(HLSLTEST PRIVATE ${PROJECT_NAME})
  target_include_directories(HLSLTEST PUBLIC include)
endif()