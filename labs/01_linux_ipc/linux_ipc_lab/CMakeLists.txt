cmake_minimum_required(VERSION 3.16)
project(linux_ipc_lab VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)
# Keeping ament optional makes these POSIX-only labs useful before ROS is installed,
# while the same package remains discoverable by colcon on ROS2 Humble.
find_package(ament_cmake QUIET)

function(add_ipc_lab executable_name source_file)
  add_executable(${executable_name} ${source_file})
  target_include_directories(
    ${executable_name}
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/include
  )
  target_compile_options(
    ${executable_name}
    PRIVATE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wshadow
  )
  target_link_libraries(${executable_name} PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # glibc 2.34 folded librt into libc; this compatibility link also supports
    # older Linux toolchains where shm_open/clock_gettime still live in librt.
    target_link_libraries(${executable_name} PRIVATE rt)
  endif()

  install(TARGETS ${executable_name} DESTINATION lib/${PROJECT_NAME})
endfunction()

add_ipc_lab(thread_shared_state src/thread_shared_state.cpp)
add_ipc_lab(process_memory_isolation src/process_memory_isolation.cpp)
add_ipc_lab(pipe_process_a src/pipe_process_a.cpp)
add_ipc_lab(pipe_process_b src/pipe_process_b.cpp)
add_ipc_lab(unix_domain_subscriber src/unix_domain_subscriber.cpp)
add_ipc_lab(unix_domain_publisher src/unix_domain_publisher.cpp)
add_ipc_lab(tcp_subscriber src/tcp_subscriber.cpp)
add_ipc_lab(tcp_publisher src/tcp_publisher.cpp)
add_ipc_lab(udp_subscriber src/udp_subscriber.cpp)
add_ipc_lab(udp_publisher src/udp_publisher.cpp)
add_ipc_lab(shared_memory_writer src/shared_memory_writer.cpp)
add_ipc_lab(shared_memory_reader src/shared_memory_reader.cpp)

install(DIRECTORY include/ DESTINATION include)

include(CTest)
if(BUILD_TESTING)
  add_test(NAME thread_shared_state_test COMMAND thread_shared_state --iterations 2000)
  add_test(NAME process_memory_isolation_test COMMAND process_memory_isolation)
  add_test(NAME pipe_process_test COMMAND pipe_process_a --count 3 --interval-ms 1)
  add_test(
    NAME ipc_pairs_integration_test
    COMMAND
      bash
      ${CMAKE_CURRENT_SOURCE_DIR}/test/run_ipc_pairs.sh
      $<TARGET_FILE_DIR:thread_shared_state>
  )
  set_tests_properties(ipc_pairs_integration_test PROPERTIES TIMEOUT 30)
endif()

if(ament_cmake_FOUND)
  ament_package()
else()
  message(STATUS "ament_cmake not found: building linux_ipc_lab as standalone CMake")
endif()
