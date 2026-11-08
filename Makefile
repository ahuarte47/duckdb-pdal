PROJ_DIR := $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

# Configuration of extension
EXT_NAME=pdal
EXT_CONFIG=${PROJ_DIR}extension_config.cmake

# DuckDB v1.5.0 forces CMAKE_MSVC_RUNTIME_LIBRARY="MultiThreaded" (/MT, static
# CRT) in its CMakeLists.txt. All vcpkg packages must also use static CRT to
# avoid a CRT mismatch at link time.
#
# PDAL's vcpkg.json has "supports": "!(static & staticcrt)" which would block
# installation with the static-CRT triplet required by DuckDB v1.5.0 (/MT).
# --allow-unsupported bypasses that constraint so vcpkg proceeds with the build.
ifeq ($(DUCKDB_PLATFORM),windows_amd64)
	EXT_FLAGS += -DVCPKG_INSTALL_OPTIONS='--allow-unsupported'
	# Temporary release symbols for the Windows crash investigation.
	EXT_RELEASE_FLAGS += -DCMAKE_C_FLAGS_RELEASE='/O2 /Ob2 /DNDEBUG /Z7' -DCMAKE_CXX_FLAGS_RELEASE='/O2 /Ob2 /DNDEBUG /Z7'
	EXT_RELEASE_FLAGS += -DCMAKE_EXE_LINKER_FLAGS_RELEASE='/DEBUG /OPT:REF /OPT:ICF' -DCMAKE_SHARED_LINKER_FLAGS_RELEASE='/DEBUG /OPT:REF /OPT:ICF'
endif

# Include the Makefile from extension-ci-tools
include extension-ci-tools/makefiles/duckdb_extension.Makefile