cmake_minimum_required(VERSION 3.21)
project(SCAPMS VERSION 3.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Test Network PrintSupport)
add_library(clinic_core
    src/model/PatientRecord.cpp
    src/model/PatientTypes.cpp
    src/model/ClinicDatabase.cpp
    src/algorithms/Algorithms.cpp
    src/controller/ClinicManager.cpp
    src/controller/ReportService.cpp
    src/persistence/FileHandler.cpp
    src/persistence/RecoveryService.cpp
    src/persistence/WorkspaceGuard.cpp
    src/security/PortableBackup.cpp
    src/algorithms/Benchmark.cpp
    src/security/AccessControl.cpp)
target_include_directories(clinic_core PUBLIC src)
target_link_libraries(clinic_core PUBLIC Qt6::Core Qt6::Network)
if(WIN32)
    target_link_libraries(clinic_core PRIVATE crypt32 bcrypt)
endif()
file(GLOB GUI_SOURCES CONFIGURE_DEPENDS src/gui/*.cpp src/gui/panels/*.cpp)
add_library(clinic_gui ${GUI_SOURCES})
target_link_libraries(clinic_gui PUBLIC clinic_core Qt6::Widgets Qt6::PrintSupport)
add_executable(scapms WIN32 src/main.cpp)
target_link_libraries(scapms PRIVATE clinic_gui)
if(MSVC)
    target_compile_options(clinic_core PRIVATE /W4 /utf-8)
    target_compile_options(clinic_gui PRIVATE /W4 /utf-8)
else()
    target_compile_options(clinic_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(clinic_gui PRIVATE -Wall -Wextra -Wpedantic)
endif()
enable_testing()
add_executable(clinic_tests tests/ClinicTests.cpp)
target_link_libraries(clinic_tests PRIVATE clinic_gui Qt6::Test)
target_compile_definitions(clinic_tests PRIVATE PROJECT_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME clinic_tests COMMAND clinic_tests -platform offscreen -o ${CMAKE_CURRENT_BINARY_DIR}/test-results.txt,txt -o ${CMAKE_CURRENT_BINARY_DIR}/test-results.xml,junitxml)
set_tests_properties(clinic_tests PROPERTIES TIMEOUT 600 WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
install(TARGETS scapms RUNTIME DESTINATION .)


if(WIN32)
    set_tests_properties(clinic_tests PROPERTIES ENVIRONMENT "QT_QPA_FONTDIR=$ENV{WINDIR}/Fonts")
endif()
