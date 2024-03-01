set(ROOT ${CMAKE_BINARY_DIR}/boost/)

ExternalProject_add(
	boost
	PREFIX boost
	URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
	TLS_VERIFY TRUE
	BUILD_IN_SOURCE TRUE
	GIT_SHALLOW TRUE
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	CONFIGURE_COMMAND ${ROOT}src/boost/bootstrap.sh 
		--with-libraries=json 
		--prefix=${ROOT}build
	BUILD_COMMAND ${ROOT}src/boost/b2 link=static
	INSTALL_COMMAND ${ROOT}src/boost/b2 install
)

set(LIBS ${LIBS} ${ROOT}build/lib/libboost_json.a)
set(INCLUDES ${INCLUDES} ${ROOT}build/include)
