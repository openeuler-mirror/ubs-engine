set(THIRD_PARTY_DIR ${PROJECT_SOURCE_DIR}/3rdparty)

add_subdirectory(hcom)
add_subdirectory(cpp-httplib)
add_subdirectory(rapidjson)
add_subdirectory(libxml2)
add_subdirectory(securec)

if (BUILD_TESTS)
    add_subdirectory(mockcpp)
endif ()