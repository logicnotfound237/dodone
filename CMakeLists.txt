cmake_minimum_required(VERSION 3.14)
project(dodone)

set(CMAKE_CXX_STANDARD 17)

# Add FTXUI from your local source
add_subdirectory(external/ftxui)

# Your app
add_executable(dodone main.cpp)

# Link against FTXUI modules
target_link_libraries(dodone PRIVATE ftxui::screen ftxui::dom ftxui::component)
