cmake_minimum_required(VERSION 2.8.12)

PROJECT(gustavd C)
ADD_DEFINITIONS(-Os -Wall -Werror --std=gnu99 -g3 -Wmissing-declarations)

SET(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "")

ADD_EXECUTABLE(gustavd main.c term.c fdio.c at.c)

INSTALL(TARGETS gustavd
	RUNTIME DESTINATION sbin
)
