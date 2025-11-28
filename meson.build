project('trm', 'cpp',
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

exe = executable('trm', sources, 
  include_directories : inc,
  dependencies : [harfbuzz_dep, freetype_dep, glfw_dep, opengl_dep, glu_dep, glm_dep, glew_dep]
)

