add_executable(unit_managers
    test_managers.cpp
)

add_executable(unit_cli
    test_cli.cpp
)

add_executable(unit_utils
    test_utils.cpp
)

add_executable(unit_config
    test_config.cpp
)

target_include_directories(unit_managers PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(unit_cli PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(unit_utils PRIVATE ${PROJECT_SOURCE_DIR}/include)
target_include_directories(unit_config PRIVATE ${PROJECT_SOURCE_DIR}/include)

target_link_libraries(unit_managers PRIVATE purpleman_core)
target_link_libraries(unit_cli PRIVATE purpleman_core)
target_link_libraries(unit_utils PRIVATE purpleman_core)
target_link_libraries(unit_config PRIVATE purpleman_core)

add_test(NAME unit_managers COMMAND unit_managers)
add_test(NAME unit_cli COMMAND unit_cli)
add_test(NAME unit_utils COMMAND unit_utils)
add_test(NAME unit_config COMMAND unit_config)
