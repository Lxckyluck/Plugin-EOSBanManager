cmake_minimum_required(VERSION 3.20)
project(EOSBanManager CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Chemins à adapter à ton environnement
set(ASAAPI_ROOT "${CMAKE_SOURCE_DIR}/../AsaApi"  CACHE PATH "Racine du SDK AsaApi")
set(MYSQL_ROOT  "C:/mysql-8.0.27-winx64/mysql-8.0.27-winx64" CACHE PATH "Racine du Connector MySQL C")

add_library(EOSBanManager SHARED
    src/dllmain.cpp
    src/Plugin.cpp
    src/Database.cpp
    src/Commands.cpp
    src/Hooks.cpp
)

target_include_directories(EOSBanManager PRIVATE
    include
    ${ASAAPI_ROOT}/AsaApi/Core/Public
    ${ASAAPI_ROOT}/AsaApi/Core/Public/API
    ${ASAAPI_ROOT}/AsaApi/Core/Public/API/UE
    ${MYSQL_ROOT}/include
)

target_link_directories(EOSBanManager PRIVATE
    ${ASAAPI_ROOT}/x64/Release
    ${MYSQL_ROOT}/lib
)

target_link_libraries(EOSBanManager PRIVATE
    AsaApi
    libmysql
)

if (MSVC)
    target_compile_options(EOSBanManager PRIVATE /W4 /permissive- /MT)
    target_compile_definitions(EOSBanManager PRIVATE WIN32 _WINDOWS NDEBUG _USRDLL EOSBANMANAGER_EXPORTS)
endif()
