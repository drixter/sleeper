
cmake_minimum_required(VERSION 3.16)
project(sleeper C)

set(CMAKE_C_STANDARD 11)

add_executable(sleeper sleep_progress.c)

# Nothing extra needed for Windows; kernel32 is linked by default for console apps.
# If you split files:
# add_executable(sleeper sleep_progress_win.c)  # for Windows-only version
