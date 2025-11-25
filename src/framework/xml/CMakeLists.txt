add_library(ubse_xml STATIC
        ubse_xml.cpp
)

target_include_directories(ubse_xml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_xml PUBLIC libxml2 ubse_utils)