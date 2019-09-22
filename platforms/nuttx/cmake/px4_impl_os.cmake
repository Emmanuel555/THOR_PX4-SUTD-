############################################################################
#
# Copyright (c) 2015 PX4 Development Team. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in
#    the documentation and/or other materials provided with the
#    distribution.
# 3. Neither the name PX4 nor the names of its contributors may be
#    used to endorse or promote products derived from this software
#    without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
# OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
# AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.
#
############################################################################

#=============================================================================
#
#	Defined functions in this file
#
#	Required OS Interface Functions
#
#		* px4_os_add_flags
#		* px4_os_prebuild_targets
#

include(px4_base)

#=============================================================================
#
#	px4_os_add_flags
#
#	Set the nuttx build flags.
#
function(px4_os_add_flags)

	include_directories(BEFORE SYSTEM
		${PX4_BINARY_DIR}/NuttX/nuttx/include
		${PX4_BINARY_DIR}/NuttX/nuttx/include/cxx
	)

	include_directories(
		${PX4_BINARY_DIR}/NuttX/nuttx/arch/${CONFIG_ARCH}/src/${CONFIG_ARCH_FAMILY}
		${PX4_BINARY_DIR}/NuttX/nuttx/arch/${CONFIG_ARCH}/src/chip
		${PX4_BINARY_DIR}/NuttX/nuttx/arch/${CONFIG_ARCH}/src/common

		${PX4_BINARY_DIR}/NuttX/apps/include
		)

	add_definitions(
		-D__PX4_NUTTX
		-D__DF_NUTTX
		)

<<<<<<< HEAD
	if("${CONFIG_ARMV7M_STACKCHECK}" STREQUAL "y")
		message(STATUS "NuttX Stack Checking (CONFIG_ARMV7M_STACKCHECK) enabled")
		add_compile_options(
=======

	if("${config_nuttx_hw_stack_check_${BOARD}}" STREQUAL "y")
		set(instrument_flags
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
			-finstrument-functions
			-ffixed-r10
			)
	endif()

endfunction()

#=============================================================================
#
#	px4_os_prebuild_targets
#
#	This function generates os dependent targets
#
#	Usage:
#		px4_os_prebuild_targets(
#			OUT <out-list_of_targets>
#			BOARD <in-string>
#			)
#
#	Input:
#		BOARD		: board
#		THREADS		: number of threads for building
#
#	Output:
#		OUT	: the target list
#
#	Example:
#		px4_os_prebuild_targets(OUT target_list BOARD px4_fmu-v2)
#
function(px4_os_prebuild_targets)
	px4_parse_function_args(
			NAME px4_os_prebuild_targets
<<<<<<< HEAD
			ONE_VALUE OUT BOARD
			REQUIRED OUT
=======
			ONE_VALUE OUT BOARD THREADS
			REQUIRED OUT BOARD
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
			ARGN ${ARGN})

	if(PX4_BOARD_LABEL MATCHES "stackcheck")
		set(NUTTX_CONFIG "stackcheck" CACHE INTERNAL "NuttX config" FORCE)
	else()
		set(NUTTX_CONFIG "nsh" CACHE INTERNAL "NuttX config" FORCE)
	endif()

	add_library(prebuild_targets INTERFACE)
	target_link_libraries(prebuild_targets INTERFACE nuttx_cxx nuttx_c nuttx_fs nuttx_mm nuttx_sched m gcc)
	add_dependencies(prebuild_targets DEPENDS nuttx_context uorb_headers)

	# parse nuttx config options for cmake
<<<<<<< HEAD
	file(STRINGS ${PX4_BOARD_DIR}/nuttx-config/${NUTTX_CONFIG}/defconfig ConfigContents)
=======
	file(STRINGS ${PX4_SOURCE_DIR}/platforms/nuttx/nuttx-configs/${BOARD}/nsh/defconfig ConfigContents)
>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
	foreach(NameAndValue ${ConfigContents})
		# Strip leading spaces
		string(REGEX REPLACE "^[ ]+" "" NameAndValue ${NameAndValue})

		# Find variable name
		string(REGEX MATCH "^CONFIG[^=]+" Name ${NameAndValue})

		if (Name)
			# Find the value
			string(REPLACE "${Name}=" "" Value ${NameAndValue})

			# remove extra quotes
			string(REPLACE "\"" "" Value ${Value})

			# Set the variable
			#message(STATUS "${Name} ${Value}")
			set(${Name} ${Value} CACHE INTERNAL "NUTTX DEFCONFIG: ${Name}" FORCE)
		endif()
	endforeach()
endfunction()
<<<<<<< HEAD
=======

#=============================================================================
#
#	px4_nuttx_configure
#
#	This function sets the nuttx configuration
#
#	Usage:
#		px4_nuttx_configure(
#	    HWCLASS <m3|m4>
#		  [ROMFS <y|n>
#		  ROMFSROOT <root>]
#			)
#
#	Input:
#	  HWCLASS		: the class of hardware
#	  CONFIG		: the nuttx configuration to use
#	  ROMFS			: whether or not to use incllude theROMFS
#	  ROMFSROOT		: If ROMFS used set the root the default is px4fmu_common
#
#	Output:
#		OUT	: None
#
#	Example:
#		px4_nuttx_configure(HWCLASS m4 CONFIG nsh ROMFS y)
#
function(px4_nuttx_configure)
	px4_parse_function_args(
			NAME px4_nuttx_configure
			ONE_VALUE HWCLASS CONFIG ROMFS ROMFSROOT IO
			REQUIRED HWCLASS
			ARGN ${ARGN})

	# HWCLASS -> CMAKE_SYSTEM_PROCESSOR
	if(HWCLASS STREQUAL "m7")
		set(CMAKE_SYSTEM_PROCESSOR "cortex-m7" PARENT_SCOPE)
	elseif(HWCLASS STREQUAL "m4")
		set(CMAKE_SYSTEM_PROCESSOR "cortex-m4" PARENT_SCOPE)
	elseif(HWCLASS STREQUAL "m3")
		set(CMAKE_SYSTEM_PROCESSOR "cortex-m3" PARENT_SCOPE)
	endif()
	set(CMAKE_SYSTEM_PROCESSOR ${CMAKE_SYSTEM_PROCESSOR} CACHE INTERNAL "system processor" FORCE)
	set(CMAKE_TOOLCHAIN_FILE ${PX4_SOURCE_DIR}/cmake/toolchains/Toolchain-arm-none-eabi.cmake CACHE INTERNAL "toolchain file" FORCE)

	# ROMFS
	if("${ROMFS}" STREQUAL "y")
		if (NOT DEFINED ROMFSROOT)
			set(config_romfs_root px4fmu_common)
		else()
			set(config_romfs_root ${ROMFSROOT})
		endif()
		set(config_romfs_root ${config_romfs_root} PARENT_SCOPE)
	endif()

	# IO board placed in ROMFS
	if(config_romfs_root)
		set(config_io_board ${IO} PARENT_SCOPE)
	endif()
endfunction()

>>>>>>> 97f14edcbd3ff8526326d26d749656a8e8f309c9
