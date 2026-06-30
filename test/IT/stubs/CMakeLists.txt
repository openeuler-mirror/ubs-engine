# -----------------------------
# IT test stub shared libraries
# Only built when BUILD_TESTS=ON
# -----------------------------
if(NOT BUILD_TESTS)
    return()
endif()

# OBMM stub - produces libobmm.so.1 so dlopen("libobmm.so.1") resolves via LD_LIBRARY_PATH
# VERSION/SOVERSION must match the real library's soname
add_library(obmm_stub SHARED obmm_stub.cpp)
set_target_properties(obmm_stub PROPERTIES
    OUTPUT_NAME "obmm"
    VERSION 1
    SOVERSION 1
)
target_compile_options(obmm_stub PRIVATE -fno-strict-aliasing)

# URMA UVS stub - produces libtpsa.so so dlopen("libtpsa.so") resolves via LD_LIBRARY_PATH
# The daemon loads libtpsa.so (which contains uvs_* functions), not liburma_uvs.so
add_library(urma_uvs_stub SHARED urma_uvs_stub.cpp)
set_target_properties(urma_uvs_stub PROPERTIES
    OUTPUT_NAME "tpsa"
)

# xalarm stub - produces libxalarm.so so dlopen("libxalarm.so") resolves via LD_LIBRARY_PATH
add_library(xalarm_stub SHARED xalarm_stub.cpp)
set_target_properties(xalarm_stub PROPERTIES
    OUTPUT_NAME "xalarm"
)

# Interface preload stub - produces libubse_interface_preload.so
# Only contains libc function overrides (syscall capget/capset, getgrnam ubm_nuds).
# Class method overrides for static libs (MTI/MMI/NodeController) were removed
# because LD_PRELOAD cannot intercept static-linked symbols.
add_library(ubse_interface_preload SHARED ubse_interface_preload.cpp)

add_library(ubse_capability_it_stub STATIC capability_stub.cpp)
target_include_directories(ubse_capability_it_stub PRIVATE
    ${PROJECT_SOURCE_DIR}/src/framework/security
    ${PROJECT_SOURCE_DIR}/src/include
)

# SMBIOS mock stub - STATIC library force-included via --whole-archive into ubse_it_daemon
# to override real ubse_smbios symbols at link time. No LD_PRELOAD needed.
add_subdirectory(smbios)
add_subdirectory(mti)
add_subdirectory(node_controller)
add_subdirectory(com_engine)
add_subdirectory(npu)
add_subdirectory(mock_plugin)
