
if (ENABLE_SASL)
	tarantool_plug("auth_sasl" auth_sasl.m)
endif()
