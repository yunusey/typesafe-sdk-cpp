find_package(Threads REQUIRED)

set(TYPESAFE_EXAMPLES
    system_one
    fan_out
    confidence_routing
    guardrails
    citation_check
    structured_state
    composite_scoring
    intent_routing
    noul_uncertainty
    blocking_triage
    async_fan_out)

foreach(example ${TYPESAFE_EXAMPLES})
  add_executable(${example} ${example}.cpp)
  target_link_libraries(${example} PRIVATE typesafe::sdk typesafe::testing Threads::Threads)
  target_include_directories(${example} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
  set_target_properties(${example} PROPERTIES RUNTIME_OUTPUT_DIRECTORY
                                              ${CMAKE_BINARY_DIR}/examples)
endforeach()
