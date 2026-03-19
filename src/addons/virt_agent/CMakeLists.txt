add_subdirectory(default_strategy)
add_subdirectory(common/message)
add_subdirectory(sdk)

file(GLOB_RECURSE VM_CPP "*.cpp")
file(GLOB_RECURSE SDK_C "sdk/*.c*")
list(REMOVE_ITEM VM_CPP ${SDK_C})

# Add exposed rack dependencies
include_directories(${PROJECT_SOURCE_DIR}/include)

# Add dependencies for the virt_agent plugin
FIND_INCLUDE_DIR(VM_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})
include_directories(${VM_INCLUDE})

# Add the virt_agent plugin shared library
add_library(vm SHARED ${VM_CPP})
target_include_directories(vm PUBLIC ${VM_INCLUDE}
        ${PROJECT_SOURCE_DIR}/src/include
        ${PROJECT_SOURCE_DIR}/src/framework/config
        ${PROJECT_SOURCE_DIR}/src/framework/serde
        )
target_link_libraries(vm PUBLIC rapidjson boundscheck)

file(COPY conf/plugin_vm.conf DESTINATION ${CMAKE_BINARY_DIR}/expand_conf FILE_PERMISSIONS OWNER_READ OWNER_WRITE)