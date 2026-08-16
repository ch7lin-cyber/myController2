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
if(NOT MSVC)
    target_link_libraries(test_config_manager PRIVATE m)
endif()
add_test(NAME fuzzy_config_manager COMMAND test_config_manager)

add_executable(test_identified_plant
    ../Part10_ClosedLoop_PlantTest.c
)
target_link_libraries(test_identified_plant PRIVATE
    myController2::fuzzy_controller
    myController2::thermal_plant
)
add_test(NAME identified_plant_smoke COMMAND test_identified_plant)

add_executable(test_fuzzy_self_tuner
    test_fuzzy_self_tuner.c
)
target_link_libraries(test_fuzzy_self_tuner PRIVATE myController2::fuzzy_controller)
add_test(NAME fuzzy_self_tuner COMMAND test_fuzzy_self_tuner)

add_executable(test_fuzzy_self_tuning_bridge
    test_fuzzy_self_tuning_bridge.c
)
target_link_libraries(test_fuzzy_self_tuning_bridge PRIVATE myController2::fuzzy_controller)
add_test(NAME fuzzy_self_tuning_bridge COMMAND test_fuzzy_self_tuning_bridge)

add_executable(test_fuzzy_temperature_profile
    test_fuzzy_temperature_profile.c
)
target_link_libraries(test_fuzzy_temperature_profile PRIVATE myController2::fuzzy_controller)
add_test(NAME fuzzy_temperature_profile COMMAND test_fuzzy_temperature_profile)

add_executable(test_fuzzy_profile_recommendation
    test_fuzzy_profile_recommendation.c
)
target_link_libraries(test_fuzzy_profile_recommendation PRIVATE myController2::fuzzy_controller)
add_test(NAME fuzzy_profile_recommendation COMMAND test_fuzzy_profile_recommendation)

if(MYCONTROLLER_BUILD_PC_CONTROL)
    add_executable(test_fuzzy_pc_bridge
        test_fuzzy_pc_bridge.c
    )
    target_link_libraries(test_fuzzy_pc_bridge PRIVATE fuzzy_pc_bridge)
    add_test(NAME fuzzy_pc_bridge_api COMMAND test_fuzzy_pc_bridge)
endif()

if(WIN32 AND MYCONTROLLER_BUILD_SHARED)
    set(_fuzzy_dll_path "$<TARGET_FILE_DIR:fuzzy_controller>")
    set_tests_properties(
        fuzzy_c_api
        fuzzy_cpp_api
        fuzzy_config_manager
        identified_plant_smoke
        fuzzy_self_tuner
        fuzzy_self_tuning_bridge
        fuzzy_temperature_profile
        fuzzy_profile_recommendation
        PROPERTIES ENVIRONMENT "PATH=${_fuzzy_dll_path};$ENV{PATH}"
    )
endif()

if(WIN32 AND MYCONTROLLER_BUILD_PC_CONTROL)
    set(_pc_bridge_dll_path "$<TARGET_FILE_DIR:fuzzy_pc_bridge>")
    set_tests_properties(
        fuzzy_pc_bridge_api
        PROPERTIES ENVIRONMENT "PATH=${_pc_bridge_dll_path};$ENV{PATH}"
    )
endif()
