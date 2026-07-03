# Shared test logic modules — each compiles to a static library
# that scenes link against to pull in their test cases.

add_subdirectory(smoke)
add_subdirectory(election)
add_subdirectory(fault)
add_subdirectory(mem_borrow)
add_subdirectory(npu)
