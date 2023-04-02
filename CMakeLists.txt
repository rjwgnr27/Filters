cmake_minimum_required(VERSION 3.16.0)

# cmake command line options:
# --toolchain=cmake/XXX
#
# -DCMAKE_BUILD_TYPE={Debug|RelWithDebInfo|Release}
# -DWITH_ASAN=true      -- build with address sanitizer
# -DWITH_UBSAN=true     -- build with undefined behavior sanitizer
# -DWITH_LTO=true       -- build with link-time (inter-procedural) optimization (LTO, IPO)

# The version number.
set(APP_VERSION_MAJOR "1")
set(APP_VERSION_MINOR "0")
set(APP_VERSION_PATCH "2")
set(APP_VERSION_STRING "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH}")

project(filters VERSION ${APP_VERSION_STRING} LANGUAGES CXX)

option(WITH_ASAN "build with address sanitizer" FALSE)
option(WITH_UBSAN "build with undefined behavior sanitizer" FALSE)
option(WITH_LTO "build with link-time (inter-procedural) optimization (LTO, IPO)" FALSE)

set(QT_MIN_VERSION "5.3.0")
set(KF5_MIN_VERSION "5.2.0")

find_package(ECM 1.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMInstallIcons)
#include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

include(GNUInstallDirs)
if(NOT WIN32 AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    #set(CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}")
    set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)

# Make cmake-gui use a selection, rather than text entry, for the likely build types:
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo )

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
# Instruct CMake to create code from Qt designer UI files
set(CMAKE_AUTOUIC ON)

add_subdirectory(src)
add_subdirectory(icons)

# Install the executable
install(TARGETS filters DESTINATION bin)
install(PROGRAMS filters.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES filtersui.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/filters)
