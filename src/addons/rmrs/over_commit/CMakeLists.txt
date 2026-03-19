file(GLOB_RECURSE OVER_COMMIT_SRCS *.cpp *.h)

add_library(over_commit STATIC ${OVER_COMMIT_SRCS})

FIND_INCLUDE_DIR(OVER_COMMIT_INCLUDE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(over_commit PRIVATE
        ${OVER_COMMIT_INCLUDE}
        ${PROJECT_SOURCE_DIR}/src/message/include
)
target_link_libraries(over_commit PRIVATE
        mempooling_message mempooling_internode_strategy mempooling_manager
)

target_sources(mempooling PRIVATE ${OVER_COMMIT_SRCS})
