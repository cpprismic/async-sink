cmake_minimum_required(VERSION 3.14)
project(logger VERSION 1.0.0 LANGUAGES CXX)

# ── Header-only interface library ─────────────────────────────────────────────
add_library(logger INTERFACE)
add_library(logger::logger ALIAS logger)

target_include_directories(logger INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_features(logger INTERFACE cxx_std_17)

# Link pthreads on Unix
find_package(Threads REQUIRED)
target_link_libraries(logger INTERFACE Threads::Threads)

# ── Tests ──────────────────────────────────────────────────────────────────────
option(LOGGER_BUILD_TESTS "Build logger tests" ON)
if(LOGGER_BUILD_TESTS)
    enable_testing()
    add_executable(test_logger tests/test_logger.cpp)
    target_link_libraries(test_logger PRIVATE logger::logger)
    target_compile_features(test_logger PRIVATE cxx_std_17)
    add_test(NAME LoggerTests COMMAND test_logger)
endif()

# ── Demo (main.cpp) ────────────────────────────────────────────────────────────
option(LOGGER_BUILD_DEMO "Build logger demo" ON)
if(LOGGER_BUILD_DEMO)
    add_executable(logger_demo examples/demo.cpp)
    target_link_libraries(logger_demo PRIVATE logger::logger)
    target_compile_features(logger_demo PRIVATE cxx_std_17)
endif()

# ── Install ────────────────────────────────────────────────────────────────────
include(GNUInstallDirs)
install(TARGETS logger EXPORT loggerTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT loggerTargets
    FILE loggerTargets.cmake
    NAMESPACE logger::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/logger
)
