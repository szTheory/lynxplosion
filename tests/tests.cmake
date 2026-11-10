# Every test directory is include()d from here, never add_subdirectory()d, so
# the top-level directory's TESTS property holds every registered name and the
# inventory check in cmake/Tests.cmake sees all of them. Later plans append
# their include lines below.
include(${CMAKE_CURRENT_LIST_DIR}/abi/tests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/api/tests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/cpu/tests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/det/tests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/diag/tests.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/gates/docs.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/gates/inventory.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/gates/rights.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/gates/supply.cmake)
