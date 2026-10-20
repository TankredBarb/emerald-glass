# Add the localization domain
add_definitions(-DTRANSLATION_DOMAIN="kwin_effects_glass")

add_subdirectory(kcm)

# Read compat preambles
file(READ shaders/compat_core.glsl COMPAT_CORE)
#file(READ shaders/compat_legacy.glsl COMPAT_LEGACY)
file(READ shaders/oklab.glsl OKLAB_SHADER)
file(READ shaders/snells-glass.glsl SNELLS_GLASS_SHADER)
file(READ shaders/rim.glsl RIM_SHADER)
file(READ shaders/glass.glsl GLASS_SHADER)
set(EMERALD_SHADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../../preview/glass.frag")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${EMERALD_SHADER_PATH}")
file(READ "${EMERALD_SHADER_PATH}" EMERALD_SHADER)
string(REPLACE "#version 330 core\n" "" EMERALD_SHADER "${EMERALD_SHADER}")
string(REPLACE "in vec2 uv;\n" "" EMERALD_SHADER "${EMERALD_SHADER}")
string(REPLACE "out vec4 fragColor;\n" "" EMERALD_SHADER "${EMERALD_SHADER}")
string(REPLACE "void main()" "void emeraldPreviewMain()" EMERALD_SHADER "${EMERALD_SHADER}")
string(REPLACE "fragColor = emeraldMaterialMain();" "" EMERALD_SHADER "${EMERALD_SHADER}")

# Pre-expand snells-glass.glsl and rim.glsl into glass.glsl
string(REPLACE "#include \"snells-glass.glsl\"" "${SNELLS_GLASS_SHADER}" GLASS_SHADER "${GLASS_SHADER}")
string(REPLACE "#include \"rim.glsl\"" "${RIM_SHADER}" GLASS_SHADER "${GLASS_SHADER}")

# Generate a core shader variant from a single .glsl source.
# Output goes into generated/ to keep source shaders/ clean.
function(generate_shader_variants input_glsl output_base)
    file(READ "${input_glsl}" SHADER_SRC)

    # Expand helper-shader includes
    string(REPLACE "#include \"oklab.glsl\"" "${OKLAB_SHADER}" SHADER_SRC_EXPANDED "${SHADER_SRC}")
    string(REPLACE "#include \"glass.glsl\"" "${GLASS_SHADER}" SHADER_SRC_EXPANDED "${SHADER_SRC_EXPANDED}")
    string(REPLACE "#include \"emerald.glsl\"" "${EMERALD_SHADER}" SHADER_SRC_EXPANDED "${SHADER_SRC_EXPANDED}")

    # Core variant
    file(WRITE "generated/${output_base}${ARGN}" "${COMPAT_CORE}\n${SHADER_SRC_EXPANDED}")
endfunction()

# Generate vertex shaders
generate_shader_variants(shaders/vertex.glsl vertex .vert)
generate_shader_variants(shaders/onscreen_rounded_vertex.glsl onscreen_rounded .vert)

# Generate fragment shaders
generate_shader_variants(shaders/downsample.glsl downsample .frag)
generate_shader_variants(shaders/upsample.glsl upsample .frag)
generate_shader_variants(shaders/noise.glsl noise .frag)
generate_shader_variants(shaders/onscreen_rounded.glsl onscreen_rounded .frag)

set(glass_SOURCES
    blur.cpp
    settings.cpp
    blur.qrc
    main.cpp
)

kconfig_add_kcfg_files(glass_SOURCES
    blurconfig.kcfgc
)

if(GLASS_WAYLAND)
    add_library(glass MODULE ${glass_SOURCES})
    set_target_properties(glass PROPERTIES OUTPUT_NAME emeraldglass)
    target_link_libraries(glass PRIVATE
        KDecoration3::KDecoration
        KF6::ConfigGui
        KWin::kwin
    )
    if(GLASS_KWIN_67)
        target_compile_definitions(glass PRIVATE GLASS_KWIN_67)
    endif()
    if(GLASS_KWIN_68)
        target_compile_definitions(glass PRIVATE GLASS_KWIN_68)
    endif()
    install(TARGETS glass DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin/effects/plugins)
endif()
if(GLASS_X11)
    add_library(glass_x11 MODULE ${glass_SOURCES})
    target_link_libraries(glass_x11 PRIVATE
        KDecoration3::KDecoration
        KF6::ConfigGui
        KWinX11::kwin
        XCB::XCB
    )
    target_compile_definitions(glass_x11 PRIVATE GLASS_X11)
    install(TARGETS glass_x11 DESTINATION ${KDE_INSTALL_PLUGINDIR}/kwin-x11/effects/plugins)
endif()
