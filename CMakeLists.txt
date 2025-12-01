cmake_minimum_required(VERSION 3.5)

# Set extension name here
set(TARGET_NAME pdal)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# DuckDB's extension distribution supports vcpkg. As such, dependencies can be added in ./vcpkg.json and then
# used in cmake with find_package. Feel free to remove or replace with other dependencies.
# Note that it should also be removed from vcpkg.json to prevent needlessly installing it.

# Find required dependencies for PDAL static linking
find_package(Eigen3 CONFIG REQUIRED)
find_package(nlohmann_json_schema_validator CONFIG REQUIRED)
find_package(utf8cpp CONFIG REQUIRED)
find_package(nanoflann CONFIG REQUIRED)
find_package(LibXml2 CONFIG REQUIRED)
find_package(GDAL CONFIG REQUIRED)
find_package(PDAL CONFIG REQUIRED)

if (TARGET PDAL::pdalcpp)
    set(PDAL_TARGET PDAL::pdalcpp)
elseif (TARGET PDAL::PDAL)
    set(PDAL_TARGET PDAL::PDAL)
else()
    message(WARNING "PDAL target not found, using variables PDAL_LIBRARIES/PDAL_INCLUDE_DIRS")
    set(PDAL_TARGET ${PDAL_LIBRARIES})
    include_directories(${PDAL_INCLUDE_DIRS})
endif()

set(EXTENSION_NAME ${TARGET_NAME}_extension)
set(LOADABLE_EXTENSION_NAME ${TARGET_NAME}_loadable_extension)

project(${TARGET_NAME})
include_directories(src/include)

set(EXTENSION_SOURCES src/pdal_extension.cpp)
add_subdirectory(src/pdal)

build_static_extension(${TARGET_NAME} ${EXTENSION_SOURCES})
build_loadable_extension(${TARGET_NAME} " " ${EXTENSION_SOURCES})

# Include PDAL directories in the target
target_include_directories(${EXTENSION_NAME} PRIVATE
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:pdalcpp,INTERFACE_INCLUDE_DIRECTORIES>>
)
target_include_directories(${LOADABLE_EXTENSION_NAME} PRIVATE
    $<BUILD_INTERFACE:$<TARGET_PROPERTY:pdalcpp,INTERFACE_INCLUDE_DIRECTORIES>>
)

# Add PDAL base directory to allow relative includes like <filters/StatsFilter.hpp>
if(VCPKG_TOOLCHAIN)
    target_include_directories(${EXTENSION_NAME} PRIVATE
        ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/pdal
    )
    target_include_directories(${LOADABLE_EXTENSION_NAME} PRIVATE
        ${CMAKE_BINARY_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}/include/pdal
    )
endif()

# Link PDAL in both the static library as the loadable extension
target_link_libraries(${EXTENSION_NAME} ${PDAL_TARGET} GDAL::GDAL)
target_link_libraries(${LOADABLE_EXTENSION_NAME} ${PDAL_TARGET} GDAL::GDAL)

install(
  TARGETS ${EXTENSION_NAME}
  EXPORT "${DUCKDB_EXPORT_SET}"
  LIBRARY DESTINATION "${INSTALL_LIB_DIR}"
  ARCHIVE DESTINATION "${INSTALL_LIB_DIR}")
