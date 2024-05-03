set(DOC_FILES 
	${CMAKE_SOURCE_DIR}/ChangeLog.md
	${CMAKE_SOURCE_DIR}/COPYING
)
set(SHARE_DIR ${CMAKE_INSTALL_PREFIX}/share/${PACKAGE})
set(CONF_FILE ${CMAKE_SOURCE_DIR}/nzbget.conf)
set(WEBUI_DIR ${CMAKE_SOURCE_DIR}/webui)
set(DOC_FILES_DEST ${SHARE_DIR}/doc)
set(CONF_FILE_DEST ${SHARE_DIR})
set(WEBUI_DIR_DEST ${SHARE_DIR})
set(BIN_FILE_DEST ${CMAKE_INSTALL_PREFIX}/bin)

install(TARGETS ${PACKAGE} PERMISSIONS 
	OWNER_EXECUTE 
	OWNER_WRITE 
	OWNER_READ 
	GROUP_READ 
	GROUP_EXECUTE
	WORLD_READ 
	WORLD_EXECUTE
	DESTINATION ${BIN_FILE_DEST})
install(FILES ${DOC_FILES} DESTINATION ${DOC_FILES_DEST})
install(DIRECTORY ${WEBUI_DIR} DESTINATION ${WEBUI_DIR_DEST})

file(READ ${CONF_FILE} CONFIG_CONTENT)
string(REPLACE "WebDir=" "WebDir=${WEBUI_DIR_DEST}/webui" MODIFIED_CONFIG_CONTENT "${CONFIG_CONTENT}")
string(REPLACE "ConfigTemplate=" "ConfigTemplate=${CONF_FILE_DEST}/nzbget.conf" MODIFIED_CONFIG_CONTENT "${MODIFIED_CONFIG_CONTENT}")
file(WRITE ${CMAKE_BINARY_DIR}/nzbget.conf "${MODIFIED_CONFIG_CONTENT}")

if(EXISTS ${CMAKE_INSTALL_PREFIX}/etc/nzbget.conf)
	message(STATUS "nzbget.conf is already installed in ${CMAKE_INSTALL_PREFIX}/etc")
	message(STATUS "If you want to overwrite it, then do it manually with caution")
endif()

add_custom_target(uninstall
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${DOC_FILES_DEST}
	COMMAND ${CMAKE_COMMAND} -E remove_directory ${SHARE_DIR}
	COMMAND ${CMAKE_COMMAND} -E remove ${BIN_FILE_DEST}/${PACKAGE}
	COMMENT "Uninstalling" ${PACKAGE}
)

add_custom_target(install-conf
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_INSTALL_PREFIX}/etc
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/nzbget.conf ${CMAKE_INSTALL_PREFIX}/etc/nzbget.conf
    COMMENT "Installing nzbget.conf"
)

add_custom_target(uninstall-conf
    COMMAND ${CMAKE_COMMAND} -E remove_directory ${CMAKE_INSTALL_PREFIX}/etc
    COMMAND ${CMAKE_COMMAND} -E remove ${CMAKE_BINARY_DIR}/nzbget.conf
    COMMENT "Uninstalling nzbget.conf"
)
