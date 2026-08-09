add_executable(test_fuzzy_c
    test_fuzzy_c.c
)
target_link_libraries(test_fuzzy_c PRIVATE myController2::fuzzy_controller)
add_test(NAME fuzzy_c_api COMMAND test_fuzzy_c)

add_executable(test_fuzzy_cpp
    test_fuzzy_cpp.cpp
)
target_link_libraries(test_fuzzy_cpp PRIVATE myController2::fuzzy_controller)
add_test(NAME fuzzy_cpp_api COMMAND test_fuzzy_cpp)

add_executable(test_config_manager
    ../Part_ConfigManager_SmokeTest.c
)
target_link_libraries(test_config_manager PRIVATE myController2::fuzzy_controller)
add_test(NAME fuzzy_config_manager COMMAND test_config_manager)

add_executable(test_identified_plant
    ../Part10_ClosedLoop_PlantTest.c
)
target_link_libraries(test_identified_plant PRIVATE
    myController2::fuzzy_controller
    myController2::thermal_plant
)
add_test(NAME identified_plant_smoke COMMAND test_identified_plant)
