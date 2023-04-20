cmake_minimum_required(VERSION 3.19)
project(HyprlandPlugin VERSION "0.1")
set(CMAKE_CXX_STANDARD 23)
add_compile_definitions(WLR_USE_UNSTABLE)
add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-value -Wno-missing-field-initializers -Wno-narrowing -Wno-pointer-arith)

# nix workaround
if(CMAKE_EXPORT_COMPILE_COMMANDS)
  set(CMAKE_CXX_STANDARD_INCLUDE_DIRECTORIES
      ${CMAKE_CXX_IMPLICIT_INCLUDE_DIRECTORIES})
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED pixman-1 libdrm wf-touch)

include_directories(
	"include/"
)

add_library(touch-gestures SHARED
	# files here
	"src/main.cpp"
)

target_include_directories(touch-gestures PRIVATE
	${DEPS_INCLUDE_DIRS}
)

install(TARGETS touch-gestures
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
