cmake_minimum_required(VERSION 3.29)

project(
	Turing
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)

add_executable(${CMAKE_PROJECT_NAME} turing.cxx)

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})

install(
	TARGETS ${CMAKE_PROJECT_NAME}
	RUNTIME DESTINATION bin
)

install(
	DIRECTORY data/
	DESTINATION bin/
)