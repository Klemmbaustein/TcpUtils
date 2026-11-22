cmake_minimum_required (VERSION 4.0)
file(
	GLOB_RECURSE
	SRCS
	"src/**.cpp"
)
file(
	GLOB_RECURSE
	INCLUDES
	"inlude/**.h"
)

project("TcpUtils")

add_library(TcpUtils ${SRCS} ${INCLUDES})
target_include_directories(TcpUtils PUBLIC "include/")

set_property(TARGET TcpUtils PROPERTY CXX_STANDARD 23)

if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_SOURCE_DIR})
	add_subdirectory("test")
endif()

if(UNIX)
	find_package(OpenSSL REQUIRED)

	target_link_libraries(TcpUtils PRIVATE OpenSSL::SSL)
	target_compile_options(TcpUtils PUBLIC -fsanitize=address)
	target_link_options(TcpUtils PUBLIC -fsanitize=address)
endif()