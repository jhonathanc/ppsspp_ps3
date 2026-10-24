
set(LIBRETRO_SRCS
	libretro.cpp
	LibretroGraphicsContext.cpp)

if(NOT NO_GL_BUILD)
	list(APPEND LIBRETRO_SRCS
		LibretroGLContext.cpp
		LibretroGLCoreContext.cpp
		LibretroVulkanContext.cpp
		LibretroVulkanPresentation.cpp)
endif()

if(WIN32)
	set(LIBRETRO_SRCS ${LIBRETRO_SRCS}
		LibretroD3D11Context.cpp)
endif()

set(LIBRETRO_VFS_SOURCE libretro-common/vfs/vfs_implementation.c)
if(PS3_PSL1GHT)
	find_package(Python3 REQUIRED COMPONENTS Interpreter)
	set(LIBRETRO_VFS_SOURCE "${CMAKE_CURRENT_BINARY_DIR}/vfs_implementation.c")
	set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
		"${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/vfs/vfs_implementation.c"
		"${CMAKE_SOURCE_DIR}/PS3/prepare-libretro-vfs.py")
	execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/PS3/prepare-libretro-vfs.py"
		"${CMAKE_CURRENT_SOURCE_DIR}/libretro-common/vfs/vfs_implementation.c" "${LIBRETRO_VFS_SOURCE}"
		RESULT_VARIABLE VFS_PREPARE_RESULT)
	if(NOT VFS_PREPARE_RESULT EQUAL 0)
		message(FATAL_ERROR "Could not prepare the PSL1GHT libretro VFS overlay")
	endif()
endif()

set(LIBRETRO_SRCS ${LIBRETRO_SRCS}
	libretro-common/compat/compat_posix_string.c
	libretro-common/compat/fopen_utf8.c
	libretro-common/compat/compat_strl.c
	libretro-common/encodings/encoding_utf.c
	libretro-common/file/file_path.c
	libretro-common/file/file_path_io.c
	libretro-common/streams/file_stream.c
	libretro-common/streams/file_stream_transforms.c
	libretro-common/string/stdstring.c
	libretro-common/time/rtime.c
	${LIBRETRO_VFS_SOURCE})

include_directories(libretro)

if(PS3)
	add_library(ppsspp_libretro STATIC ${LIBRETRO_SRCS})
else()
	add_library(ppsspp_libretro SHARED ${LIBRETRO_SRCS})
endif()
set(LIBRARY_OUTPUT_PATH "${CMAKE_BINARY_DIR}")
set_target_properties(ppsspp_libretro PROPERTIES PREFIX "")
if(ANDROID)
	set_target_properties(ppsspp_libretro PROPERTIES SUFFIX "_android.so")
endif()
target_include_directories(ppsspp_libretro PRIVATE libretro-common/include)

if(NOT MSVC AND NOT PS3)
   if (APPLE OR IOS)
   	target_link_libraries(ppsspp_libretro objc "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/libretro.osx.def")
   else()
   	target_link_libraries(ppsspp_libretro "-Wl,-Bsymbolic")
   endif()
endif()
target_link_libraries(ppsspp_libretro Core ${CMAKE_THREAD_LIBS_INIT})
if(ANDROID)
	target_link_libraries(ppsspp_libretro log)
endif()

if(PS3_PSL1GHT)
	add_custom_target(ps3_libretro_archive
		COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/PS3/libretro/package.py"
			--ar "${CMAKE_AR}" --directory "${CMAKE_BINARY_DIR}/lib"
			--output "${CMAKE_BINARY_DIR}/ppsspp_libretro_psl1ght.a"
		DEPENDS ppsspp_libretro
		VERBATIM)
endif()
