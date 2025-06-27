cmake_minimum_required(VERSION 3.24)

# =============================================================================
# Project metadata
# =============================================================================
project(TabDisplay VERSION 0.1.0 LANGUAGES CXX)

# =============================================================================
# Global build configuration
# =============================================================================
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Produce compile_commands.json for IDEs
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    # Treat warnings as warnings (/W4) but do not fail (-WX-)
    add_compile_options(/W4 /WX-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_definitions(-DNOMINMAX)

# =============================================================================
# Dependency handling via CPM.cmake (already vendored in cmake/)
# =============================================================================
include(cmake/CPM.cmake)

# ---- spdlog ---------------------------------------------------------------
CPMAddPackage(
    NAME spdlog
    GITHUB_REPOSITORY gabime/spdlog
    VERSION 1.13.0
    OPTIONS
        "SPDLOG_BUILD_EXAMPLE OFF"
        "SPDLOG_BUILD_TESTS OFF"
        "SPDLOG_BUILD_BENCH OFF"
        "SPDLOG_INSTALL OFF"
)

# ---- nlohmann/json --------------------------------------------------------
CPMAddPackage(
    NAME nlohmann_json
    GITHUB_REPOSITORY nlohmann/json
    VERSION 3.11.2
    OPTIONS "JSON_BuildTests OFF"
)

# =============================================================================
# Windows App SDK / WinUI 3 Detection
# =============================================================================
set(USE_WINUI3 OFF)

if(WIN32)
    # First try to find NuGet packages
    set(NUGET_PACKAGES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/packages")
    
    if(EXISTS "${NUGET_PACKAGES_DIR}")
        file(GLOB WINDOWS_APP_SDK_DIRS "${NUGET_PACKAGES_DIR}/Microsoft.WindowsAppSDK.*")
        if(WINDOWS_APP_SDK_DIRS)
            list(GET WINDOWS_APP_SDK_DIRS 0 WINDOWS_APP_SDK_DIR)
            set(USE_WINUI3 ON)
            message(STATUS "Found Windows App SDK via NuGet at: ${WINDOWS_APP_SDK_DIR}")
            
            # Set up include and library paths
            set(WINDOWS_APP_SDK_INCLUDE_DIR "${WINDOWS_APP_SDK_DIR}/include")
            if(CMAKE_SIZEOF_VOID_P EQUAL 8)
                set(WINDOWS_APP_SDK_LIB_DIR "${WINDOWS_APP_SDK_DIR}/lib/win10-x64")
            else()
                set(WINDOWS_APP_SDK_LIB_DIR "${WINDOWS_APP_SDK_DIR}/lib/win10-x86")
            endif()
        endif()
    endif()
    
    # Fallback: Try to find via Windows SDK
    if(NOT USE_WINUI3)
        find_package(WindowsAppSDK QUIET)
        
        if(WindowsAppSDK_FOUND)
            set(USE_WINUI3 ON)
            message(STATUS "Found Windows App SDK via find_package")
        else()
            # Check for WinRT headers as a backup
            find_path(WINRT_BASE_H
                NAMES "winrt/base.h"
                HINTS
                    "C:/Program Files (x86)/Windows Kits/10/Include/*/winrt"
                    "C:/Program Files/Windows Kits/10/Include/*/winrt"
                PATH_SUFFIXES "winrt"
            )
            
            if(WINRT_BASE_H)
                # Check for Microsoft.UI.Xaml.h specifically
                find_path(WINUI3_HEADERS
                    NAMES "winrt/Microsoft.UI.Xaml.h"
                    HINTS
                        "${WINRT_BASE_H}/.."
                        "C:/Program Files (x86)/Windows Kits/10/Include/*/winrt"
                        "C:/Program Files/Windows Kits/10/Include/*/winrt"
                )
                
                if(WINUI3_HEADERS)
                    set(USE_WINUI3 ON)
                    message(STATUS "Found WinUI 3 headers at: ${WINUI3_HEADERS}")
                endif()
            endif()
        endif()
    endif()
endif()

if(USE_WINUI3)
    message(STATUS "Building with WinUI 3 support")
else()
    message(WARNING "Windows App SDK not found - building with stub implementation. Run install_windows_app_sdk.ps1 to install.")
endif()

# =============================================================================
# Local (in-tree) third-party libraries
# =============================================================================
add_subdirectory(third_party/fec)

# =============================================================================
# Source files
# =============================================================================
set(TABDISPLAY_SOURCES
    src/main.cpp
    src/CaptureDXGI.cpp
    src/EncoderAMF.cpp
    src/UdpSender.cpp
    src/InputInjector.cpp
    src/Settings.cpp
    src/UsbDeviceDiscovery.cpp
)

# Add the appropriate TrayApp implementation based on WinUI 3 availability
if(USE_WINUI3)
    # Check if the generated headers exist and include the necessary Microsoft.UI.Xaml headers
    set(REQUIRED_WINUI3_HEADERS 
        "${CMAKE_CURRENT_SOURCE_DIR}/generated_winui3/winrt/Microsoft.UI.Xaml.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/generated_winui3/winrt/Microsoft.UI.Xaml.Controls.h")
    
    set(WINUI3_HEADERS_FOUND TRUE)
    foreach(HEADER ${REQUIRED_WINUI3_HEADERS})
        if(NOT EXISTS ${HEADER})
            set(WINUI3_HEADERS_FOUND FALSE)
            message(STATUS "Missing WinUI 3 header: ${HEADER}")
        endif()
    endforeach()
    
    if(WINUI3_HEADERS_FOUND)
        list(APPEND TABDISPLAY_SOURCES src/TrayApp.xaml.cpp)
        message(STATUS "Using full WinUI 3 implementation")
    else()
        # Check if we have some WinUI 3 headers (use Win32 tray instead of basic)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated_winui3/winrt/Microsoft.UI.Xaml.Data.h")
            list(APPEND TABDISPLAY_SOURCES src/TrayApp_win32.cpp)
            message(STATUS "Using Win32 tray implementation - WinUI 3 headers partially available")
        else()
            list(APPEND TABDISPLAY_SOURCES src/TrayApp_basic.cpp)
            message(STATUS "Using basic implementation - WinUI 3 headers not found")
        endif()
    endif()
else()
    list(APPEND TABDISPLAY_SOURCES src/TrayApp_stub.cpp)
    message(STATUS "Using stub implementation - Windows App SDK not detected")
endif()

# =============================================================================
# Executable target
# =============================================================================
add_executable(TabDisplay WIN32 ${TABDISPLAY_SOURCES})

# Include search paths
# (third_party contains amf_stub & other headers that are header-only)
# Keep build directory include for generated files if any

set(TABDISPLAY_INCLUDE_DIRS
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/third_party
    ${PROJECT_SOURCE_DIR}/third_party/amf_stub
)

# Add Windows App SDK include directories if available
if(USE_WINUI3 AND WINDOWS_APP_SDK_INCLUDE_DIR)
    list(APPEND TABDISPLAY_INCLUDE_DIRS ${WINDOWS_APP_SDK_INCLUDE_DIR})
    
    # Add generated WinRT headers if they exist
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated_winui3/winrt")
        list(APPEND TABDISPLAY_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/generated_winui3")
        message(STATUS "Added generated WinUI 3 headers from generated_winui3")
    elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/generated/winrt")
        list(APPEND TABDISPLAY_INCLUDE_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/generated")
        message(STATUS "Added generated WinUI 3 headers from generated")
    endif()
endif()

target_include_directories(TabDisplay
    PRIVATE ${TABDISPLAY_INCLUDE_DIRS}
)

target_compile_definitions(TabDisplay
    PRIVATE SPDLOG_COMPILED_LIB _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)

# Add WinUI 3 specific compile definitions
if(USE_WINUI3)
    target_compile_definitions(TabDisplay
        PRIVATE WINRT_LEAN_AND_MEAN
    )
endif()

# Base libraries
set(TABDISPLAY_LIBRARIES
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    fec
    d3d11
    dxgi
    ws2_32
    bcrypt
    shcore
    winmm
)

# Add WinUI 3 libraries if available
if(USE_WINUI3)
    if(WINDOWS_APP_SDK_LIB_DIR)
        # Add Windows App SDK library directory
        target_link_directories(TabDisplay PRIVATE ${WINDOWS_APP_SDK_LIB_DIR})
        
        # Add Windows App SDK specific libraries
        list(APPEND TABDISPLAY_LIBRARIES
            windowsapp
            Microsoft.WindowsAppRuntime
        )
    else()
        # Fallback to system libraries
        list(APPEND TABDISPLAY_LIBRARIES
            windowsapp
        )
    endif()
else()
    list(APPEND TABDISPLAY_LIBRARIES
        windowsapp
    )
endif()

target_link_libraries(TabDisplay
    PRIVATE ${TABDISPLAY_LIBRARIES}
)

# =============================================================================
# Installation (export & config package)
# =============================================================================
include(GNUInstallDirs)

install(TARGETS TabDisplay
        EXPORT TabDisplayTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(EXPORT TabDisplayTargets
        FILE TabDisplayTargets.cmake
        NAMESPACE TabDisplay::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TabDisplay)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/TabDisplayConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/TabDisplay-config.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/TabDisplay-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TabDisplay
)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/TabDisplay-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/TabDisplayConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/TabDisplay) 