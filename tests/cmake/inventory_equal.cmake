# Registered-vs-listed test comparison, shared by configure and the self-tests:
#   include(inventory_equal.cmake); lynx_inventory_compare(<registered> <inventory>)
# or, as a script,
#   cmake -DREGISTERED=<file> -DINVENTORY=<file> -P inventory_equal.cmake
# Fault prevented: a test that is silently not registered, or an inventory row
# for a test that does not exist, lets CI pass without running something.
# Both files hold "name<TAB>labels" rows; lines starting with # and blank lines
# are ignored. The inventory must be in LC_ALL=C order (tab sorts before the dot)
# without duplicates.
# Registration order does not matter. Every finding is reported, one line each
# with the prefix "inventory:", in a single FATAL_ERROR.
# Adapted from muhtari (MIT, owner project) at commit 7fd0386, factored out of
# its configure-time check so the same code is self-tested.

# Splits "name<TAB>rest" into name and rest (rest empty when there is no tab).
function(lynx__inventory_split row name_var rest_var)
  string(FIND "${row}" "\t" pos)
  if(pos EQUAL -1)
    set(${name_var} "${row}" PARENT_SCOPE)
    set(${rest_var} "" PARENT_SCOPE)
  else()
    string(SUBSTRING "${row}" 0 ${pos} name)
    math(EXPR from "${pos} + 1")
    string(SUBSTRING "${row}" ${from} -1 rest)
    set(${name_var} "${name}" PARENT_SCOPE)
    set(${rest_var} "${rest}" PARENT_SCOPE)
  endif()
endfunction()

function(lynx__inventory_rows file out_var)
  file(STRINGS "${file}" lines)
  set(rows "")
  foreach(line IN LISTS lines)
    if(NOT line STREQUAL "" AND NOT line MATCHES "^#")
      list(APPEND rows "${line}")
    endif()
  endforeach()
  set(${out_var} "${rows}" PARENT_SCOPE)
endfunction()

function(lynx_inventory_compare registered_file inventory_file)
  foreach(file "${registered_file}" "${inventory_file}")
    if(NOT EXISTS "${file}")
      message(FATAL_ERROR "inventory: ${file} does not exist")
    endif()
  endforeach()
  lynx__inventory_rows("${registered_file}" registered)
  lynx__inventory_rows("${inventory_file}" listed)
  set(found "")

  set(sorted ${listed})
  list(SORT sorted)
  if(NOT sorted STREQUAL listed)
    string(APPEND found "\ninventory: not sorted")
  endif()

  # Row name -> labels maps, as parallel lists keyed by name.
  set(listed_names "")
  foreach(row IN LISTS listed)
    lynx__inventory_split("${row}" name labels)
    if(name IN_LIST listed_names)
      string(APPEND found "\ninventory: duplicate row: ${name}")
    else()
      list(APPEND listed_names "${name}")
      set(listed_labels_${name} "${labels}")
    endif()
  endforeach()
  set(registered_names "")
  foreach(row IN LISTS registered)
    lynx__inventory_split("${row}" name labels)
    list(APPEND registered_names "${name}")
    set(registered_labels_${name} "${labels}")
  endforeach()

  foreach(name IN LISTS registered_names)
    if(NOT name IN_LIST listed_names)
      string(APPEND found "\ninventory: registered but not listed: ${name}")
    elseif(NOT "${registered_labels_${name}}" STREQUAL "${listed_labels_${name}}")
      string(APPEND found "\ninventory: labels differ: ${name}: registered ${registered_labels_${name}}, listed ${listed_labels_${name}}")
    endif()
  endforeach()
  foreach(name IN LISTS listed_names)
    if(NOT name IN_LIST registered_names)
      string(APPEND found "\ninventory: listed but not registered: ${name}")
    endif()
  endforeach()

  if(found)
    message(FATAL_ERROR "${inventory_file} does not match the registered tests:${found}")
  endif()
endfunction()

if(CMAKE_SCRIPT_MODE_FILE)
  if(NOT DEFINED REGISTERED OR NOT DEFINED INVENTORY)
    message(FATAL_ERROR "inventory: -DREGISTERED=<file> and -DINVENTORY=<file> are required")
  endif()
  lynx_inventory_compare("${REGISTERED}" "${INVENTORY}")
  message(STATUS "inventory: registered tests equal ${INVENTORY}")
endif()
