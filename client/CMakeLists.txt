function(tarantool_client client_name)

  set (client_sources ${ARGV})
  list(REMOVE_ITEM client_sources ${client_name})

  add_executable(${client_name} ${client_sources}
      ${CMAKE_SOURCE_DIR}/third_party/rijndael.c
      ${CMAKE_SOURCE_DIR}/third_party/cmac.c)

  #set_target_properties(lt${client_name} PROPERTIES COMPILE_FLAGS
	#"${core_cflags}")

  set_target_properties(${client_name} PROPERTIES COMPILE_FLAGS
	"${core_cflags}")

#target_link_libraries(${client_name} lt${client_name})
#
  #if (ENABLE_STATIC)
      #set_target_properties(${client_name} PROPERTIES
          #LINK_SEARCH_END_STATIC ON)
  #endif()

  target_link_libraries (${client_name} connector_c)

  install (TARGETS ${client_name} DESTINATION bin)
endfunction()

tarantool_client("tnt" tnt.c)
