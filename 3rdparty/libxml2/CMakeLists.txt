find_package(LibXml2 REQUIRED)

if(NOT LIBXML2_FOUND)
    message(FATAL_ERROR "libxml2 not found! Please execute:
                sudo yum install libxml2-devel-2.11.9")
endif()

add_library(libxml2 INTERFACE)
target_include_directories(libxml2 SYSTEM INTERFACE ${LIBXML2_INCLUDE_DIRS})
target_link_libraries(libxml2 INTERFACE ${LIBXML2_LIBRARIES})
