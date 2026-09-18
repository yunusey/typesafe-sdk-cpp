find_package(GTest REQUIRED)
find_package(Threads REQUIRED)
include(GoogleTest)

set(TYPESAFE_TESTS contract_test examples_test unit_test async_test)

foreach(test ${TYPESAFE_TESTS})
  add_executable(${test} ${test}.cpp)
  target_link_libraries(${test} PRIVATE typesafe::sdk typesafe::testing GTest::gtest
                                        GTest::gtest_main Threads::Threads)
  target_include_directories(
    ${test} PRIVATE ${CMAKE_SOURCE_DIR}/examples # support/fixtures.hpp
                    ${CMAKE_SOURCE_DIR}/src)      # internal headers (logging.hpp)
  gtest_discover_tests(${test})
endforeach()
