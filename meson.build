project('trm', 'cpp', 'c',
  version : '0.1',
  default_options : ['warning_level=3', 'cpp_std=c++23']
)

sources = [
  'main.cpp', 
  'src/gui.cpp',
  'src/tty.cpp',
  'src/oglutil.cpp',
  'src/utils.cpp',
  'src/terminal.cpp',
  'src/terminal_parser.cpp',
  'src/shader.cpp',
  'src/text_renderer.cpp',
  'src/terminal_view.cpp',
]

# Include directories
inc = include_directories('include')

# Find dependencies
harfbuzz_dep = dependency('harfbuzz', required: true)
freetype_dep = dependency('freetype2', required: true)
glfw_dep = dependency('glfw3', required: true)
opengl_dep = dependency('gl', required: true)
glu_dep = dependency('glu', required: true)
glm_dep = dependency('glm', required: true)
glew_dep = dependency('glew', required: true)

# Wayland dependencies for input method support
wayland_client_dep = dependency('wayland-client', required: false)
wayland_protocols_dep = dependency('wayland-protocols', required: false)

# Add Wayland text-input support if available
if wayland_client_dep.found() and wayland_protocols_dep.found()
  wayland_scanner = find_program('wayland-scanner')
  wayland_protocols_dir = wayland_protocols_dep.get_variable(pkgconfig: 'pkgdatadir')
  
  # Generate text-input-v3 protocol files
  text_input_xml = wayland_protocols_dir / 'unstable/text-input/text-input-unstable-v3.xml'
  
  text_input_client_header = custom_target(
    'text-input-client-header',
    input: text_input_xml,
    output: 'text-input-unstable-v3-client-protocol.h',
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'],
  )
  
  text_input_code = custom_target(
    'text-input-code',
    input: text_input_xml,
    output: 'text-input-unstable-v3-protocol.c',
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
  )
  
  sources += ['src/wayland_text_input.cpp', text_input_code]
  
  add_project_arguments('-DHAVE_WAYLAND', language: 'cpp')
  add_project_arguments('-DGLFW_EXPOSE_NATIVE_WAYLAND', language: 'cpp')
  
  message('Wayland text-input-v3 support enabled')
else
  message('Wayland text-input-v3 support disabled (missing dependencies)')
endif

# Copy shader files to build directory
shader_files = [
  'shaders/text.vert',
  'shaders/text.frag'
]

foreach shader : shader_files
  custom_target(
    'copy_' + shader.split('/')[-1],
    input : shader,
    output : shader.split('/')[-1],
    command : ['cp', '@INPUT@', '@OUTPUT@'],
    build_by_default : true
  )
endforeach

# Build dependency list
deps = [harfbuzz_dep, freetype_dep, glfw_dep, opengl_dep, glu_dep, glm_dep, glew_dep]
if wayland_client_dep.found()
  deps += wayland_client_dep
endif

exe = executable('trm', sources, 
  include_directories : inc,
  dependencies : deps
)
