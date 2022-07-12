cmake_minimum_required(VERSION 3.20.0)

# Setup project name and languages
project(firmware LANGUAGES C ASM)

add_subdirectory(sdk)

if(DEFINED LED_STRIP_COUNT)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC LED_STRIP_COUNT=${LED_STRIP_COUNT})
endif()

if(DEFINED LED_STRIP_TYPE)
    target_compile_definitions(${CMAKE_PROJECT_NAME} PUBLIC LED_STRIP_TYPE=${LED_STRIP_TYPE})
endif()

# If you need to add some source files to the project add them to the "src" folder and update CMakeLists there
add_subdirectory(src)
