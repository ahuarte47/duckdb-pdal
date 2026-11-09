vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO PDAL/PDAL
    REF "${VERSION}"
    #[[
        Attention: pdal-dimbuilder must be updated together with pdal
    #]]
    SHA512 051e0a8ee2f03ad6eaf55b5843a41626ebc6d2517cf9eeedffda52878fbb9b7d854ea24174e2b3434f69ef41ab244e20e250f6c2c1083325b496489526fb8fea
    HEAD_REF master
    PATCHES
        dependencies.diff
        external-dimbuilder.diff
        find-library-suffix.diff
        no-rpath.patch
        rapidxml.diff
)

# Patch Arbiter to use constexpr for base64 and hex tables instead of std::string.
# Fix for Windows build with static CRT.
set(ARBITER_SOURCE "${SOURCE_PATH}/vendor/arbiter/arbiter.cpp")
file(READ "${ARBITER_SOURCE}" ARBITER_CONTENT)
set(ARBITER_OLD [=[
    const std::string base64Vals(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");

    const std::string hexVals("0123456789abcdef");]=])
string(FIND "${ARBITER_CONTENT}" "${ARBITER_OLD}" ARBITER_OLD_POSITION)
if(ARBITER_OLD_POSITION EQUAL -1)
    message(FATAL_ERROR "Could not locate Arbiter's base64/hex tables in ${ARBITER_SOURCE}")
endif()
set(ARBITER_NEW [=[
    constexpr char base64Vals[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    constexpr char hexVals[] = "0123456789abcdef";]=])
string(REPLACE "${ARBITER_OLD}" "${ARBITER_NEW}" ARBITER_CONTENT "${ARBITER_CONTENT}")
file(WRITE "${ARBITER_SOURCE}" "${ARBITER_CONTENT}")

# Patch PDAL's base64 encoder to use constexpr for the base64 table instead of std::string.
# Fix for Windows build with static CRT.
set(PDAL_UTILS_SOURCE "${SOURCE_PATH}/pdal/util/Utils.cpp")
file(READ "${PDAL_UTILS_SOURCE}" PDAL_UTILS_CONTENT)
set(PDAL_BASE64_ENCODE_OLD [=[
    const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    std::string ret;]=])
string(FIND "${PDAL_UTILS_CONTENT}" "${PDAL_BASE64_ENCODE_OLD}" PDAL_BASE64_ENCODE_POSITION)
if(PDAL_BASE64_ENCODE_POSITION EQUAL -1)
    message(FATAL_ERROR "Could not locate PDAL's base64 encoder table in ${PDAL_UTILS_SOURCE}")
endif()
set(PDAL_BASE64_ENCODE_NEW [=[
    constexpr char base64_chars[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    std::string ret;]=])
string(REPLACE "${PDAL_BASE64_ENCODE_OLD}" "${PDAL_BASE64_ENCODE_NEW}" PDAL_UTILS_CONTENT "${PDAL_UTILS_CONTENT}")
file(WRITE "${PDAL_UTILS_SOURCE}" "${PDAL_UTILS_CONTENT}")

# Build PDAL and its bundled libraries as static libraries.
file(READ "${SOURCE_PATH}/cmake/libraries.cmake" LIBS_CONTENT)
string(REPLACE "set(PDAL_LIB_TYPE \"SHARED\")" "set(PDAL_LIB_TYPE \"STATIC\")" LIBS_CONTENT "${LIBS_CONTENT}")
file(WRITE "${SOURCE_PATH}/cmake/libraries.cmake" "${LIBS_CONTENT}")

file(READ "${SOURCE_PATH}/cmake/macros.cmake" MACROS_CONTENT)
string(REPLACE "    endif()\nendmacro(PDAL_ADD_FREE_LIBRARY)" "    endif()\n    install(TARGETS \${_name}\n        EXPORT PDALTargets\n        RUNTIME DESTINATION \${CMAKE_INSTALL_BINDIR}\n        LIBRARY DESTINATION \${CMAKE_INSTALL_LIBDIR}\n        ARCHIVE DESTINATION \${CMAKE_INSTALL_LIBDIR})\nendmacro(PDAL_ADD_FREE_LIBRARY)" MACROS_CONTENT "${MACROS_CONTENT}")
file(WRITE "${SOURCE_PATH}/cmake/macros.cmake" "${MACROS_CONTENT}")

file(READ "${SOURCE_PATH}/CMakeLists.txt" CMAKE_CONTENT)
string(REPLACE "export(\n    TARGETS\n        \${PDAL_LIB_NAME}\n    FILE\n        \"\${PDAL_BINARY_DIR}/PDALTargets.cmake\")" "export(\n    TARGETS\n        \${PDAL_LIB_NAME}\n        \${PDAL_ARBITER_LIB_NAME}\n                \${PDAL_KAZHDAN_LIB_NAME}\n        \${PDAL_LEPCC_LIB_NAME}\n        \${PDAL_LAZPERF_LIB_NAME}\n    FILE\n        \"\${PDAL_BINARY_DIR}/PDALTargets.cmake\")" CMAKE_CONTENT "${CMAKE_CONTENT}")
file(WRITE "${SOURCE_PATH}/CMakeLists.txt" "${CMAKE_CONTENT}")

file(REMOVE_RECURSE
    "${SOURCE_PATH}/cmake/modules/FindCurl.cmake"
    "${SOURCE_PATH}/cmake/modules/FindGeoTIFF.cmake"
    "${SOURCE_PATH}/cmake/modules/FindICONV.cmake"
    "${SOURCE_PATH}/cmake/modules/FindZSTD.cmake"
    "${SOURCE_PATH}/plugins/e57/libE57Format"
    "${SOURCE_PATH}/vendor/eigen"
    "${SOURCE_PATH}/vendor/h3"
    "${SOURCE_PATH}/vendor/nanoflann"
    "${SOURCE_PATH}/vendor/nlohmann"
    "${SOURCE_PATH}/vendor/schema-validator"
    "${SOURCE_PATH}/vendor/utfcpp"
)
# PDAL includes "h3api.h", and some calls are decorated with PDALH3
file(COPY "${CURRENT_PORT_DIR}/h3api.h" DESTINATION "${SOURCE_PATH}")
# PDAL uses namespace 'NL' for nlohmann
file(COPY "${CURRENT_INSTALLED_DIR}/include/nlohmann" DESTINATION "${SOURCE_PATH}/vendor/nlohmann/")
file(APPEND "${SOURCE_PATH}/vendor/nlohmann/nlohmann/json.hpp" "\nnamespace NL = nlohmann;\n")
file(APPEND "${SOURCE_PATH}/vendor/nlohmann/nlohmann/json_fwd.hpp" "\nnamespace NL = nlohmann;\n")
file(WRITE "${SOURCE_PATH}/pdal/JsonFwd.hpp" "/* vcpkg redacted */\n#include <nlohmann/json_fwd.hpp>\nnamespace NL = nlohmann;\n")
file(MAKE_DIRECTORY "${SOURCE_PATH}/vendor/nlohmann/schema-validator")
file(WRITE "${SOURCE_PATH}/vendor/nlohmann/schema-validator/json-schema.hpp" "/* vcpkg redacted */\n#include <nlohmann/json-schema.hpp>\n")
# PDAL vendors arbiter vendors rapidxml
file(COPY "${CURRENT_INSTALLED_DIR}/include/rapidxml/rapidxml.hpp" DESTINATION "${SOURCE_PATH}/vendor/arbiter/")

unset(ENV{OSGEO4W_HOME})

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        draco       BUILD_PLUGIN_DRACO
        e57         BUILD_PLUGIN_E57
        hdf5        BUILD_PLUGIN_HDF
        lzma        WITH_LZMA
        pgpointcloud BUILD_PLUGIN_PGPOINTCLOUD
        spz         BUILD_PLUGIN_SPZ
        zstd        WITH_ZSTD
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        "-DCMAKE_PROJECT_INCLUDE=${CMAKE_CURRENT_LIST_DIR}/cmake-project-include.cmake"
        "-DDIMBUILDER_EXECUTABLE=${CURRENT_HOST_INSTALLED_DIR}/manual-tools/pdal-dimbuilder/${VERSION}/dimbuilder${VCPKG_HOST_EXECUTABLE_SUFFIX}"
        -DPDAL_PLUGIN_INSTALL_PATH=.
        -DWITH_TESTS:BOOL=OFF
        -DWITH_COMPLETION:BOOL=OFF
        -DCMAKE_DISABLE_FIND_PACKAGE_Libexecinfo:BOOL=ON
        -DCMAKE_DISABLE_FIND_PACKAGE_Libunwind:BOOL=ON
        ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/PDAL)
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()

# Install and cleanup executables
file(GLOB pdal_unsupported
    "${CURRENT_PACKAGES_DIR}/bin/*.bat"
    "${CURRENT_PACKAGES_DIR}/bin/pdal-config"
    "${CURRENT_PACKAGES_DIR}/debug/bin/*.bat"
    "${CURRENT_PACKAGES_DIR}/debug/bin/*.exe"
    "${CURRENT_PACKAGES_DIR}/debug/bin/pdal-config"
)
file(REMOVE ${pdal_unsupported})
vcpkg_copy_tools(TOOL_NAMES pdal AUTO_CLEAN)

# Post-install clean-up
file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/include/pdal/filters/private/csf"
    "${CURRENT_PACKAGES_DIR}/include/pdal/filters/private/miniball"
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
)

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")

set(arbiter_license "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/arbiter LICENSE")
file(COPY_FILE "${SOURCE_PATH}/vendor/arbiter/LICENSE" "${arbiter_license}")

set(kazhdan_license "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/kazhdan license (PoissonRecon.h)")
file(READ "${SOURCE_PATH}/vendor/kazhdan/PoissonRecon.h" license)
string(REGEX REPLACE "^/\\*\n|\\*/.*\$" "" license "${license}")
file(WRITE "${kazhdan_license}" "${license}")

set(lazperf_license "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/lazperf license (lazperf.hpp)")
file(READ "${SOURCE_PATH}/vendor/lazperf/lazperf.hpp" license)
string(REGEX REPLACE "^/\\*\n|\\*/.*\$" "" license "${license}")
file(WRITE "${lazperf_license}" "${license}")

set(lepcc_license "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-rel/LEPCC license (LEPCC.h)")
file(READ "${SOURCE_PATH}/vendor/lepcc/src/LEPCC.h" license)
string(REGEX REPLACE "^/\\*\n|\\*/.*\$" "" license "${license}")
file(WRITE "${lepcc_license}" "${license}")

vcpkg_install_copyright(FILE_LIST
    "${SOURCE_PATH}/LICENSE.txt"
    "${arbiter_license}"
    "${kazhdan_license}"
    "${lazperf_license}"
    "${lepcc_license}"
)
