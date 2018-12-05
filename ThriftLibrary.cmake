#
# Requirements:
# Please provide the following two variables before using these macros:
#   ${THRIFT1} - path/to/bin/thrift1
#   ${THRIFT_TEMPLATES} - path/to/include/thrift/templates
#   ${THRIFTCPP2} - path/to/lib/thriftcpp2
#

include(CMakeParseArguments)


# See usage in thrift_generate2
#
# This constructs a cmake object library called ${file_name}-${language}-obj,
macro(thrift_object2)
    thrift_generate2(${ARGV})
    bypass_source_check(${${file_name}-${language}-SOURCES})
    add_library(
      "${file_name}-${language}-obj"
      OBJECT
      ${${file_name}-${language}-SOURCES}
    )
    add_dependencies(
      "${file_name}-${language}-obj"
      "${file_name}-${language}-target"
    )
    message("Thrift will create the Object file : ${file_name}-${language}-obj")
endmacro()

#
# thrift_object - DEPRECATED. Use thrift_object2.
#
# This creates a object that will contain the source files and all the proper
# dependencies to generate and compile thrift generated files
#
# Params:
#   @file_name - The name of the thrift file
#   @services  - A list of services that are declared in the thrift file
#   @language  - The generator to use (cpp or cpp2)
#   @options   - Extra options to pass to the generator
#   @file_path - The directory where the thrift file lives
#   @output_path - The directory where the thrift objects will be built
#   @include_prefix - The last part of output_path, relative include prefix
#
# Output:
#  A object file named `${file-name}-${language}-obj` to include into your
#  project's library
#
# Notes:
# If any of the fields is empty, it is still required to provide an empty string
#
# Usage:
#   thrift_object(
#     #file_name
#     #services
#     #language
#     #options
#     #file_path
#     #output_path
#     #include_prefix
#   )
#   add_library(somelib $<TARGET_OBJECTS:${file_name}-${language}-obj> ...)
#
macro(thrift_object file_name services language options file_path output_path include_prefix)
    thrift_object2(
        FILE ${file_name}
        SERVICES ${services}
        LANGUAGE ${language}
        OPTIONS ${options}
        FILE_DIRECTORY ${file_path}
        OUTPUT_PATH ${output_path}
        GENERATED_INCLUDE_PREFIX ${include_prefix}
    )
endmacro()

macro(thrift_library2)
    thrift_object2(${ARGV})
    add_library(
      "${file_name}-${language}"
      $<TARGET_OBJECTS:${file_name}-${language}-obj>
    )
    target_link_libraries("${file_name}-${language}" ${THRIFTCPP2})
    message("Thrift will create the Library file : ${file_name}-${language}")
endmacro()

# thrift_library - DEPRECATED. Use thrift_library2
# Same as thrift object in terms of usage but creates the library instead of
# object so that you can use to link against your library instead of including
# all symbols into your library
#
# Params:
#   @file_name - The name of the thrift file
#   @services  - A list of services that are declared in the thrift file
#   @language  - The generator to use (cpp or cpp2)
#   @options   - Extra options to pass to the generator
#   @file_path - The directory where the thrift file lives
#   @output_path - The directory where the thrift objects will be built
#   @include_prefix - The last part of output_path, relative include prefix
#
# Output:
#  A library file named `${file-name}-${language}` to link against your
#  project's library
#
# Notes:
# If any of the fields is empty, it is still required to provide an empty string
#
# Usage:
#   thrift_library(
#     #file_name
#     #services
#     #language
#     #options
#     #file_path
#     #output_path
#     #include_prefix
#   )
#   add_library(somelib ...)
#   target_link_libraries(somelibe ${file_name}-${language} ...)
#

macro(thrift_library file_name services language options file_path output_path include_prefix)
thrift_library2(
    FILE "${file_name}"
    SERVICES "${services}"
    LANGUAGE "${language}"
    OPTIONS "${options}"
    FILE_DIRECTORY "${file_path}"
    OUTPUT_PATH "${output_path}"
    GENERATED_INCLUDE_PREFIX "${include_prefix}"
)
endmacro()

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

#
# thrift_generate2
#
# This is used to codegen thrift files using the thrift compiler
#   SYNTAX:
#     thrift_generate2(
#       FILE interface.thrift
#       FILE_DIRECTORY directory
#       LANGUAGE language
#       OUTPUT_PATH output_directory
#       [SERVICES [svc1] [svc2] [svc3]...]
#       [WORKING_DIRECTORY working_directory]
#       [TARGET_NAME cmake_target_name]
#       [INSTALL]
#       [OPTIONS additional_option1 [additional_option2] ...]
#       [THRIFT_INCLUDE include_dir1 [include_dir2] ...]
#       [GENERATED_INCLUDE_PREFIX include_prefix]
#     )

macro(thrift_generate2)
    cmake_parse_arguments(
        THRIFT_GENERATE_MACRO
        "INSTALL"
        "FILE;FILE_DIRECTORY;LANGUAGE;OUTPUT_PATH;TARGET_NAME;GENERATED_INCLUDE_PREFIX"
        "SERVICES;OPTIONS;THRIFT_INCLUDE"
        ${ARGN}
    )
    if ((NOT THRIFT_GENERATE_MACRO_FILE) OR
        (NOT THRIFT_GENERATE_MACRO_OUTPUT_PATH) OR
        (NOT THRIFT_GENERATE_MACRO_LANGUAGE) OR
        (NOT THRIFT_GENERATE_MACRO_FILE_DIRECTORY))
        message(FATAL_ERROR "Invalid invocation of thrift_generate2")
    endif()

    set(language ${THRIFT_GENERATE_MACRO_LANGUAGE})
    set(file_name ${THRIFT_GENERATE_MACRO_FILE})
    set(file_path ${THRIFT_GENERATE_MACRO_FILE_DIRECTORY})
    set(output_path ${THRIFT_GENERATE_MACRO_OUTPUT_PATH})
    set(include_prefix ${THRIFT_GENERATE_MACRO_GENERATED_INCLUDE_PREFIX})
    set(options ${THRIFT_GENERATE_MACRO_OPTIONS})

    set("${file_name}-${language}-HEADERS"
      ${output_path}/gen-${language}/${file_name}_constants.h
      ${output_path}/gen-${language}/${file_name}_data.h
      ${output_path}/gen-${language}/${file_name}_types.h
      ${output_path}/gen-${language}/${file_name}_types.tcc
    )
    set("${file_name}-${language}-SOURCES"
      ${output_path}/gen-${language}/${file_name}_constants.cpp
      ${output_path}/gen-${language}/${file_name}_data.cpp
      ${output_path}/gen-${language}/${file_name}_types.cpp
    )

    foreach(service ${THRIFT_GENERATE_MACRO_SERVICES})
      set("${file_name}-${language}-HEADERS"
        ${${file_name}-${language}-HEADERS}
        ${output_path}/gen-${language}/${service}.h
        ${output_path}/gen-${language}/${service}.tcc
        ${output_path}/gen-${language}/${service}AsyncClient.h
        ${output_path}/gen-${language}/${service}_custom_protocol.h
      )
      set("${file_name}-${language}-SOURCES"
        ${${file_name}-${language}-SOURCES}
        ${output_path}/gen-${language}/${service}.cpp
        ${output_path}/gen-${language}/${service}AsyncClient.cpp
      )
    endforeach()

    # cmake_parse_arguments parses multi-value arguments as lists (which are
    # represented as semicolon delimited strings). Thrift expects options
    # separated by commas
    string(REPLACE ";" "," options "${THRIFT_GENERATE_MACRO_OPTIONS}")

    set(include_prefix_text "include_prefix=${include_prefix}")
    message("include prefix text is ${include_prefix_text}")
    if(NOT "${options}" STREQUAL "")
      set(include_prefix_text ",${include_prefix_text}")
    endif()
    set(gen_language ${language})
    if("${language}" STREQUAL "cpp2")
      set(gen_language "mstch_cpp2")
    endif()

    set(thrift_includes "")
    foreach(incl ${THRIFT_GENERATE_MACRO_THRIFT_INCLUDE})
        set(thrift_includes ${thrift_includes} -I "${incl}")
    endforeach()

    set(invocation
        ${THRIFT1}
        --gen
        ${gen_language}:${options}${include_prefix_text}
        -o ${output_path}
        --templates ${THRIFT_TEMPLATES}
        ${thrift_includes}
        ${file_path}/${file_name}.thrift)

    message("invocation is ${invocation}")
    add_custom_command(
      OUTPUT ${${file_name}-${language}-HEADERS} ${${file_name}-${language}-SOURCES}
      COMMAND ${invocation}
      DEPENDS ${THRIFT1}
      WORKING_DIRECTORY ${THRIFT_GENERATE_MACRO_WORKING_DIRECTORY}
      COMMENT "Generating ${file_name} files. Output: ${output_path}"
    )

    set(gen_target_name "${THRIFT_GENERATE_MACRO_TARGET_NAME}")
    if(gen_target_name STREQUAL "")
        set(gen_target_name "${file_name}-${language}-target")
    endif()

    add_custom_target(
      ${gen_target_name} ALL
      DEPENDS ${${language}-${language}-HEADERS} ${${file_name}-${language}-SOURCES}
    )

    if(THRIFT_GENERATE_MACRO_INSTALL)
        install(
          DIRECTORY gen-${language}
          DESTINATION include/${include_prefix}
          FILES_MATCHING PATTERN "*.h")
        install(
          DIRECTORY gen-${language}
          DESTINATION include/${include_prefix}
          FILES_MATCHING PATTERN "*.tcc")
    endif()

endmacro()

#
# thrift_generate
# This is used to codegen thrift files using the thrift compiler
# Params:
#   @file_name - The name of tge thrift file
#   @services  - A list of services that are declared in the thrift file
#   @language  - The generator to use (cpp or cpp2)
#   @options   - Extra options to pass to the generator
#   @output_path - The directory where the thrift file lives
#
# Output:
#  file-language-target     - A custom target to add a dependenct
#  ${file-language-HEADERS} - The generated Header Files.
#  ${file-language-SOURCES} - The generated Source Files.
#
# Notes:
# If any of the fields is empty, it is still required to provide an empty string
#
# When using file_language-SOURCES it should always call:
#   bypass_source_check(${file_language-SOURCES})
# This will prevent cmake from complaining about missing source files
#

macro(thrift_generate file_name services language options file_path output_path include_prefix)
    thrift_generate2(
        FILE ${file_name}
        FILE_DIRECTORY ${file_path}
        SERVICES ${services}
        LANGUAGE ${language}
        OPTIONS ${options}
        OUTPUT_PATH ${output_path}
        GENERATED_INCLUDE_PREFIX ${include_prefix}
        INSTALL
    )
endmacro()
