# Windows-only TX Inhibit spacebar tester (native .exe, no extra runtime).
# Built as part of the main project when WIN32; installed next to wsjtx.exe.

if (NOT WIN32)
  return ()
endif ()

add_executable (inhibit_spacebar WIN32
  inhibit_spacebar.c
  )

target_link_libraries (inhibit_spacebar PRIVATE
  ws2_32
  comctl32
  user32
  gdi32
  )

# Helpful for MinGW: ensure UNICODE-off ANSI entry matches WinMain
if (MINGW)
  target_compile_options (inhibit_spacebar PRIVATE -Wall -O2)
  # Prefer subsystem windows
  set_target_properties (inhibit_spacebar PROPERTIES
    WIN32_EXECUTABLE TRUE
    OUTPUT_NAME "inhibit_spacebar"
    )
endif ()

install (TARGETS inhibit_spacebar
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT runtime
  )
