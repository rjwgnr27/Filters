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

include(GNUInstallDirs)
if(NOT WIN32 AND CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    #set(CMAKE_INSTALL_PREFIX "/opt/${PROJECT_NAME}")
    set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()

include(CheckIPOSupported)
check_ipo_supported(RESULT ipo_supported OUTPUT error)

# Make cmake-gui use a selection, rather than text entry, for the likely build types:
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo )

configure_file(src/filters_config.h.in filters_config.h)

# Find includes in corresponding build directories
set(CMAKE_INCLUDE_CURRENT_DIR ON)
# Instruct CMake to run moc automatically when needed.
set(CMAKE_AUTOMOC ON)
# Instruct CMake to create code from Qt designer UI files
set(CMAKE_AUTOUIC ON)

set(QT_MIN_VERSION "5.3.0")
set(KF5_MIN_VERSION "5.2.0")

find_package(ECM 1.0.0 REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

include(KDEInstallDirs)
include(KDECMakeSettings)
include(ECMInstallIcons)
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
target_compile_features(filters PUBLIC "cxx_std_20")
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -Werror -Wextra -pedantic)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Werror -Wextra -pedantic)
endif ()

target_link_libraries(filters PRIVATE
    Qt5::Core
    Qt5::Widgets
    KF5::CoreAddons
    KF5::DBusAddons
    KF5::I18n
    KF5::KIOCore
    KF5::KIOFileWidgets
    KF5::KIOWidgets
    KF5::XmlGui
)

# Optional sanitizers
# see https://gcc.gnu.org/onlinedocs/gcc/Instrumentation-Options.html
#
# with sanatize=address: At runtime set environment variable
#   "ASAN_OPTIONS=detect_stack_use_after_return=1"

# note, that if both asan and ubsan are used, asan must appear first in
# the list of libraries
if(WITH_ASAN)
    target_compile_options(filters PRIVATE -fsanitize=address)
    target_link_libraries(filters PRIVATE asan)
endif()
add_feature_info(asan WITH_ASAN "memory address sanitizer (ASAN)")

if(WITH_UBSAN)
    target_compile_options(filters PRIVATE -fsanitize=undefined)
    target_link_libraries(filters PRIVATE ubsan)
endif()
add_feature_info(ubsan WITH_UBSAN "undefined behavior sanitizer (UBSAN)")

if(${WITH_LTO})
    if(${ipo_supported})
        set_property(TARGET filters PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    else()
        message(SEND_ERROR "inter-procedural optimization (LTO) requested but not supported")
    endif()
endif()
add_feature_info(lto WITH_LTO "inter-procedural optimization (LTO)")

add_subdirectory(icons)

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

# Install the executable
install(TARGETS filters DESTINATION bin)
install(PROGRAMS src/filters.desktop DESTINATION ${XDG_APPS_INSTALL_DIR})
install(FILES src/filtersui.rc DESTINATION ${KDE_INSTALL_KXMLGUI5DIR}/filters)
