# The Qt-free core of the capture application: the sample codec, the FLAC writer, the
# capture-file reader and the test-pattern analyser.
#
# Split out as a library so the tests can link it without a QApplication, and so the parts
# most worth testing are the parts with no UI wrapped around them.

add_library(ddd-common STATIC
    capturereader.cpp
    flacwriter.cpp
    testdataanalyser.cpp
)

target_include_directories(ddd-common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(ddd-common PRIVATE
    ${FLAC_INCLUDE_DIRS}
)

target_link_libraries(ddd-common PRIVATE
    ${FLAC_LIBRARIES}
)

# Position-independent so it can be linked into the application on every platform the
# packaging jobs build, some of which default to PIE.
set_target_properties(ddd-common PROPERTIES POSITION_INDEPENDENT_CODE ON)
