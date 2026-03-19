include(${PROJECT_SOURCE_DIR}/scripts/cmake/mockcpp.cmake)
add_library(mockcpp INTERFACE)
target_include_directories(mockcpp SYSTEM INTERFACE ${DEPS_DIR}/mockcpp/include)
target_link_libraries(mockcpp INTERFACE ${DEPS_DIR}/mockcpp/lib/libmockcpp.a)

