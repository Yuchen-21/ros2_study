cmake_minimum_required(VERSION 3.16)
project(task_orchestration_concepts LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(fsm_delivery fsm_delivery.cpp)
add_executable(bt_delivery bt_delivery.cpp)
foreach(target fsm_delivery bt_delivery)
  target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()

enable_testing()
add_test(NAME fsm_transitions COMMAND fsm_delivery)
add_test(NAME bt_success COMMAND bt_delivery normal)
add_test(NAME bt_low_battery_interrupt COMMAND bt_delivery low_battery)
add_test(NAME bt_fallback COMMAND bt_delivery blocked)
