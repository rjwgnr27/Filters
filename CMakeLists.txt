cmake_minimum_required(VERSION 3.0)

# The version number.
set(APP_VERSION_MAJOR "1")
set(APP_VERSION_MINOR "0")
set(APP_VERSION_PATCH "1")
set(APP_VERSION_STRING "${APP_VERSION_MAJOR}.${APP_VERSION_MINOR}.${APP_VERSION_PATCH}")

project(filters VERSION ${APP_VERSION_STRING} LANGUAGES CXX)

# use either of these option to override the system default compiler
option(USE_GCC "build application with gcc" OFF) # OFF is the default
option(USE_CLANG "build application with clang" OFF) # OFF is the default

if (USE_GCC AND USE_CLANG)
    message(FATAL_ERROR "conflicting compiler selections")
endif (USE_GCC AND USE_CLANG)

if (USE_GCC)
    message(STATUS "Using GCC compilers")
    set(CMAKE_C_COMPILER "/usr/bin/gcc" CACHE STRING "gcc compiler" FORCE)
    set(CMAKE_CXX_COMPILER "/usr/bin/g++" CACHE STRING "g++ compiler" FORCE)
endif (USE_GCC)

if (USE_CLANG)
    message(STATUS "Using LLVM/CLANG compilers")
    set(CMAKE_C_COMPILER "/usr/bin/clang" CACHE STRING "clang compiler" FORCE)
    set(CMAKE_CXX_COMPILER "/usr/bin/clang++" CACHE STRING "clang++ compiler" FORCE)
endif (USE_CLANG)

configure_file(src/filters_config.h.in filters_config.h)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
# Instruct CMake to create code from Qt designer ui files
set(CMAKE_AUTOUIC ON)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(QT_MIN_VERSION "5.3.0")
set(KF5_MIN_VERSION "5.2.0")

find_package(ECM 1.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
#include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Widgets
)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    DBusAddons
    I18n
    KIO
    XmlGui
)

set(filters_SRC
    src/main.cpp
    src/filters.cpp
)

qt5_wrap_ui(filters_SRC
    src/mainwidget.ui
)

add_executable(filters ${filters_SRC})
target_compile_features(filters PUBLIC "cxx_std_11")

target_link_libraries(filters
    Qt5::Widgets
    KF5::CoreAddons
    KF5::DBusAddons
    KF5::I18n
    KF5::KIOCore
    KF5::KIOFileWidgets
    KF5::KIOWidgets
    KF5::XmlGui
)

if (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
    SET (CMAKE_CXX_FLAGS "-std=c++11")
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
    SET (CMAKE_CXX_FLAGS "-std=c++11")
else ()
    message(FATAL_ERROR "Unknown compiler: ${CMAKE_CXX_COMPILER_ID}")
endif ()

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

# Install the executable
install(TARGETS filters DESTINATION bin)
install(PROGRAMS src/filters.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES src/filtersui.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/filters)
