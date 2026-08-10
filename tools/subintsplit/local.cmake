# ---------------------------------------------------------------------------
# SubIntSplit benchmark
# ---------------------------------------------------------------------------

set(BTR_SUBINTSPLIT_DIR ${CMAKE_CURRENT_LIST_DIR})

# Links btrblocks only, deliberately: the playground tools pull in the AWS SDK
# for the whole tools tree, so this stays buildable with an explicit --target
# even when that is unavailable.
add_executable(subintsplit_bench ${BTR_SUBINTSPLIT_DIR}/subintsplit_bench.cpp)
target_link_libraries(subintsplit_bench btrblocks)
target_include_directories(subintsplit_bench PRIVATE ${BTR_SUBINTSPLIT_DIR})
