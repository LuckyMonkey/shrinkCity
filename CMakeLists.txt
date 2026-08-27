cmake_minimum_required(VERSION 3.16)
project(shrink_city C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(SHRINK_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

add_library(shrink_sim STATIC sim/world.c sim/pathfinding.c sim/geometry.c)
target_include_directories(shrink_sim PUBLIC sim/include)
target_link_libraries(shrink_sim PUBLIC m)
target_compile_options(shrink_sim PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

if(SHRINK_ENABLE_SANITIZERS AND CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(shrink_sim PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(shrink_sim PUBLIC -fsanitize=address,undefined)
endif()

add_executable(shrink-sim tools/shrink_sim.c)
target_link_libraries(shrink-sim PRIVATE shrink_sim)
target_compile_options(shrink-sim PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)

enable_testing()
add_executable(shrink-tests tests/test_sim.c)
target_link_libraries(shrink-tests PRIVATE shrink_sim)
target_include_directories(shrink-tests PRIVATE sim)
target_compile_options(shrink-tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
add_test(NAME shrink-tests COMMAND shrink-tests)

add_test(NAME shrink-stream COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tools/test_stream.sh $<TARGET_FILE:shrink-sim>)
