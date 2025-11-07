set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/3rdparty)

add_subdirectory(hcom)
add_subdirectory(mockcpp)
add_subdirectory(libxml2)
add_subdirectory(secure_c)
add_subdirectory(cpp-httplib)

if (NOT (SOURCE_COMPILING AND B_VERSION))
    list(APPEND targets
            build_mockcpp)
endif ()
