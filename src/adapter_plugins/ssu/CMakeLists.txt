aux_source_directory(${CMAKE_CURRENT_SOURCE_DIR} SSU_SRC)
add_library(ubse_ssu STATIC ${SSU_SRC})
target_include_directories(ubse_ssu PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../include/adapter_plugins/ssu
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBBLOCKDEV REQUIRED blockdev)
pkg_check_modules(GLIB REQUIRED glib-2.0)

target_link_libraries(ubse_ssu
        PUBLIC
        -lbd_lvm
        -lbd_mdraid
        PRIVATE
        ubse_log
        ubse_utils
        ubse_config
        ${LIBBLOCKDEV_LIBRARIES}
        ${GLIB_LIBRARIES}
)
target_include_directories(ubse_ssu PRIVATE ${LIBBLOCKDEV_INCLUDE_DIRS} ${GLIB_INCLUDE_DIRS})