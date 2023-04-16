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

IF(NOT DEFINED ENV{HYPRLAND_HEADERS})
 message(FATAL_ERROR "$HYPRLAND_HEADERS is unset")
ENDIF()

find_package(PkgConfig REQUIRED)
pkg_check_modules(DEPS REQUIRED pixman-1 libdrm)

include_directories(
	"include/"
)

add_library(hy3 SHARED
	# files here
	"src/main.cpp"
)

target_include_directories(hy3 PRIVATE
	${DEPS_INCLUDE_DIRS}
	$ENV{HYPRLAND_HEADERS}
)

install(TARGETS hy3
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
