# Copies fonts and images into the layout LocateResource searches on Linux,
# and copies rigs/ as VoLumRigs. Invoked at build time:
#   cmake -DRES=... -DDEST=... -DRIGS_SRC=... -DRIGS_DEST=... -P copy-resources.cmake
#
# DEST is the directory that contains the type folders (ttf, png, jpg, svg).
# For the VST3 that is VoLum.vst3/Contents/Resources (the .so is in
# Contents/x86_64-linux, and LocateResource tries ../Resources/<type>/<file>).
# For the app it is <bindir>/resources (LocateResource tries
# <exe_dir>/resources/<type>/<file>). Fonts use type "ttf". Images use the
# file extension (png, jpg, svg).
#
# RIGS_DEST is VoLumRigs next to the app, or Contents/Resources/VoLumRigs
# inside the bundle. Spec Paths searches those before the XDG and
# /usr/share locations.

if(NOT RES OR NOT DEST)
  message(FATAL_ERROR "copy-resources.cmake needs RES and DEST")
endif()

file(MAKE_DIRECTORY "${DEST}/ttf" "${DEST}/png" "${DEST}/jpg" "${DEST}/svg")

file(GLOB ttf_files "${RES}/fonts/*.ttf" "${RES}/fonts-win/*.ttf")
foreach(f ${ttf_files})
  file(COPY "${f}" DESTINATION "${DEST}/ttf")
endforeach()

file(GLOB img_files "${RES}/img/*")
foreach(f ${img_files})
  if(IS_DIRECTORY "${f}")
    continue()
  endif()
  get_filename_component(ext "${f}" EXT)
  string(TOLOWER "${ext}" ext)
  string(REGEX REPLACE "^\\." "" ext "${ext}")
  if(ext STREQUAL "jpeg")
    set(ext "jpg")
  endif()
  if(ext STREQUAL "png" OR ext STREQUAL "jpg" OR ext STREQUAL "svg")
    file(COPY "${f}" DESTINATION "${DEST}/${ext}")
  endif()
endforeach()

if(RIGS_SRC AND RIGS_DEST)
  file(COPY "${RIGS_SRC}/" DESTINATION "${RIGS_DEST}")
endif()
