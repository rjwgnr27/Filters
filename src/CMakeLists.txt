set(filters_SRC
    main.cpp
    filters.cpp
    mainwidget.cpp
    wlogtext.cpp
)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
    Core
    Widgets
)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
    CoreAddons
    DBusAddons
    I18n
    IconThemes
    KIO
    TextWidgets
    XmlGui
)

add_executable(filters ${filters_SRC})
configure_file(filters_config.h.in filters_config.h)

target_compile_features(filters PUBLIC "cxx_std_20")
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    add_compile_options(-Wall -Werror -Wextra -pedantic)
elseif (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Werror -Wextra -pedantic)
endif ()

add_compile_options(-Wunreachable-code)
add_compile_options(-Wshadow-compatible-local)
add_compile_options(-Wformat-signedness)
add_compile_options(-Wshadow)
add_compile_options(-Wconversion)
add_compile_options(-Wzero-as-null-pointer-constant)
#add_compile_options(-Wsign-conversion)
add_compile_options(-Wold-style-cast)

target_link_libraries(filters PRIVATE
    Qt5::Core
    Qt5::Widgets
    KF5::CoreAddons
    KF5::DBusAddons
    KF5::I18n
    KF5::IconThemes
    KF5::KIOCore
    KF5::KIOFileWidgets
    KF5::KIOWidgets
    KF5::TextWidgets
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

# qt5_wrap_ui(filters_SRC
#     mainwidget.ui
# )

feature_summary(WHAT ALL FATAL_ON_MISSING_REQUIRED_PACKAGES)

