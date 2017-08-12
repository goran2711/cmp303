cmake_minimum_required(VERSION 2.6)
project(networking)

add_definitions(-DSMFL_STATIC)
set(EXEC_NAME "networking-paddles")

file(GLOB SOURCES "*.cpp")
# file(GLOB INC "*.h")

add_executable(${EXEC_NAME} ${SOURCES})

find_package(SFML REQUIRED graphics network system window)

if(SFML_FOUND)
		include_directories(${SFML_INCLUDE_DIR})
		target_link_libraries(${EXEC_NAME} ${SFML_LIBRARIES} ${SFML_DEPENDENCIES})
endif(SFML_FOUND)

find_package(Threads)
target_link_libraries(${EXEC_NAME} ${CMAKE_THREAD_LIBS_INIT})
