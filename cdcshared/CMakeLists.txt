#shared cdc library
project(cdcshared)

#declaration of the private implementation ${CMAKE_SOURCE_DIR}/src/CDCImplPri.h
#is intentionally kept outside of interface declarations

#platform dependant parts of the private implementation 
if (WIN32) 
	set(CDCPlatforSpec_SRC
		${clibcdc_CMAKE_SOURCE_DIR}/src/CDCImpl_Win.cpp
	)
else()
	set(CDCPlatforSpec_SRC
		${clibcdc_CMAKE_SOURCE_DIR}/src/CDCImpl_Lin.cpp
	)
endif()

# Specify source and header files.
set(cdc_SRC_FILES
	${CDCPlatforSpec_SRC}
	${clibcdc_CMAKE_SOURCE_DIR}/src/CDCImpl.cpp
	${clibcdc_CMAKE_SOURCE_DIR}/src/CDCImplException.cpp
	${clibcdc_CMAKE_SOURCE_DIR}/src/CDCMessageParser.cpp
	${clibcdc_CMAKE_SOURCE_DIR}/src/CDCMessageParserException.cpp
	${clibcdc_CMAKE_SOURCE_DIR}/src/CDCReceiveException.cpp
	${clibcdc_CMAKE_SOURCE_DIR}/src/CDCSendException.cpp
)

set(cdc_INC_FILES
	${clibcdc_CMAKE_SOURCE_DIR}/include/CDCImpl.h
	${clibcdc_CMAKE_SOURCE_DIR}/include/CDCImplException.h
	${clibcdc_CMAKE_SOURCE_DIR}/include/CdcInterface.h
	${clibcdc_CMAKE_SOURCE_DIR}/include/CDCMessageParser.h
	${clibcdc_CMAKE_SOURCE_DIR}/include/CDCMessageParserException.h
	${clibcdc_CMAKE_SOURCE_DIR}/include/CDCReceiveException.h
	${clibcdc_CMAKE_SOURCE_DIR}/include/CDCSendException.h
	${clibcdc_CMAKE_SOURCE_DIR}/include/CDCTypes.h
	${clibcdc_CMAKE_SOURCE_DIR}/src/CDCImplPri.h #declaration of private impl
)

# Group the files in IDE.
source_group("include" FILES ${cdc_INC_FILES})

include_directories(${clibcdc_CMAKE_SOURCE_DIR}/include)
include_directories(${clibcdc_CMAKE_SOURCE_DIR}/src) #declaration of private impl.

add_library(
	${PROJECT_NAME}
	SHARED
	${cdc_SRC_FILES} ${cdc_INC_FILES}
)

# link to pthread
#target_link_libraries(${project} pthread)

#install(TARGETS ${project} DESTINATION ${CMAKE_INSTALL_PREFIX}/lib)
