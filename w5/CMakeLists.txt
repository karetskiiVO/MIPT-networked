cmake_minimum_required(VERSION 3.13)

project(w5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

SET(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CLIENT_SOURCES main.cpp protocol.cpp entity.cpp )
set(SERVER_SOURCES server.cpp protocol.cpp entity.cpp )

include_directories("../3rdParty/raylib/src")
include_directories("../3rdParty/enet/include")

if(MSVC)
    # https://github.com/raysan5/raylib/issues/857
    add_compile_definitions(NOVIRTUALKEYCODES NOWINMESSAGES NOWINSTYLES NOSYSMETRICS NOMENUS NOICONS NOKEYSTATES NOSYSCOMMANDS NORASTEROPS NOSHOWWINDOW OEMRESOURCE NOATOM NOCLIPBOARD NOCOLOR NOCTLMGR NODRAWTEXT NOGDI NOKERNEL NOUSER NOMB NOMEMMGR NOMETAFILE NOMINMAX NOMSG NOOPENFILE NOSCROLL NOSERVICE NOSOUND NOTEXTMETRIC NOWH NOWINOFFSETS NOCOMM NOKANJI NOHELP NOPROFILER NODEFERWINDOWPOS NOMCX)
endif()

add_executable(client ${CLIENT_SOURCES})
target_link_libraries(client PUBLIC project_options project_warnings)
target_link_libraries(client PUBLIC raylib enet)

add_executable(server ${SERVER_SOURCES})
target_link_libraries(server PUBLIC project_options project_warnings)
target_link_libraries(server PUBLIC enet)

if(MSVC)
    target_link_libraries(client PUBLIC ws2_32.lib winmm.lib)
    target_link_libraries(server PUBLIC ws2_32.lib winmm.lib)
endif()

