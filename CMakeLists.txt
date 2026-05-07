cmake_minimum_required(VERSION 3.14)
project(crud_app LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# nlohmann/json 자동 다운로드
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

add_executable(crud_app main.cpp)
target_link_libraries(crud_app PRIVATE nlohmann_json::nlohmann_json)

# MSVC 한글 출력을 위한 UTF-8 설정
if(MSVC)
    target_compile_options(crud_app PRIVATE /utf-8)
endif()
