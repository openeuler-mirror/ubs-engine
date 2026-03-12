file(GLOB_RECURSE DS_CPP *.cpp)

# Add the default_strategy plugin shared library
add_library(strategy SHARED ${DS_CPP})

target_include_directories(strategy PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(strategy PUBLIC)

install(
        TARGETS strategy
        DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        COMPONENT strategy
)