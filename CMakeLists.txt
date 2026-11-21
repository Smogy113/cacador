cmake_minimum_required(VERSION 3.20)
project(cacador CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
set(CUSTOMIZE_BUILD ON CACHE BOOL "" FORCE)
FetchContent_Declare(raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG 5.5 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(raylib)
# plano de corte proximo a 10 cm: muito mais precisao de profundidade (sombras e SSAO)
target_compile_definitions(raylib PUBLIC RL_CULL_DISTANCE_NEAR=0.1)
# a juncao final da tela le 5 texturas alem da cena (brilho, raios, oclusao, profundidade, desfoque)
target_compile_definitions(raylib PUBLIC RL_DEFAULT_BATCH_MAX_TEXTURE_UNITS=8)

# --- logica pura (sem raylib) ---
file(GLOB CORE_SRC CONFIGURE_DEPENDS src/core/*.cpp)
add_library(core STATIC ${CORE_SRC})
target_include_directories(core PUBLIC src)

# --- jogo ---
file(GLOB JOGO_SRC CONFIGURE_DEPENDS src/render/*.cpp src/game/*.cpp)
add_executable(cacador src/main.cpp ${JOGO_SRC})
target_link_libraries(cacador PRIVATE core raylib)
target_compile_definitions(cacador PRIVATE DATA_DIR="data")

if(WIN32)
  target_link_options(cacador PRIVATE -static -static-libgcc -static-libstdc++ -mwindows)
endif()

if(EMSCRIPTEN)
  set_target_properties(cacador PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  target_link_options(cacador PRIVATE -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=128MB
    -sASYNCIFY --preload-file ${CMAKE_SOURCE_DIR}/data@data)
endif()

# --- testes (so no build nativo) ---
if(NOT EMSCRIPTEN AND NOT WIN32)
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(doctest)
  file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
  add_executable(testes ${TEST_SRC})
  target_link_libraries(testes PRIVATE core doctest::doctest)
  target_compile_definitions(testes PRIVATE DATA_DIR="${CMAKE_SOURCE_DIR}/data")
endif()
