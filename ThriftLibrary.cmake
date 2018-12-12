#
# Requirements:
# Please provide the following two variables before using these macros:
#   ${THRIFT1} - path/to/bin/thrift1
#   ${THRIFT_TEMPLATES} - path/to/include/thrift/templates
#   ${THRIFTCPP2} - path/to/lib/thriftcpp2
#

include(CMakeParseArguments)

#
# bypass_source_check
# This tells cmake to ignore if it doesn't see the following sources in
# the library that will be installed. Thrift files are generated at compile
# time so they do not exist at source check time
#
# Params:
#   @sources - The list of files to ignore in source check
#

macro(bypass_source_check sources)
set_source_files_properties(
  ${sources}
  PROPERTIES GENERATED TRUE
)
endmacro()

# add_thrift_library
#
#   SYNTAX:
#     add_thrift_library(
#       target-name [STATIC|SHARED|OBJECT]
#       /path/to/input.thrift
#       LANGUAGE language
#       [OUTPUT_PATH] output_directory
#       [SERVICES [svc1] [svc2] [svc3]...]
#       [WORKING_DIRECTORY working_directory]
#       [INSTALL]
#       [OPTIONS additional_option1 [additional_option2] ...]
#       [THRIFT_INCLUDE include_dir1 [include_dir2] ...]
#       [GENERATED_INCLUDE_PREFIX include_prefix]
#     )
#
# Creates a CMake library named target-name by generating source code based
# on an input thrift file.

function(add_thrift_library)
    cmake_parse_arguments(
        ARG
        "INSTALL;STATIC;SHARED;OBJECT"
        "LANGUAGE;OUTPUT_PATH;GENERATED_INCLUDE_PREFIX"
        "SERVICES;OPTIONS;THRIFT_INCLUDE"
        ${ARGN}
    )

    # Positional args should be the target-name and the input-thrift-file
    list(LENGTH ARG_UNPARSED_ARGUMENTS num_positional_args)

    # At most one of SHARED, STATIC, OBJECT may be specified. Detect this
    # by combining them all into a list, and checking the list length.
    if(${ARG_SHARED})
        list(APPEND library_type SHARED)
    endif()
    if(${ARG_STATIC})
        list(APPEND library_type STATIC)
    endif()
    if(${ARG_OBJECT})
        list(APPEND library_type OBJECT)
    endif()
    list(LENGTH library_type num_library_types_specified)

    # Check for required arguments
    if ((NOT ARG_LANGUAGE) OR
        (NOT (num_positional_args EQUAL 2)) OR
        (num_library_types_specified GREATER 1))
        message(FATAL_ERROR "Invalid invocation of add_thrift_library")
    endif()

    list(GET ARG_UNPARSED_ARGUMENTS 0 target_name)
    list(GET ARG_UNPARSED_ARGUMENTS 1 input_path)
    get_filename_component(input_name_noext ${input_path} NAME_WE)
    set(language ${ARG_LANGUAGE})
    set(output_path ${ARG_OUTPUT_PATH})
    set(include_prefix ${ARG_GENERATED_INCLUDE_PREFIX})
    set(options ${ARG_OPTIONS})

    # If output_path is not specified, default to the current binary directory
    if("${output_path}" STREQUAL "")
        set(output_path ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    # If working_directory is not specified, by default run in
    # CMAKE_CURRENT_SOURCE_DIR, so the input thrift file may be expressed
    # as a relative path.
    set(working_directory ${ARG_WORKING_DIRECTORY})
    if("${working_directory}" STREQUAL "")
        set(working_directory ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    set("${input_name_noext}-${language}-HEADERS"
      ${output_path}/gen-${language}/${input_name_noext}_constants.h
      ${output_path}/gen-${language}/${input_name_noext}_data.h
      ${output_path}/gen-${language}/${input_name_noext}_types.h
      ${output_path}/gen-${language}/${input_name_noext}_types.tcc
    )

    set("${input_name_noext}-${language}-SOURCES"
      ${output_path}/gen-${language}/${input_name_noext}_constants.cpp
      ${output_path}/gen-${language}/${input_name_noext}_data.cpp
      ${output_path}/gen-${language}/${input_name_noext}_types.cpp
    )

    foreach(service ${ARG_SERVICES})
      set("${input_name_noext}-${language}-HEADERS"
        ${${input_name_noext}-${language}-HEADERS}
        ${output_path}/gen-${language}/${service}.h
        ${output_path}/gen-${language}/${service}.tcc
        ${output_path}/gen-${language}/${service}AsyncClient.h
        ${output_path}/gen-${language}/${service}_custom_protocol.h
      )
      set("${input_name_noext}-${language}-SOURCES"
        ${${input_name_noext}-${language}-SOURCES}
        ${output_path}/gen-${language}/${service}.cpp
        ${output_path}/gen-${language}/${service}AsyncClient.cpp
      )
    endforeach()

    # cmake_parse_arguments parses multi-value arguments as lists (which are
    # represented as semicolon delimited strings). Thrift expects options
    # separated by commas
    string(REPLACE ";" "," options "${ARG_OPTIONS}")

    set(include_prefix_text "include_prefix=${include_prefix}")
    if(NOT "${options}" STREQUAL "")
      set(include_prefix_text ",${include_prefix_text}")
    endif()
    set(gen_language ${language})
    if("${language}" STREQUAL "cpp2")
      set(gen_language "mstch_cpp2")
    endif()

    foreach(incl ${ARG_THRIFT_INCLUDE})
        list(APPEND thrift_includes -I ${incl})
    endforeach()

    set(invocation
        ${THRIFT1}
        --gen
        ${gen_language}:${options}${include_prefix_text}
        -o ${output_path}
        --templates ${THRIFT_TEMPLATES}
        ${thrift_includes}
        ${input_path})

    add_custom_command(
      OUTPUT ${${input_name_noext}-${language}-HEADERS} ${${input_name_noext}-${language}-SOURCES}
      COMMAND ${invocation}
      DEPENDS ${THRIFT1}
      WORKING_DIRECTORY ${working_directory}
      COMMENT "Generating ${input_name_noext} files. Output: ${output_path}"
    )

    add_custom_target(
      ${target_name}-gen ALL
      DEPENDS
        ${${language}-${language}-HEADERS}
        ${${input_name_noext}-${language}-SOURCES}
    )

    if(ARG_INSTALL)
        install(
          DIRECTORY gen-${language}
          DESTINATION include/${include_prefix}
          FILES_MATCHING PATTERN "*.h")
        install(
          DIRECTORY gen-${language}
          DESTINATION include/${include_prefix}
          FILES_MATCHING PATTERN "*.tcc")
    endif()


    bypass_source_check(${${input_name_noext}-${language}-SOURCES})
    add_library(
      ${target_name} ${library_type}
      ${${input_name_noext}-${language}-SOURCES}
    )

    target_include_directories(
        ${target_name}
        PUBLIC
            $<BUILD_INTERFACE:${output_path}>
            $<INSTALL_INTERFACE:include>
    )

    add_dependencies(
      "${target_name}"
      "${target_name}-gen"
    )
    if(NOT ${ARG_OBJECT})
        target_link_libraries(${target_name} INTERFACE ${THRIFTCPP2})
    endif()
endfunction()
