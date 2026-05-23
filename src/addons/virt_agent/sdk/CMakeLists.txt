# make library
file(GLOB_RECURSE ALL_SOURCES CONFIGURE_DEPENDS "*.cpp" "*.c")
# exclude test file
list(FILTER ALL_SOURCES EXCLUDE REGEX "/sample/")
add_library(ubs_virt_agent_sdk SHARED ${ALL_SOURCES})
# include directory
FIND_INCLUDE_DIR(SDK_PRIVATE_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})
list(FILTER SDK_PRIVATE_INCLUDE EXCLUDE REGEX "include")
target_include_directories(ubs_virt_agent_sdk
        PRIVATE ${SDK_PRIVATE_INCLUDE}
        PRIVATE ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/include
        PRIVATE ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/common
        PRIVATE ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/common/message
        PRIVATE ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/common/message/sdk
        PRIVATE ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/common/mempooling
        PRIVATE ${PROJECT_SOURCE_DIR}/src/addons/virt_agent/common/util
        PUBLIC include
)
# link deps library
target_link_libraries(ubs_virt_agent_sdk PRIVATE ubse_ipc_client virt_agent_sdk_message)
# set deps library link option
target_link_options(ubs_virt_agent_sdk PRIVATE -Wl,--no-whole-archive)
# set library property
set_target_properties(ubs_virt_agent_sdk PROPERTIES
        OUTPUT_NAME "ubs-virt-agent"
        VERSION 1.0.1    # full version, exp: libxxx.so.1.0.1
        SOVERSION 1      # main version, exp: libxxx.so.1 -> libxxx.so.1.0.1
)

# test
#add_executable(ubs_virt_agent_ham_migrate_sample EXCLUDE_FROM_ALL ham_migrate/sample/libvirt_agent_ham_migrate_sample.c)
#target_link_libraries(ubs_virt_agent_ham_migrate_sample PRIVATE ubs_virt_agent_sdk)
#
#add_executable(virt_mem_fragmentation EXCLUDE_FROM_ALL mem_fragmentation/sample/libvirt_agent_mem_fragmentation_sample.c)
#target_link_libraries(virt_mem_fragmentation PRIVATE ubs_virt_agent_sdk)
