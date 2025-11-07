include(FetchContent)

set(CPPHTTPLIB_REPO_DEFAULT "https://gitee.com/mirrors_yhirose/cpp-httplib.git")
set(CPPHTTPLIB_TAG_DEFAULT "v0.27.0")

if (NOT DEFINED CPPHTTPLIB_REPO)
    if (DEFINED ENV{CPPHTTPLIB_REPO})
        set(CPPHTTPLIB_REPO "$ENV{CPPHTTPLIB_REPO}")
    else ()
        set(CPPHTTPLIB_REPO "${CPPHTTPLIB_REPO_DEFAULT}")
    endif ()
endif ()

if (NOT DEFINED CPPHTTPLIB_TAG)
    if (DEFINED ENV{CPPHTTPLIB_TAG})
        set(CPPHTTPLIB_TAG "$ENV{CPPHTTPLIB_TAG}")
    else ()
        set(CPPHTTPLIB_TAG "${CPPHTTPLIB_TAG_DEFAULT}")
    endif ()
endif ()

message(STATUS "cpp-httplib: REPO=${CPPHTTPLIB_REPO}, TAG=${CPPHTTPLIB_TAG}")

FetchContent_Declare(
        cpp-httplib
        GIT_REPOSITORY ${CPPHTTPLIB_REPO}
        GIT_TAG ${CPPHTTPLIB_TAG}
)

FetchContent_MakeAvailable(cpp-httplib)
