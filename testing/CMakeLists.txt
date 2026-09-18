add_library(typesafe_testing STATIC mock_server.cpp)
add_library(typesafe::testing ALIAS typesafe_testing)

target_include_directories(typesafe_testing
                           PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(typesafe_testing PUBLIC typesafe::sdk)
target_compile_features(typesafe_testing PUBLIC cxx_std_23)
