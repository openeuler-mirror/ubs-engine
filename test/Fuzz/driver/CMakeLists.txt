# DT Fuzz 自研驱动（不依赖 libFuzzer，GCC/Clang 均可）
# harness 通过链接 ft_driver 获得 main()，只需实现 LLVMFuzzerTestOneInput。
add_library(ft_driver STATIC ft_driver.cpp)
target_include_directories(ft_driver PUBLIC ${CMAKE_CURRENT_LIST_DIR})
find_package(Threads REQUIRED)
target_link_libraries(ft_driver PUBLIC Threads::Threads)
# 覆盖率构建：fork 子进程 _exit 前强引用 __gcov_dump 手动落盘 gcda
if (FT_COVERAGE)
    target_compile_definitions(ft_driver PRIVATE FT_ENABLE_GCOV)
endif ()
