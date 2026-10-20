# SPDX-License-Identifier: LGPL-3.0-or-later
find_package(ECM REQUIRED NO_MODULE)
list(APPEND CMAKE_MODULE_PATH ${ECM_MODULE_PATH})
find_package(Qt6 REQUIRED COMPONENTS Widgets DBus OpenGL)
find_package(KF6CoreAddons REQUIRED)
find_package(KF6Config REQUIRED)
find_package(KF6KCMUtils REQUIRED)
find_package(KDecoration3 REQUIRED)

add_library(emeraldglass-decoration MODULE decoration.cpp)
set_target_properties(emeraldglass-decoration PROPERTIES
    AUTOMOC ON PREFIX "" OUTPUT_NAME "org.tankred.emeraldglass")
target_link_libraries(emeraldglass-decoration PRIVATE
    Qt6::Widgets KF6::CoreAddons KF6::ConfigCore KDecoration3::KDecoration)
install(TARGETS emeraldglass-decoration DESTINATION lib/qt6/plugins/org.kde.kdecoration3)

add_library(emeraldglass-material-preview STATIC materialpreview.cpp materialpreview.qrc)
set_target_properties(emeraldglass-material-preview PROPERTIES AUTORCC ON POSITION_INDEPENDENT_CODE ON)
target_link_libraries(emeraldglass-material-preview PUBLIC
    Qt6::Widgets Qt6::OpenGL KF6::ConfigCore)
target_link_libraries(emeraldglass-decoration PRIVATE emeraldglass-material-preview)

add_library(kcm_emeraldglassdecoration MODULE kcm.cpp)
set_target_properties(kcm_emeraldglassdecoration PROPERTIES
    AUTOMOC ON PREFIX "" OUTPUT_NAME "kcm_emeraldglassdecoration")
target_link_libraries(kcm_emeraldglassdecoration PRIVATE
    emeraldglass-material-preview Qt6::Widgets Qt6::DBus
    KF6::CoreAddons KF6::ConfigCore KF6::KCMUtils)
install(TARGETS kcm_emeraldglassdecoration DESTINATION lib/qt6/plugins/org.kde.kdecoration3.kcm)

if(BUILD_TESTING)
    add_executable(effect-settings-test ../tests/effect_settings_test.cpp ../vendor/kwin-effects-glass/src/settings.cpp)
    kconfig_add_kcfg_files(effect-settings-test ../vendor/kwin-effects-glass/src/blurconfig.kcfgc)
    target_include_directories(effect-settings-test PRIVATE ${CMAKE_CURRENT_BINARY_DIR} ../vendor/kwin-effects-glass/src)
    target_link_libraries(effect-settings-test PRIVATE Qt6::Widgets KF6::ConfigGui)
    add_test(NAME effect-settings COMMAND effect-settings-test)
    add_executable(material-preview-test ../tests/material_preview_test.cpp)
    target_include_directories(material-preview-test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(material-preview-test PRIVATE emeraldglass-material-preview)
    add_test(NAME material-preview-failure COMMAND material-preview-test)
    set_tests_properties(material-preview-failure PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
    find_package(PkgConfig QUIET)
    if(PkgConfig_FOUND)
        pkg_check_modules(EGL QUIET IMPORTED_TARGET egl)
        pkg_check_modules(EPOXY QUIET IMPORTED_TARGET epoxy)
        if(TARGET PkgConfig::EGL AND TARGET PkgConfig::EPOXY)
            add_executable(blur-stack-probe ../tests/blur_stack_probe.cpp)
            target_link_libraries(blur-stack-probe PRIVATE Qt6::Widgets KF6::ConfigCore PkgConfig::EGL PkgConfig::EPOXY)
            add_executable(material-shader-test ../tests/material_shader_test.cpp)
            target_link_libraries(material-shader-test PRIVATE PkgConfig::EGL PkgConfig::EPOXY)
            add_test(NAME material-shader COMMAND material-shader-test ${PROJECT_SOURCE_DIR}/preview/glass.frag --sheet-geometry)
            add_test(NAME header-optics COMMAND blur-stack-probe ${PROJECT_SOURCE_DIR} --header-optics)
            set_tests_properties(header-optics PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
            set_tests_properties(material-shader PROPERTIES ENVIRONMENT "LIBGL_ALWAYS_SOFTWARE=1")
        endif()
    endif()
    find_package(Qt6 REQUIRED COMPONENTS Quick)
    add_executable(decoration-card-test ../tests/decoration_card_test.cpp)
    target_link_libraries(decoration-card-test PRIVATE Qt6::Widgets Qt6::Quick KF6::ConfigCore)
    add_dependencies(decoration-card-test emeraldglass-decoration)
    add_test(NAME decoration-card COMMAND decoration-card-test $<TARGET_FILE:emeraldglass-decoration>)
    set_tests_properties(decoration-card PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent")
    add_executable(decoration-scope-test ../tests/decoration_scope_test.cpp)
    target_link_libraries(decoration-scope-test PRIVATE
        Qt6::Widgets KF6::CoreAddons KF6::ConfigCore KF6::KCMUtils
        KDecoration3::KDecoration KDecoration3::KDecoration3Private)
    add_executable(decoration-resize-probe ../tests/decoration_resize_probe.cpp)
    target_link_libraries(decoration-resize-probe PRIVATE Qt6::Widgets KF6::CoreAddons KF6::ConfigCore KDecoration3::KDecoration KDecoration3::KDecoration3Private)
    add_dependencies(decoration-scope-test emeraldglass-decoration kcm_emeraldglassdecoration)
    add_test(NAME decoration-scope COMMAND decoration-scope-test
        $<TARGET_FILE:emeraldglass-decoration> $<TARGET_FILE:kcm_emeraldglassdecoration>)
    set_tests_properties(decoration-scope PROPERTIES ENVIRONMENT
        "QT_QPA_PLATFORM=offscreen;DBUS_SESSION_BUS_ADDRESS=unix:path=/nonexistent")
endif()
