add_subdirectory(message)
add_library(ubse_election STATIC
        ubse_election.cpp
        ubse_election_module.cpp
        ubse_election_node_mgr.cpp
        ubse_election_comm_mgr.cpp
        ubse_election_pkt_handler.cpp
        ubse_election_utils.cpp
        role/ubse_election_role.cpp
        role/ubse_election_role_mgr.cpp
        role/ubse_election_role_master.cpp
        role/ubse_election_role_standby.cpp
        role/ubse_election_role_agent.cpp
        role/ubse_election_role_initializer.cpp
        role/ubse_election_role_global_initializer.cpp
        role/ubse_election_role_global_agent.cpp
        role/ubse_election_role_global_standby.cpp
        role/ubse_election_role_global_master.cpp
)

target_include_directories(ubse_election PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(ubse_election PUBLIC ubse_election_message
        PRIVATE ubse_com ubse_http ubse_context ubse_config ubse_serial api_server ubse_node_controller ubse_node_mgr)
