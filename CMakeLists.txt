cmake_minimum_required(VERSION 3.20)
project(VulkanRayTracing LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ── Vulkan SDK ───────────────────────────────────────────────────────────────
find_package(Vulkan REQUIRED)

# ── Third-party dependencies ─────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
)
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)

FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.1
    GIT_SHALLOW    TRUE
)

# VMA: download only, used as header-only in our code
FetchContent_Declare(vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.1.0
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(tinyobjloader
    GIT_REPOSITORY https://github.com/tinyobjloader/tinyobjloader.git
    GIT_TAG        v2.0.0rc13
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.5
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(glfw glm)

FetchContent_MakeAvailable(vma)
FetchContent_MakeAvailable(tinyobjloader)

# ImGui has no CMakeLists.txt – download only
FetchContent_GetProperties(imgui)
if(NOT imgui_POPULATED)
    FetchContent_Populate(imgui)
endif()

# ── Executable ───────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
add_executable(${PROJECT_NAME} ${SOURCES})

target_sources(${PROJECT_NAME} PRIVATE
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${vma_SOURCE_DIR}/include
    ${tinyobjloader_SOURCE_DIR}
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    glm::glm
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    GLFW_INCLUDE_VULKAN
    GLM_FORCE_RADIANS
    GLM_FORCE_DEPTH_ZERO_TO_ONE
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS NOMINMAX>
)

# ── Shader compilation ───────────────────────────────────────────────────────
set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(SHADER_BIN_DIR ${CMAKE_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_BIN_DIR})

find_program(GLSLC glslc HINTS $ENV{VULKAN_SDK}/Bin)

file(GLOB SHADERS
    ${SHADER_SRC_DIR}/*.rgen  ${SHADER_SRC_DIR}/*.rmiss
    ${SHADER_SRC_DIR}/*.rchit ${SHADER_SRC_DIR}/*.rahit
    ${SHADER_SRC_DIR}/*.comp  ${SHADER_SRC_DIR}/*.vert
    ${SHADER_SRC_DIR}/*.frag
)

set(SPV_FILES "")
foreach(SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV ${SHADER_BIN_DIR}/${SHADER_NAME}.spv)
    add_custom_command(
        OUTPUT  ${SPV}
        COMMAND ${GLSLC} --target-env=vulkan1.3 ${SHADER} -o ${SPV}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader: ${SHADER_NAME}"
    )
    list(APPEND SPV_FILES ${SPV})
endforeach()

if(SPV_FILES)
    add_custom_target(Shaders DEPENDS ${SPV_FILES})
    add_dependencies(${PROJECT_NAME} Shaders)
endif()

# Copy compiled shaders next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SHADER_BIN_DIR} "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders"
    COMMENT "Copying shaders to output directory"
)

# Copy assets (models, textures) next to the executable
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_SOURCE_DIR}/assets "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
    COMMENT "Copying assets to output directory"
)
