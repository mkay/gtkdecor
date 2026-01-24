librsvg = dependency('librsvg-2.0', required: false)

gtkdecor_deps = [wlroots, pixman, wf_protos, wfconfig, cairo, pango, pangocairo, plugin_pch_dep]
gtkdecor_cpp_args = []

if librsvg.found()
    gtkdecor_deps += librsvg
    gtkdecor_cpp_args += '-DHAVE_LIBRSVG'
endif

gtkdecor = shared_module('gtkdecor',
    ['decoration.cpp', 'deco-subsurface.cpp', 'deco-button.cpp',
      'deco-layout.cpp', 'deco-theme.cpp'],
    include_directories: [wayfire_api_inc, wayfire_conf_inc, plugins_common_inc],
    dependencies: gtkdecor_deps,
    cpp_args: gtkdecor_cpp_args,
    install: true,
    install_dir: join_paths(get_option('libdir'), 'wayfire'))
