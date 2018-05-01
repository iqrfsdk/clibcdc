project(ReadTemperatureExample)

set(read_temperature_example_SRC_FILES
	ReadTemperature.cpp
)

include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(${PROJECT_NAME} ${read_temperature_example_SRC_FILES})

if (WIN32) 
	target_link_libraries(${PROJECT_NAME} cdc)
else()
	target_link_libraries(${PROJECT_NAME} cdc pthread)
endif()

#install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_INSTALL_PREFIX}/sbin)
