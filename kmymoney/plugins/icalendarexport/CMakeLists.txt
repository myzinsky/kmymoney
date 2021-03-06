# patch the version with the version defined in the build system
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/kmm_icalendarexport.json.in
  ${CMAKE_CURRENT_BINARY_DIR}/kmm_icalendarexport.json
  @ONLY
)

set(kmm_icalendarexport_PART_SRCS
  schedulestoicalendar.cpp
  icalendarexport.cpp
)

kconfig_add_kcfg_files(kmm_icalendarexport_PART_SRCS pluginsettings.kcfgc)
add_library(kmm_icalendarexport MODULE ${kmm_icalendarexport_PART_SRCS})

target_include_directories(kmm_icalendarexport PRIVATE ${LIBICAL_INCLUDE_DIRS})
target_link_libraries(kmm_icalendarexport
  KF5::KIOWidgets
  kmm_mymoney
  kmm_plugin
  ${LIBICAL_LIBRARIES}
)

install(FILES kmm_icalendarexport.rc
        DESTINATION "${KXMLGUI_INSTALL_DIR}/kmm_icalendarexport")

install(TARGETS kmm_icalendarexport
        DESTINATION "${KDE_INSTALL_PLUGINDIR}/kmymoney/")

# the KCM module

set(kcm_kmm_icalendarexport_PART_SRCS
    kcm_icalendarexport.cpp
    )

kconfig_add_kcfg_files(kcm_kmm_icalendarexport_PART_SRCS pluginsettings.kcfgc)
ki18n_wrap_ui(kcm_kmm_icalendarexport_PART_SRCS pluginsettingsdecl.ui)

add_library(kcm_kmm_icalendarexport MODULE ${kcm_kmm_icalendarexport_PART_SRCS})
kcoreaddons_desktop_to_json(kcm_kmm_icalendarexport kcm_kmm_icalendarexport.desktop)

target_link_libraries(kcm_kmm_icalendarexport
  KF5::I18n
  KF5::ConfigWidgets
  KF5::KIOWidgets
  KF5::Completion
  Qt5::Widgets
  Qt5::Gui
)

install(TARGETS kcm_kmm_icalendarexport
        DESTINATION ${KDE_INSTALL_PLUGINDIR})

install(FILES kcm_kmm_icalendarexport.desktop
        DESTINATION ${SERVICES_INSTALL_DIR})
