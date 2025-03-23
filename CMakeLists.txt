cmake_minimum_required( VERSION 3.16 )

project( MEOWGL )

set( CMAKE_EXPORT_COMPILE_COMMANDS ON )

set( GAME_SOURCES
  # includes
  src/hardware.hpp
  src/logging.hpp
  src/render.hpp
  src/render_utils.hpp
  src/res.hpp
  src/shape.hpp
  src/state.hpp
  src/utils.hpp
  src/wavefront.hpp

  # sources
  src/logging.cpp
  src/main.cpp
  src/render.cpp
  src/render_utils.cpp
  src/file_res.cpp
  src/shape.cpp
  src/state.cpp
  src/utils.cpp
  src/wavefront.cpp
)

# common libs
add_library( cglm INTERFACE )
target_include_directories( cglm INTERFACE libs/cglm/include )
add_library( stb INTERFACE )
target_include_directories( stb INTERFACE libs/stb/include )
add_library( glad libs/glad/src/glad.c )
target_include_directories( glad PUBLIC libs/glad/include )
add_library( imgui 
    libs/imgui/imgui.cpp 
    libs/imgui/imgui_demo.cpp 
    libs/imgui/imgui_widgets.cpp 
    libs/imgui/imgui_draw.cpp 
    libs/imgui/imgui_tables.cpp 
    libs/imgui/backends/imgui_impl_glfw.cpp 
    libs/imgui/backends/imgui_impl_opengl3.cpp )
target_include_directories( imgui PUBLIC libs/imgui )
add_library( cjson libs/cjson/cJSON.c )
target_include_directories( cjson PUBLIC libs/cjson )

#
# linux build 
#
if ( (NOT DEFINED EMSCRIPTEN) AND UNIX )

  # pull libraries from the system
  find_package( PkgConfig REQUIRED )
  pkg_check_modules( GLFW REQUIRED IMPORTED_TARGET glfw3 )
  add_executable( app ${GAME_SOURCES} src/platform/desktop.cpp )
  target_link_libraries( app PRIVATE imgui cjson glad cglm stb PkgConfig::GLFW )
  add_custom_target( run COMMAND app DEPENDS app WORKING_DIRECTORY ${CMAKE_PROJECT_DIR} )

endif()

#
# windows build
#
if ( (NOT DEFINED EMSCRIPTEN) AND WIN32 )

  # pull prebuilt libraries
  add_library( glfw STATIC IMPORTED )
  set_target_properties(glfw PROPERTIES IMPORTED_LOCATION ${PROJECT_SOURCE_DIR}/libs/glfw/lib-vc2019/glfw3.lib)
  target_include_directories(glfw INTERFACE libs/glfw/include)
  add_library( openal SHARED IMPORTED )
  add_executable( app ${GAME_SOURCES} src/platform/desktop.cpp )
  set_target_properties(app PROPERTIES WIN32_EXECUTABLE $<CONFIG:Release>)
  target_link_libraries( app PRIVATE glad cglm stb glfw )
endif()

#
# emscripten build
#
if ( DEFINED EMSCRIPTEN )

  add_executable( app ${GAME_SOURCES} src/platform/web.cpp )
  target_link_libraries( app PRIVATE cglm stb imgui )
  set_target_properties( app PROPERTIES LINK_FLAGS "-sMIN_WEBGL_VERSION=2 -s USE_GLFW=3 --shell-file ${PROJECT_SOURCE_DIR}/shell.html --embed-file ../res@/" )
  set(CMAKE_EXECUTABLE_SUFFIX ".html")

endif()

# common build flags
target_include_directories( app PRIVATE src )
target_compile_features( app PRIVATE cxx_std_20 )
target_compile_definitions( app PRIVATE "RELEASE=$<CONFIG:Release>" )

