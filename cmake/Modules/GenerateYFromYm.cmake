# This function is syslog-ng specific

function(generate_y_from_ym FileWoExt)
    if (${ARGC} EQUAL 1)
        add_custom_command (OUTPUT ${PROJECT_BINARY_DIR}/${FileWoExt}.y
            COMMAND ${PROJECT_SOURCE_DIR}/lib/merge-grammar.pl ${PROJECT_SOURCE_DIR}/${FileWoExt}.ym > ${PROJECT_BINARY_DIR}/${FileWoExt}.y
            DEPENDS ${PROJECT_SOURCE_DIR}/lib/cfg-grammar.y
                    ${PROJECT_SOURCE_DIR}/${FileWoExt}.ym)
    elseif(${ARGC} EQUAL 2)
        add_custom_command (OUTPUT ${PROJECT_BINARY_DIR}/${ARGV1}.y
            COMMAND ${PROJECT_SOURCE_DIR}/lib/merge-grammar.pl ${PROJECT_SOURCE_DIR}/${FileWoExt}.ym > ${PROJECT_BINARY_DIR}/${ARGV1}.y
            DEPENDS ${PROJECT_SOURCE_DIR}/lib/cfg-grammar.y
                    ${PROJECT_SOURCE_DIR}/${FileWoExt}.ym)
    else()
        message(SEND_ERROR "Wrong usage of generate_y_from_ym() function")
    endif()
endfunction()

