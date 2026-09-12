cmake_minimum_required(VERSION 3.15)
project(membyte CXX)

# C++17 es obligatorio: el navegador de carpetas usa <filesystem>.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# En Visual Studio, pegar el runtime de C++ DENTRO del .exe en vez de
# depender de DLLs del sistema. Sin esto, el ejecutable solo corre en
# computadoras que ya tengan instalado Visual Studio o el redistribuible.
# No afecta a g++/clang.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(membyte main.cpp membyte.cpp)
