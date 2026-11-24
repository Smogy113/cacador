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
set(SUPPORT_FILEFORMAT_JPG ON CACHE BOOL "" FORCE)
set(SUPPORT_FILEFORMAT_OGG ON CACHE BOOL "" FORCE)
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
# cronometro da placa (consultas de tempo do OpenGL pelo carregador que a raylib ja traz)
target_include_directories(cacador PRIVATE ${raylib_SOURCE_DIR}/src)
target_compile_definitions(cacador PRIVATE DATA_DIR="data")

if(WIN32)
  target_link_options(cacador PRIVATE -static -static-libgcc -static-libstdc++ -mwindows)
endif()

if(EMSCRIPTEN)
  set_target_properties(cacador PROPERTIES OUTPUT_NAME index SUFFIX ".html")
  # O jogo roda por emscripten_set_main_loop, entao o ASYNCIFY e so uma garantia. Desligado
  # (-DCACADOR_WEB_ASYNCIFY=OFF), o link nao precisa do wasm-emscripten-finalize, que o Smart App
  # Control de alguns Windows bloqueia.
  option(CACADOR_WEB_ASYNCIFY "Liga -sASYNCIFY na versao web" ON)
  if(CACADOR_WEB_ASYNCIFY)
    target_link_options(cacador PRIVATE -sASYNCIFY)
  endif()
  # O backend de audio da raylib le o buffer de amostras por Module.HEAPF32.
  target_link_options(cacador PRIVATE -sUSE_GLFW=3 -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=128MB
    -sEXPORTED_RUNTIME_METHODS=HEAPF32
    # argumentos pela URL (index.html?--direto&--qualidade&baixo): ver web/argumentos.js
    --pre-js ${CMAKE_SOURCE_DIR}/web/argumentos.js
    --preload-file ${CMAKE_SOURCE_DIR}/data@data --exclude-file "${CMAKE_SOURCE_DIR}/data/solo")
endif()

# --- testes ---
# Build nativo (Linux, ou Windows com MinGW): ./build/testes. Fora da compilacao cruzada para Windows.
# Na web (Emscripten) os testes viram testes.js e rodam no Node: node build-web/testes.js
# (serve para PCs que nao deixam rodar executaveis novos, como Windows com o Smart App Control ligado).
if(EMSCRIPTEN OR NOT CMAKE_CROSSCOMPILING)
  # O CMakeLists do doctest 2.4.11 pede uma versao antiga; o CMake 4 so aceita com este piso.
  set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
  FetchContent_Declare(doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11 GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(doctest)
  file(GLOB TEST_SRC CONFIGURE_DEPENDS tests/*.cpp)
  add_executable(testes ${TEST_SRC})
  target_link_libraries(testes PRIVATE core doctest::doctest)
  target_compile_definitions(testes PRIVATE DATA_DIR="${CMAKE_SOURCE_DIR}/data")
  if(EMSCRIPTEN)
    # NODERAWFS: le data/ direto do disco; excecoes ligadas para o REQUIRE do doctest.
    target_compile_options(testes PRIVATE -fexceptions)
    target_link_options(testes PRIVATE -fexceptions -sNODERAWFS=1 -sENVIRONMENT=node -sALLOW_MEMORY_GROWTH=1
      -sSTACK_SIZE=8MB -sEXIT_RUNTIME=1)
  endif()
endif()
