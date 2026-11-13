# Fork identity for wa1hco/wsjtx-inhibit.
#
# set_build_type() turns release-state.txt into the upstream suffix
# ("-beta6", "-rc1", "-devel", or empty for GA). This file appends one
# more suffix so a user can tell the build from upstream.
#
# This is the only place to change the marker. A rebase onto the next
# upstream beta updates release-state.txt; the program version, installer
# name, and release tag pick up the new beta number without another edit.
#
# version() in revision_utils.cpp reads BUILD_TYPE_REVISION, and that
# string is what the title bar, Help > About, wsjtx_app_version, the
# UDP Heartbeat version field, and the PSK Reporter software field show.
# The Heartbeat Id stays the application name ("WSJT-X"). Do not put the
# marker there: KEY agents match that Id.
set (WSJT_INHIBIT_VERSION_SUFFIX "-inhibit")

function (wsjt_apply_inhibit_version_suffix)
  if (NOT DEFINED BUILD_TYPE_REVISION OR NOT DEFINED BUILD_TYPE_PACKAGE_REVISION)
    message (FATAL_ERROR "wsjt_apply_inhibit_version_suffix requires set_build_type() first")
  endif ()
  if (NOT WSJT_INHIBIT_VERSION_SUFFIX MATCHES "^-[A-Za-z0-9._+-]+$")
    message (FATAL_ERROR "WSJT_INHIBIT_VERSION_SUFFIX must look like -inhibit")
  endif ()
  set (BUILD_TYPE_REVISION "${BUILD_TYPE_REVISION}${WSJT_INHIBIT_VERSION_SUFFIX}" PARENT_SCOPE)
  set (BUILD_TYPE_PACKAGE_REVISION "${BUILD_TYPE_PACKAGE_REVISION}${WSJT_INHIBIT_VERSION_SUFFIX}" PARENT_SCOPE)
endfunction ()
