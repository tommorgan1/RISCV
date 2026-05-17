include(FetchContent)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.6.0)
FetchContent_MakeAvailable(Catch2)

add_executable(emu_tests
    test_memory.cpp
)
target_link_libraries(emu_tests PRIVATE emu_core Catch2::Catch2WithMain)
include(Catch)
catch_discover_tests(emu_tests)


# compliance_runner depends on cpu.hpp — re-enable when Cpu is buildable
# add_executable(compliance_runner compliance/compliance_runner.cpp)
# target_link_libraries(compliance_runner PRIVATE emu_core)