
########### next target ###############

set(kmm_kbanking_dialogs_la_SOURCES
  kbaccountsettings.cpp
  kbpickstartdate.cpp
  kbmapaccount.cpp
)

set(kmm_kbanking_dialogs_UI
  kbaccountsettings.ui
  kbpickstartdate.ui
  kbmapaccount.ui
)

ki18n_wrap_ui(kmm_kbanking_dialogs_la_SOURCES ${kmm_kbanking_dialogs_UI})

add_library(kmm_kbanking_dialogs STATIC ${kmm_kbanking_dialogs_la_SOURCES})
target_link_libraries(kmm_kbanking_dialogs
  widgets
  dialogs
  aqbanking::aqbanking
)

