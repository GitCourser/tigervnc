if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(FATAL_ERROR "cmake must be invoked with the top level directory")
endif()

#
# Windows installer (Inno Setup)
#

if(WIN32)

if(CMAKE_SIZEOF_VOID_P MATCHES 8)
  set(INST_SUFFIX 64)
  set(INST_DEFS -DWIN64)
endif()

configure_file(tigervnc.iss.in tigervnc.iss)

add_custom_command(OUTPUT ${CMAKE_PROJECT_NAME}${INST_SUFFIX}-${VERSION}.exe
  COMMAND iscc -o. ${INST_DEFS} -F${CMAKE_PROJECT_NAME}${INST_SUFFIX}-${VERSION} tigervnc.iss
  DEPENDS vncviewer tigervnc.iss)
add_custom_target(installer DEPENDS ${CMAKE_PROJECT_NAME}${INST_SUFFIX}-${VERSION}.exe)

if(BUILD_WINVNC)
  configure_file(winvnc.iss.in winvnc.iss)

  add_custom_command(OUTPUT ${CMAKE_PROJECT_NAME}${INST_SUFFIX}-winvnc-${VERSION}.exe
    COMMAND iscc -o. ${INST_DEFS} -F${CMAKE_PROJECT_NAME}${INST_SUFFIX}-winvnc-${VERSION} winvnc.iss
    DEPENDS winvnc4 wm_hooks vncconfig winvnc.iss)
  add_custom_target(winvnc_installer DEPENDS ${CMAKE_PROJECT_NAME}${INST_SUFFIX}-winvnc-${VERSION}.exe)
endif()

# Portable ZIP package (no installer)
if(BUILD_PORTABLE_VIEWER)
  set(PORTABLE_DIR "${CMAKE_BINARY_DIR}/portable/TigerVNC-Viewer-Portable-x64-${VERSION}")
  set(PORTABLE_ZIP "${CMAKE_BINARY_DIR}/TigerVNC-Viewer-Portable-x64-${VERSION}.zip")

  add_custom_command(OUTPUT ${PORTABLE_ZIP}
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_BINARY_DIR}/portable"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PORTABLE_DIR}"
    COMMAND ${CMAKE_COMMAND} -E copy
            "$<TARGET_FILE:vncviewer>"
            "${PORTABLE_DIR}/vncviewer.exe"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_SOURCE_DIR}/LICENCE.TXT"
            "${PORTABLE_DIR}/LICENCE.TXT"
    COMMAND ${CMAKE_COMMAND} -E copy
            "${CMAKE_CURRENT_SOURCE_DIR}/portable-vncviewer.ini"
            "${PORTABLE_DIR}/vncviewer.ini"
    COMMAND ${CMAKE_COMMAND} -E chdir "${CMAKE_BINARY_DIR}/portable"
            ${CMAKE_COMMAND} -E tar cf "${PORTABLE_ZIP}" --format=zip
            "TigerVNC-Viewer-Portable-x64-${VERSION}"
    DEPENDS vncviewer "${CMAKE_CURRENT_SOURCE_DIR}/portable-vncviewer.ini"
    COMMENT "Creating portable viewer package"
    VERBATIM)
  add_custom_target(portable DEPENDS ${PORTABLE_ZIP})
endif()

endif() # WIN32


#
# Mac DMG
#

if(APPLE)

configure_file(makemacapp.in makemacapp)
configure_file(Info.plist.in Info.plist)

add_custom_command(OUTPUT TigerVNC-${VERSION}.dmg
  COMMAND sh makemacapp
  DEPENDS vncviewer makemacapp Info.plist)
add_custom_target(dmg DEPENDS TigerVNC-${VERSION}.dmg)

endif() # APPLE


#
# Binary tarball
#

if(UNIX)

configure_file(maketarball.in maketarball)

if(BUILD_JAVA)
  set(TARBALL_JAVA_DEPENDENCY java)
endif()

set(PACKAGE_FILE ${CMAKE_PROJECT_NAME}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}-${VERSION}.tar.gz)
add_custom_command(OUTPUT ${PACKAGE_FILE}
  COMMAND bash maketarball
  DEPENDS maketarball vncviewer vncpasswd vncconfig ${TARBALL_JAVA_DEPENDENCY})

add_custom_target(tarball DEPENDS ${PACKAGE_FILE})

endif() #UNIX

#
# Common
#

install(FILES ${CMAKE_SOURCE_DIR}/LICENCE.TXT DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR})
install(FILES ${CMAKE_SOURCE_DIR}/README.rst DESTINATION ${CMAKE_INSTALL_FULL_DOCDIR})
