include(FindPackageHandleStandardArgs)

find_program(GRADLE_EXECUTABLE NAMES gradle PATHS $ENV{GRADLE_HOME}/bin NO_CMAKE_FIND_ROOT_PATH)

if (GRADLE_EXECUTABLE)
    execute_process(COMMAND gradle --version
                    COMMAND grep Gradle
                    COMMAND head -1
                    COMMAND sed "s/.*\\ \\(.*\\)/\\1/"
                    OUTPUT_VARIABLE GRADLE_VERSION
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    )
endif()

find_package_handle_standard_args(Gradle FOUND_VAR GRADLE_FOUND  REQUIRED_VARS GRADLE_EXECUTABLE VERSION_VAR GRADLE_VERSION)
mark_as_advanced(GRADLE_EXECUTABLE)
