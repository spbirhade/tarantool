
function(tarantool_plug plug)
  set (plug_sources ${ARGV})
  list(REMOVE_ITEM plug_sources ${plug})

  add_library(tnt_${plug} SHARED ${plug_sources} 
      ${CMAKE_SOURCE_DIR}/third_party/rijndael.c
      ${CMAKE_SOURCE_DIR}/third_party/cmac.c)

  set_target_properties(tnt_${plug} PROPERTIES COMPILE_FLAGS
    "${core_cflags} " PREFIX "")

  install (TARGETS tnt_${plug} DESTINATION lib/tarantool)

endfunction()

foreach (plug ${TARANTOOL_PLUGS})
  add_subdirectory(${plug})
endforeach()
