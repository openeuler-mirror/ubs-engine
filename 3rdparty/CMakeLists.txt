set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/3rdparty)

add_subdirectory(hcom)
add_subdirectory(mockcpp)
add_subdirectory(cpp-httplib)
add_subdirectory(rapidjson)
add_subdirectory(libxml2)
add_subdirectory(securec)

if (NOT (SOURCE_COMPILING AND B_VERSION))
    list(APPEND targets
            build_mockcpp)
endif ()

# 版本流水线不走 CMC 下载（MR 流水线和本地需要下载）
if (BUILD_IN_CI OR SOURCE_COMPILING)
    add_custom_target(3rdparty
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${targets}
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
else ()
    add_custom_target(3rdparty
            COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/build/download_dep.sh --enable-ub ${ENABLE_UB}
            COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target ${targets}
            COMMAND bash ${CMAKE_SOURCE_DIR}/scripts/build/install_dep.sh
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
endif ()