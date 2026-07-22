if(NOT DEFINED OBJDUMP OR NOT DEFINED PORTABLE_EXECUTABLE)
  message(FATAL_ERROR "OBJDUMP and PORTABLE_EXECUTABLE are required")
endif()

execute_process(
  COMMAND "${OBJDUMP}" -p "${PORTABLE_EXECUTABLE}"
  RESULT_VARIABLE objdump_result
  OUTPUT_VARIABLE objdump_output
  ERROR_VARIABLE objdump_error)
if(NOT objdump_result EQUAL 0)
  message(FATAL_ERROR
    "Failed to inspect ${PORTABLE_EXECUTABLE}: ${objdump_error}")
endif()

string(REGEX MATCHALL "DLL Name: [^\r\n]+" imported_dll_lines
       "${objdump_output}")
if(NOT imported_dll_lines)
  message(FATAL_ERROR
    "Failed to parse any DLL imports from ${PORTABLE_EXECUTABLE}")
endif()

set(system_dlls
  advapi32.dll
  bcrypt.dll
  cfgmgr32.dll
  comctl32.dll
  comdlg32.dll
  crypt32.dll
  d2d1.dll
  d3d11.dll
  dnsapi.dll
  dwmapi.dll
  dxgi.dll
  gdi32.dll
  imm32.dll
  iphlpapi.dll
  kernel32.dll
  mf.dll
  mfplat.dll
  mfreadwrite.dll
  mfuuid.dll
  msimg32.dll
  msvcrt.dll
  ncrypt.dll
  ole32.dll
  oleaut32.dll
  powrprof.dll
  propsys.dll
  rpcrt4.dll
  secur32.dll
  setupapi.dll
  shell32.dll
  shlwapi.dll
  user32.dll
  userenv.dll
  uxtheme.dll
  version.dll
  winhttp.dll
  winmm.dll
  winspool.drv
  ws2_32.dll
  wtsapi32.dll)

set(unexpected_dlls)
foreach(import_line IN LISTS imported_dll_lines)
  string(REGEX REPLACE "DLL Name: " "" dll_name "${import_line}")
  string(STRIP "${dll_name}" dll_name)
  string(TOLOWER "${dll_name}" dll_name_lower)
  list(FIND system_dlls "${dll_name_lower}" system_dll_index)
  if(system_dll_index EQUAL -1 AND
     NOT dll_name_lower MATCHES "^(api|ext)-ms-win-")
    list(APPEND unexpected_dlls "${dll_name}")
  endif()
endforeach()

if(unexpected_dlls)
  list(JOIN unexpected_dlls ", " unexpected_dll_list)
  message(FATAL_ERROR
    "Portable vncviewer imports non-system DLLs: ${unexpected_dll_list}. "
    "Build with x86_64-w64-mingw32.static MXE dependencies.")
endif()

message(STATUS "Portable dependency check passed: Windows system DLLs only")
