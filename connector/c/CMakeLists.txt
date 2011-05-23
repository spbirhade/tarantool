
set (connector_c_sources
	tnt.c
	tnt_mem.c
	tnt_io.c
	tnt_raw.c
	tnt_md5.c
	tnt_aes.c
	tnt_cmac.c
	tnt_auth.c
	tnt_auth_chap.c
    tnt_leb128.c
	tnt_tuple.c
	tnt_insert.c
	tnt_update.c
	tnt_delete.c
	tnt_select.c
	tnt_ping.c
	tnt_recv.c
	tnt_map.c)

if (ENABLE_SASL)
	set (connector_c_sources ${connector_c_sources} 
		tnt_auth_sasl.c)
endif()

add_library(connector_c ${connector_c_sources})
