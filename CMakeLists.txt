cmake_minimum_required(VERSION 3.5)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(midi_foot_control)

add_custom_target(monitor
	COMMAND ${PYTHON} ${IDF_PATH}/tools/idf_monitor.py --port "/dev/ttyUSB0" ${IDF_PROJECT_EXECUTABLE}
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
)
