tarantool_plug("auth_chap" auth_chap.m 
	${CMAKE_SOURCE_DIR}/third_party/rijndael.c
	${CMAKE_SOURCE_DIR}/third_party/cmac.c)
