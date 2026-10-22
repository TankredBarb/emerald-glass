# SPDX-License-Identifier: LGPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(EmeraldSystemMonitor LANGUAGES CXX)
find_package(Qt6 REQUIRED COMPONENTS Quick ShaderTools)
find_package(KF6I18n REQUIRED)

set(package_dir "${CMAKE_CURRENT_BINARY_DIR}/package")
configure_file(package/metadata.json "${package_dir}/metadata.json" COPYONLY)
configure_file(../../LICENSE "${package_dir}/LICENSE" COPYONLY)
configure_file(../../COPYING "${package_dir}/COPYING" COPYONLY)
configure_file(package/contents/ui/SensorLabels.js "${package_dir}/contents/ui/SensorLabels.js" COPYONLY)
foreach(component main GlassCard DesktopRotationFix SystemMonitor SensorGraph SensorText ConfigEffects EffectSlider ConfigText TextFontButton ConfigSensors)
    configure_file("package/contents/ui/${component}.qml"
        "${package_dir}/contents/ui/${component}.qml" COPYONLY)
endforeach()
foreach(config main.xml config.qml)
    configure_file("package/contents/config/${config}"
        "${package_dir}/contents/config/${config}" COPYONLY)
endforeach()
add_custom_command(OUTPUT "${package_dir}/contents/ui/glass.frag.qsb"
    COMMAND Qt6::qsb --qt6 -o "${package_dir}/contents/ui/glass.frag.qsb"
        "${CMAKE_CURRENT_SOURCE_DIR}/glass.frag"
    DEPENDS glass.frag VERBATIM)
add_custom_target(emerald-monitor ALL DEPENDS "${package_dir}/contents/ui/glass.frag.qsb")

add_executable(emerald-monitor-preview preview.cpp)
target_link_libraries(emerald-monitor-preview PRIVATE Qt6::Quick KF6::I18nQml)
target_compile_definitions(emerald-monitor-preview PRIVATE
    MONITOR_PREVIEW_QML="${CMAKE_CURRENT_SOURCE_DIR}/Preview.qml"
    MONITOR_PACKAGE_UI="${package_dir}/contents/ui")
add_dependencies(emerald-monitor-preview emerald-monitor)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS QuickTest)
    find_package(KF6Config REQUIRED)
    add_executable(emerald-monitor-test tests/main.cpp)
    target_link_libraries(emerald-monitor-test PRIVATE Qt6::QuickTest Qt6::Quick KF6::ConfigGui KF6::I18nQml)
    set_target_properties(emerald-monitor-test PROPERTIES AUTOMOC ON)
    configure_file(tests/tst_glass.qml tests/tst_glass.qml COPYONLY)
    configure_file(tests/tst_config.qml tests/tst_config.qml COPYONLY)
    configure_file(tests/tst_text.qml tests/tst_text.qml COPYONLY)
    configure_file(tests/tst_charts.qml tests/tst_charts.qml COPYONLY)
    configure_file(tests/tst_sensors.qml tests/tst_sensors.qml COPYONLY)
    configure_file(tests/tst_rotation.qml tests/tst_rotation.qml COPYONLY)
    target_compile_definitions(emerald-monitor-test PRIVATE
        QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_BINARY_DIR}/tests"
        MONITOR_CONFIG_XML="${CMAKE_CURRENT_SOURCE_DIR}/package/contents/config/main.xml")
    add_dependencies(emerald-monitor-test emerald-monitor)
    # Isolate scene graph / native style teardown between QML fixtures.
    foreach(suite charts config glass sensors text rotation)
        add_test(NAME monitor-${suite} COMMAND emerald-monitor-test
            -input "${CMAKE_CURRENT_BINARY_DIR}/tests/tst_${suite}.qml"
            -import "${package_dir}/contents/ui")
        set_tests_properties(monitor-${suite} PROPERTIES TIMEOUT 30 RUN_SERIAL TRUE
            ENVIRONMENT "QT_QUICK_CONTROLS_STYLE=org.kde.desktop")
    endforeach()
    add_test(NAME monitor-preview-arguments COMMAND "${CMAKE_COMMAND}"
        "-DPREVIEW=$<TARGET_FILE:emerald-monitor-preview>"
        "-DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}/preview-cli-test"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/preview_arguments.cmake")
    set_tests_properties(monitor-preview-arguments PROPERTIES TIMEOUT 35 RUN_SERIAL TRUE
        ENVIRONMENT "QT_QUICK_CONTROLS_STYLE=org.kde.desktop")
endif()
