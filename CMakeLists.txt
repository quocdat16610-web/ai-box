# =============================================================================
# WorldBox AI — sandbox mô phỏng nền văn minh tự tiến hoá
#   * wb_core       : thư viện lõi (ECS, thế giới, AI, xã hội) — KHÔNG phụ thuộc đồ hoạ
#   * worldbox_sim  : chạy headless (test / benchmark / CI)
#   * worldbox      : game có đồ hoạ (raylib) — PC (Windows/Linux/macOS)
#   * libmain.so    : khi build bằng Android NDK (Gradle gọi CMake) → đóng gói APK
# =============================================================================
cmake_minimum_required(VERSION 3.16)
project(WorldBoxAI LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

# MSVC (Windows): mã nguồn có chú thích & chuỗi tiếng Việt UTF-8 → bắt buộc /utf-8;
# NOMINMAX tránh macro min/max của windows.h phá std::min/std::max.
if (MSVC)
    add_compile_options(/utf-8 /bigobj /MP)
    add_compile_definitions(NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()
# Liên kết tĩnh CRT trên MSVC → worldbox.exe chạy ngay, không cần cài VC++ Redistributable.
# Đặt cả policy mặc định để raylib (FetchContent) dùng cùng runtime, tránh lỗi LNK2038.
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(WB_BUILD_GAME "Build bản có đồ hoạ (raylib)" ON)
option(WB_BUILD_HEADLESS "Build trình chạy headless" ON)

# ---------------- Lõi mô phỏng ----------------
add_library(wb_core STATIC
    src/world/World.cpp
    src/phys/Material.cpp
    src/phys/MatterGrid.cpp
    src/phys/Chemistry.cpp
    src/phys/Physics.cpp
    src/ai/Knowledge.cpp
    src/ai/Invention.cpp
    src/ai/Creativity.cpp
    src/ai/Goap.cpp
    src/ai/Brain.cpp
    src/civ/SocietySystem.cpp
    src/sim/Species.cpp
    src/sim/Simulation.cpp
    src/sim/GodPowers.cpp
    src/sim/Scenarios.cpp
)
target_include_directories(wb_core PUBLIC src)
find_package(Threads REQUIRED)
target_link_libraries(wb_core PUBLIC Threads::Threads)
if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(wb_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

if (WB_BUILD_HEADLESS AND NOT ANDROID)
    add_executable(worldbox_sim src/headless/main_headless.cpp)
    target_link_libraries(worldbox_sim PRIVATE wb_core)
    enable_testing()
    # Kịch bản mẫu phải đạt được giai đoạn văn minh tối thiểu trong thời gian giới hạn
    add_test(NAME wolves_form_tribe COMMAND worldbox_sim --scenario wolves --years 25 --seed 3 --quiet --expect-stage Tribe)
    add_test(NAME humans_build_village COMMAND worldbox_sim --scenario humans --years 40 --seed 5 --quiet --expect-stage Village)
    add_test(NAME chemistry_reactions COMMAND worldbox_sim --chemtest)
    add_test(NAME sandbox_runs COMMAND worldbox_sim --scenario sandbox --years 10 --seed 1 --quiet)
    # Công nghệ mở: phải có phát minh thật; vật lý/hoá học: phải xảy ra nhiều loại phản ứng khác nhau
    add_test(NAME humans_invent_and_react COMMAND worldbox_sim --scenario humans --years 30 --seed 8 --quiet --expect-designs 10 --expect-reactions 5)
endif()

# ---------------- Game (raylib) ----------------
if (WB_BUILD_GAME)
    include(FetchContent)
    if (ANDROID)
        set(PLATFORM "Android" CACHE STRING "" FORCE)
    endif()
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    set(CUSTOMIZE_BUILD OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)

    set(GAME_SOURCES
        src/app/main.cpp
        src/app/Renderer.cpp
        src/app/Ui.cpp
        src/app/Text.cpp
    )
    if (ANDROID)
        # NativeActivity nạp "libmain.so" (khai báo trong AndroidManifest.xml)
        add_library(main SHARED ${GAME_SOURCES})
        target_link_libraries(main PRIVATE wb_core raylib android log EGL GLESv2 OpenSLES m)
        target_include_directories(main PRIVATE ${ANDROID_NDK}/sources/android/native_app_glue)
        # Giữ lại hàm ANativeActivity_onCreate (do raylib/native_app_glue định nghĩa)
        target_link_options(main PRIVATE "-u" "ANativeActivity_onCreate")
    else()
        add_executable(worldbox ${GAME_SOURCES})
        target_link_libraries(worldbox PRIVATE wb_core raylib)
        if (APPLE)
            target_link_libraries(worldbox PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
        endif()
        if (WIN32)
            # Game trên Windows: mở cửa sổ đồ hoạ, không bật thêm cửa sổ console đen
            set_target_properties(worldbox PROPERTIES WIN32_EXECUTABLE ON)
            if (MSVC)
                target_link_options(worldbox PRIVATE /ENTRY:mainCRTStartup)
            endif()
        endif()
    endif()
endif()
