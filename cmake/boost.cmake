set(BOOST_ROOT ${CMAKE_BINARY_DIR}/boost/src)
set(BOOST_LIBS ${BOOST_ROOT}/boost/stage/lib/libboost_json.a)

ExternalProject_add(
	boost
	PREFIX boost
	URL https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz
	TLS_VERIFY 		TRUE
	BUILD_IN_SOURCE TRUE
	GIT_SHALLOW 	TRUE
	GIT_PROGRESS	TRUE
	DOWNLOAD_EXTRACT_TIMESTAMP TRUE
	BUILD_BYPRODUCTS  ${BOOST_LIBS}
	CONFIGURE_COMMAND ${BOOST_ROOT}/boost/bootstrap.sh 
					--with-libraries=json 
					--prefix=${BOOST_ROOT}/build
	BUILD_COMMAND ${BOOST_ROOT}/boost/b2 link=static
	INSTALL_COMMAND ""
)

set(LIBS ${LIBS} ${BOOST_LIBS})
set(INCLUDES ${INCLUDES} ${BOOST_ROOT}/boost)
