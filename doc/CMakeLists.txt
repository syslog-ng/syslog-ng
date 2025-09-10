option(ENABLE_MANPAGES "Enable building manpages" OFF)
set(ENABLE_MANPAGES_INSTALL "AUTO" CACHE STRING "Enable installation of manpages: ON, OFF, AUTO")
set(WITH_MANPAGES "local" CACHE STRING "Manpages mode: local or remote")
set_property(CACHE WITH_MANPAGES PROPERTY STRINGS local remote)

if(NOT ENABLE_MANPAGES AND "${ENABLE_MANPAGES_INSTALL}" STREQUAL "ON")
    message(FATAL_ERROR
        "Manpage generation is disabled whilst installation is requested. "
        "Please enable manpage generation first (ENABLE_MANPAGES=ON)."
    )
endif()

if("${ENABLE_MANPAGES_INSTALL}" STREQUAL "AUTO" OR "${ENABLE_MANPAGES_INSTALL}" STREQUAL "auto")
    if(ENABLE_MANPAGES)
        set(SYSLOG_NG_ENABLE_MANPAGES_INSTALL "ON" CACHE STRING "Enable installation of manpages" FORCE)
    else()
        set(SYSLOG_NG_ENABLE_MANPAGES_INSTALL "OFF" CACHE STRING "Enable installation of manpages" FORCE)
    endif()
else()
    set(SYSLOG_NG_ENABLE_MANPAGES_INSTALL "${ENABLE_MANPAGES_INSTALL}" CACHE STRING "Enable installation of manpages" FORCE)
endif()

set(XSD_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/xsd/patterndb-1.xsd"
    "${CMAKE_CURRENT_SOURCE_DIR}/xsd/patterndb-2.xsd"
    "${CMAKE_CURRENT_SOURCE_DIR}/xsd/patterndb-3.xsd"
    "${CMAKE_CURRENT_SOURCE_DIR}/xsd/patterndb-4.xsd"
    "${CMAKE_CURRENT_SOURCE_DIR}/xsd/patterndb-5.xsd"
    "${CMAKE_CURRENT_SOURCE_DIR}/xsd/patterndb-6.xsd"
)
install(FILES ${XSD_FILES} DESTINATION share/syslog-ng/xsd)

if(ENABLE_MANPAGES)
    set(MAN_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/man")
    set(MAN_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/man")

    set(MANPAGES
        dqtool.1
        loggen.1
        pdbtool.1
        persist-tool.1
        secure-logging.7
        slogencrypt.1
        slogkey.1
        slogverify.1
        syslog-ng.8
        syslog-ng.conf.5
        syslog-ng-ctl.1
        syslog-ng-debun.1
    )

    if(CMAKE_VERBOSE_MAKEFILE)
        set(SH_OUTPUT1 "")
        set(SH_OUTPUT2 "")
    else()
        set(SH_OUTPUT1 ">/dev/null")
        set(SH_OUTPUT2 "2>&1")
    endif()

    # 1) Prepare step: create dir + fetch/copy
    if(WITH_MANPAGES STREQUAL "remote")
        add_custom_command(
            OUTPUT "${MAN_BUILD_DIR}/.prep"
            DEPENDS "${MAN_SRC_DIR}/update-manpages.sh"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${MAN_BUILD_DIR}"
            COMMENT "Preparing manpages - MODE=remote..."
            COMMAND "${MAN_SRC_DIR}/update-manpages.sh" -f "${MAN_BUILD_DIR}" "${SH_OUTPUT1}" "${SH_OUTPUT2}"
            COMMAND ${CMAKE_COMMAND} -E touch "${MAN_BUILD_DIR}/.prep"
        )
    else()
        add_custom_command(
            OUTPUT "${MAN_BUILD_DIR}/.prep"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${MAN_BUILD_DIR}"
            COMMENT "Preparing manpages - MODE=local..."
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${MAN_SRC_DIR}/local" "${MAN_BUILD_DIR}"
            COMMAND ${CMAKE_COMMAND} -E touch "${MAN_BUILD_DIR}/.prep"
        )
    endif()

    add_custom_target(prep_manpages ALL DEPENDS "${MAN_BUILD_DIR}/.prep")

    # 2) Update step: run path substitutions
    include(GNUInstallDirs)
    add_custom_command(
        OUTPUT "${MAN_BUILD_DIR}/.updated"
        DEPENDS prep_manpages "${MAN_SRC_DIR}/update-manpages.sh"
        COMMENT "Updating manpages..."
        COMMAND "${MAN_SRC_DIR}/update-manpages.sh" -u "${CMAKE_INSTALL_FULL_SYSCONFDIR}" "${CMAKE_INSTALL_PREFIX}" "${MAN_BUILD_DIR}" "${SH_OUTPUT1}" "${SH_OUTPUT2}"
        COMMAND ${CMAKE_COMMAND} -E touch "${MAN_BUILD_DIR}/.updated"
    )

    add_custom_target(update_manpages ALL DEPENDS "${MAN_BUILD_DIR}/.updated")

    # 3) Concrete manpages depend on update
    foreach(m ${MANPAGES})
        add_custom_command(
            OUTPUT "${MAN_BUILD_DIR}/${m}"
            DEPENDS update_manpages
        )
    endforeach()

    # 4) Install manpages (only if needed)
    if("${SYSLOG_NG_ENABLE_MANPAGES_INSTALL}" STREQUAL "ON")
        foreach(m ${MANPAGES})
            # get the section number from the filename: foo.1 -> 1
            get_filename_component(_name ${m} NAME) # foo.1
            string(REGEX MATCH "\\.([1-9])$" _section ${_name})

            if(NOT _section)
                message(FATAL_ERROR "Cannot determine man section for ${m}")
            endif()

            set(_dest "share/man/man${CMAKE_MATCH_1}")
            install(FILES "${MAN_BUILD_DIR}/${_name}" DESTINATION "${_dest}")
        endforeach()
    endif()
endif()
