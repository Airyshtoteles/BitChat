# Tests
add_executable(test_message    test_message.c)
add_executable(test_crypto     test_crypto_manager.c)
add_executable(test_mesh       test_mesh_router.c)
add_executable(test_discovery  test_node_discovery.c)

# Unity test framework
add_library(unity STATIC
    ${CMAKE_SOURCE_DIR}/third_party/unity/src/unity.c
)
target_include_directories(unity PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/unity/src
)

foreach(target test_message test_crypto test_mesh test_discovery)
    target_link_libraries(${target} PRIVATE bitchat unity)
    add_test(NAME ${target} COMMAND ${target})
endforeach()
