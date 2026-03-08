project('gtkdecor', 'cpp',
    version: '0.2.1',
    default_options: ['cpp_std=c++17'])

wayfire = dependency('wayfire', version: '>=0.10.1')
librsvg = dependency('librsvg-2.0', required: false)

gtkdecor_deps = [wayfire]
gtkdecor_cpp_args = []

if librsvg.found()
    gtkdecor_deps += librsvg
    gtkdecor_cpp_args += '-DHAVE_LIBRSVG'
endif

subdir('src')
subdir('metadata')
subdir('icons')
