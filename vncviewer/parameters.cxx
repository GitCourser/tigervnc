/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011 Pierre Ossman <ossman@cendio.se> for Cendio AB
 * Copyright 2012 Samuel Mannehed <samuel@cendio.se> for Cendio AB
 * 
 * This is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This software is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this software; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
 * USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_GNUTLS
#include <rfb/CSecurityTLS.h>
#endif

#ifdef _WIN32
#include <windows.h>
#endif

#include "parameters.h"

#include <core/Exception.h>
#include <core/LogWriter.h>
#include <core/i18n.h>
#include <core/string.h>
#include <core/xdgdirs.h>

#include <rfb/CConnection.h>
#include <rfb/SecurityClient.h>

#include <FL/fl_utf8.h>

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>
#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#include <stdint.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif
#include <list>
#include <string>

static core::LogWriter vlog("Parameters");

core::IntParameter
  pointerEventInterval("PointerEventInterval",
                       _("Time in milliseconds to rate-limit "
                         "successive pointer events"),
                       17, 0, INT_MAX);
core::BoolParameter
  emulateMiddleButton("EmulateMiddleButton",
                      _("Emulate middle mouse button by pressing left "
                        "and right mouse buttons simultaneously"),
                      false);
core::BoolParameter
  dotWhenNoCursor("DotWhenNoCursor",
                  _("[DEPRECATED] Show a dot cursor when the server "
                    "sends an invisible cursor"),
                  false);
core::BoolParameter
  alwaysCursor("AlwaysCursor",
               _("Show local cursor when not provided by the server"),
               false);
core::EnumParameter
  cursorType("CursorType",
             core::format(
               "%s (%s)",
               _("Specify which local cursor type should be used when "
                 "AlwaysCursor is active"), ("Dot, System")).c_str(),
             {"Dot", "System"}, "Dot");

core::BoolParameter
  alertOnFatalError("AlertOnFatalError",
                    _("Show an error dialog on connection problems, "
                      "rather than exiting immediately"),
                    true);

core::BoolParameter
  reconnectOnError("ReconnectOnError",
                   _("Show an error dialog on connection problems, "
                     "rather than exiting immediately, and allow the "
                     "user to reconnect"),
                   true);

core::StringParameter
  passwordFile("PasswordFile",
               _("Password file for VNC authentication"),
               "");
core::AliasParameter
  passwd("passwd", &passwordFile);

core::BoolParameter
  autoSelect("AutoSelect",
             _("Auto select pixel format and encoding"),
             true);
core::BoolParameter
  fullColour("FullColor", _("Use all available colors"), true);
core::AliasParameter
  fullColourAlias("FullColour", &fullColour);
core::IntParameter
  lowColourLevel("LowColorLevel",
                 _("Color level to use on slow connections, "
                   "0 = Very Low, 1 = Low, 2 = Medium"),
                 2, 0, 2);
core::AliasParameter
  lowColourLevelAlias("LowColourLevel", &lowColourLevel);
core::EnumParameter
  preferredEncoding("PreferredEncoding",
                    core::format(
                      "%s (%s)",
                      _("Preferred encoding to use"),
                      "Tight, JPEG, ZRLE, Hextile, "
#ifdef HAVE_H264
                      "H.264, "
#endif
                      "Raw)").c_str(),
                    {"Tight", "JPEG", "ZRLE", "Hextile",
#ifdef HAVE_H264
                     "H.264",
#endif
                     "Raw"},
                    "Tight");
core::BoolParameter
  customCompressLevel("CustomCompressLevel",
                      _("Use custom compression level as specified by "
                        "CompressLevel"),
                      false);
core::IntParameter
  compressLevel("CompressLevel",
                _("Use specified compression level, 0 = Low, 9 = High"),
                2, 0, 9);
core::IntParameter
  qualityLevel("QualityLevel",
               _("JPEG quality level, 0 = Low, 9 = High"),
               8, 0, 9);

core::BoolParameter
  maximize("Maximize", _("Maximize viewer window"), false);
core::BoolParameter
  fullScreen("FullScreen",
             _("Enable full screen as specified by FullScreenMode"),
             false);
core::EnumParameter
  fullScreenMode("FullScreenMode",
                 core::format(
                   "%s (%s)",
                   _("Specify which monitors to use when in "
                     "full screen"), "Current, Selected, All").c_str(),
                 {"Current", "Selected", "All"}, "Current");

core::BoolParameter
  fullScreenAllMonitors("FullScreenAllMonitors",
                        _("[DEPRECATED] Enable full screen over all "
                          "monitors"),
                        false);
MonitorIndicesParameter
  fullScreenSelectedMonitors("FullScreenSelectedMonitors",
                             _("Use the given list of monitors in full "
                               "screen when FullScreenMode is set to "
                               "Selected"),
                             {1});
core::StringParameter
  desktopSize("DesktopSize",
              _("Reconfigure the desktop size on the server to the "
                "specified size when connecting"),
              "");
core::StringParameter
  geometry("geometry",
           _("Specify size and position of viewer window"),
           "");

core::BoolParameter
  listenMode("listen",
             _("Listen for incoming connections from VNC servers"),
             false);

core::BoolParameter
  remoteResize("RemoteResize",
               _("Dynamically resize the remote desktop size as the "
                 "size of the local client window changes"),
               true);

core::BoolParameter
  viewOnly("ViewOnly",
           _("Don't send any mouse or keyboard events to the server"),
           false);
core::BoolParameter
  shared("Shared",
         _("Don't disconnect other viewers upon connection"),
         false);

core::BoolParameter
  acceptClipboard("AcceptClipboard",
                  _("Accept clipboard changes from the server"),
                  true);
core::BoolParameter
  sendClipboard("SendClipboard",
                _("Send clipboard changes to the server"),
                true);
#if !defined(WIN32) && !defined(__APPLE__)
core::BoolParameter
  setPrimary("SetPrimary",
             // TRANSLATORS: This refers to the two different X11
             //              clipboards
             _("Set the primary selection as well as the clipboard "
               "selection"),
             true);
core::BoolParameter
  sendPrimary("SendPrimary",
              // TRANSLATORS: This refers to the two different X11
              //              clipboards
              _("Send the primary selection to the server as well as "
                "the clipboard selection"),
              true);
core::StringParameter
  display("display", _("The X display to use"), "");
#endif

// Keep list of valid values in sync with ShortcutHandler
core::EnumListParameter
  shortcutModifiers("ShortcutModifiers",
                    // TRANSLATORS: The key names must be specified in
                    //              English
                    _("The combination of modifier keys that triggers "
                      "special actions in the viewer instead of being "
                      "sent to the remote session (possible keys are a "
                      "combination of Ctrl, Shift, Alt, and Super)"),
                    {"Ctrl", "Shift", "Alt", "Super",
                     "Win", "Option", "Cmd"},
                    {"Ctrl", "Alt"});

core::BoolParameter
  fullscreenSystemKeys("FullscreenSystemKeys",
                       _("Pass special keys (like Alt+Tab) directly to "
                         "the server when in full-screen mode"),
                       true);

#ifndef WIN32
core::StringParameter
  via("via", _("SSH gateway to tunnel the connection via"), "");
#endif

static const char* IDENTIFIER_STRING = "TigerVNC Configuration file Version 1.0";


#ifdef BUILD_PORTABLE_VIEWER
enum class ApplyPortableParameters { No, Yes };

struct PortableConfig {
  std::string serverName;
  std::list<std::string> history;
};

static bool parsePortableIni(PortableConfig* config,
                             ApplyPortableParameters applyParameters);
static void savePortableConfig(const char* servername,
                               const std::list<std::string>* historyIn);
static void savePortableHistory(const std::list<std::string>& history);
#endif


/*
 * We only save the sub set of parameters that can be modified from
 * the graphical user interface
 */
static core::VoidParameter* parameterArray[] = {
  /* Security */
#ifdef HAVE_GNUTLS
  &rfb::CSecurityTLS::X509CA,
  &rfb::CSecurityTLS::X509CRL,
#endif // HAVE_GNUTLS
  &rfb::SecurityClient::secTypes,
  /* Misc. */
  &reconnectOnError,
  &shared,
  /* Compression */
  &autoSelect,
  &fullColour,
  &lowColourLevel,
  &preferredEncoding,
  &customCompressLevel,
  &compressLevel,
  &rfb::CConnection::noJpeg,
  &qualityLevel,
  /* Display */
  &fullScreen,
  &fullScreenMode,
  &fullScreenSelectedMonitors,
  /* Input */
  &viewOnly,
  &emulateMiddleButton,
  &alwaysCursor,
  &cursorType,
  &acceptClipboard,
  &sendClipboard,
#if !defined(WIN32) && !defined(__APPLE__)
  &sendPrimary,
  &setPrimary,
#endif
  &fullscreenSystemKeys,
  /* Keyboard shortcuts */
  &shortcutModifiers,
};

static core::VoidParameter* readOnlyParameterArray[] = {
  &fullScreenAllMonitors,
  &dotWhenNoCursor
};

// Encoding Table
static const struct EscapeMap {
  const char first;
  const char second;
} replaceMap[] = { { '\n', 'n' },
                   { '\r', 'r' },
                   { '\\', '\\' } };

static bool encodeValue(const char* val, char* dest, size_t destSize) {

  size_t pos = 0;

  for (size_t i = 0; (val[i] != '\0') && (i < (destSize - 1)); i++) {
    bool normalCharacter;
    
    // Check for sequences which will need encoding
    normalCharacter = true;
    for (EscapeMap esc : replaceMap) {

      if (val[i] == esc.first) {
        dest[pos] = '\\';
        pos++;
        if (pos >= destSize)
          return false;

        dest[pos] = esc.second;
        normalCharacter = false;
        break;
      }

      if (normalCharacter) {
        dest[pos] = val[i];
      }
    }

    pos++;
    if (pos >= destSize)
      return false;
  }

  dest[pos] = '\0';
  return true;
}


static bool decodeValue(const char* val, char* dest, size_t destSize) {

  size_t pos = 0;
  
  for (size_t i = 0; (val[i] != '\0') && (i < (destSize - 1)); i++) {
    
    // Check for escape sequences
    if (val[i] == '\\') {
      bool escapedCharacter;
      
      escapedCharacter = false;
      for (EscapeMap esc : replaceMap) {
        if (val[i+1] == esc.second) {
          dest[pos] = esc.first;
          escapedCharacter = true;
          i++;
          break;
        }
      }

      if (!escapedCharacter)
        return false;

    } else {
      dest[pos] = val[i];
    }

    pos++;
    if (pos >= destSize) {
      return false;
    }
  }
  
  dest[pos] = '\0';
  return true;
}


#if defined(_WIN32) && !defined(BUILD_PORTABLE_VIEWER)
static void setKeyString(const char *_name, const char *_value, HKEY* hKey) {
  
  const DWORD buffersize = 256;

  wchar_t name[buffersize];
  unsigned size = fl_utf8towc(_name, strlen(_name)+1, name, buffersize);
  if (size >= buffersize)
    throw std::length_error("The name of the parameter is too large");

  char encodingBuffer[buffersize];
  if (!encodeValue(_value, encodingBuffer, buffersize))
    throw std::length_error("The parameter is too large");

  wchar_t value[buffersize];
  size = fl_utf8towc(encodingBuffer, strlen(encodingBuffer)+1, value, buffersize);
  if (size >= buffersize)
    throw std::length_error("The parameter is too large");

  LONG res = RegSetValueExW(*hKey, name, 0, REG_SZ, (BYTE*)&value, (wcslen(value)+1)*2);
  if (res != ERROR_SUCCESS)
    throw core::win32_error("RegSetValueExW", res);
}


static void setKeyInt(const char *_name, const int _value, HKEY* hKey) {

  const DWORD buffersize = 256;
  wchar_t name[buffersize];
  DWORD value = _value;

  unsigned size = fl_utf8towc(_name, strlen(_name)+1, name, buffersize);
  if (size >= buffersize)
    throw std::length_error("The name of the parameter is too large");

  LONG res = RegSetValueExW(*hKey, name, 0, REG_DWORD, (BYTE*)&value, sizeof(DWORD));
  if (res != ERROR_SUCCESS)
    throw core::win32_error("RegSetValueExW", res);
}


static bool getKeyString(const char* _name, char* dest, size_t destSize, HKEY* hKey) {
  
  const DWORD buffersize = 256;
  wchar_t name[buffersize];
  WCHAR* value;
  DWORD valuesize;

  unsigned size = fl_utf8towc(_name, strlen(_name)+1, name, buffersize);
  if (size >= buffersize)
    throw std::length_error("The name of the parameter is too large");

  value = new WCHAR[destSize];
  valuesize = destSize;
  LONG res = RegQueryValueExW(*hKey, name, nullptr, nullptr, (LPBYTE)value, &valuesize);
  if (res != ERROR_SUCCESS){
    delete [] value;
    if (res != ERROR_FILE_NOT_FOUND)
      throw core::win32_error("RegQueryValueExW", res);
    // The value does not exist, defaults will be used.
    return false;
  }

  char* utf8val = new char[destSize];
  size = fl_utf8fromwc(utf8val, destSize, value, wcslen(value)+1);
  delete [] value;
  if (size >= destSize) {
    delete [] utf8val;
    throw std::length_error("The parameter is too large");
  }

  bool ret = decodeValue(utf8val, dest, destSize);
  delete [] utf8val;

  if (!ret)
    throw std::invalid_argument("Invalid format or too large value");

  return true;
}


static bool getKeyInt(const char* _name, int* dest, HKEY* hKey) {
  
  const DWORD buffersize = 256;
  DWORD dwordsize = sizeof(DWORD);
  DWORD value = 0;
  wchar_t name[buffersize];

  unsigned size = fl_utf8towc(_name, strlen(_name)+1, name, buffersize);
  if (size >= buffersize)
    throw std::length_error("The name of the parameter is too large");

  LONG res = RegQueryValueExW(*hKey, name, nullptr, nullptr, (LPBYTE)&value, &dwordsize);
  if (res != ERROR_SUCCESS){
    if (res != ERROR_FILE_NOT_FOUND)
      throw core::win32_error("RegQueryValueExW", res);
    // The value does not exist, defaults will be used.
    return false;
  }

  *dest = (int)value;
  return true;
}

static void removeValue(const char* _name, HKEY* hKey) {
  const DWORD buffersize = 256;
  wchar_t name[buffersize];

  unsigned size = fl_utf8towc(_name, strlen(_name)+1, name, buffersize);
  if (size >= buffersize)
    throw std::length_error("The name of the parameter is too large");

  LONG res = RegDeleteValueW(*hKey, name);
  if (res != ERROR_SUCCESS) {
    if (res != ERROR_FILE_NOT_FOUND)
      throw core::win32_error("RegDeleteValueW", res);
    // The value does not exist, no need to remove it.
    return;
  }
}

void saveHistoryToRegKey(const std::list<std::string>& serverHistory)
{
  HKEY hKey;
  LONG res = RegCreateKeyExW(HKEY_CURRENT_USER,
                             L"Software\\TigerVNC\\vncviewer\\history", 0, nullptr,
                             REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr,
                             &hKey, nullptr);

  if (res != ERROR_SUCCESS)
    throw core::win32_error(_("Failed to create registry key"), res);

  unsigned index = 0;
  assert(SERVER_HISTORY_SIZE < 100);
  char indexString[3];

  try {
    for (const std::string& entry : serverHistory) {
      if (index > SERVER_HISTORY_SIZE)
        break;
      snprintf(indexString, 3, "%d", index);
      setKeyString(indexString, entry.c_str(), &hKey);
      index++;
    }
  } catch (std::exception& e) {
    RegCloseKey(hKey);
    throw;
  }

  res = RegCloseKey(hKey);
  if (res != ERROR_SUCCESS)
    throw core::win32_error(_("Failed to close registry key"), res);
}

static void saveToReg(const char* servername) {
  
  HKEY hKey;
    
  LONG res = RegCreateKeyExW(HKEY_CURRENT_USER,
                             L"Software\\TigerVNC\\vncviewer", 0, nullptr,
                             REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, nullptr,
                             &hKey, nullptr);
  if (res != ERROR_SUCCESS)
    throw core::win32_error(_("Failed to create registry key"), res);

  try {
    setKeyString("ServerName", servername, &hKey);
  } catch (std::exception& e) {
    RegCloseKey(hKey);
    throw std::runtime_error(core::format(
      _("Failed to save \"%s\": %s"), "ServerName", e.what()));
  }

  for (core::VoidParameter* param : parameterArray) {
    core::IntParameter* iparam;
    core::BoolParameter* bparam;

    if (param->isDefault()) {
      try {
        removeValue(param->getName(), &hKey);
      } catch (std::exception& e) {
        RegCloseKey(hKey);
        throw std::runtime_error(
          core::format(_("Failed to remove \"%s\": %s"),
                       param->getName(), e.what()));
      }
      continue;
    }

    iparam = dynamic_cast<core::IntParameter*>(param);
    bparam = dynamic_cast<core::BoolParameter*>(param);

    try {
      if (iparam != nullptr) {
        setKeyInt(iparam->getName(), (int)*(iparam), &hKey);
      } else if (bparam != nullptr) {
        setKeyInt(bparam->getName(), (int)*(bparam), &hKey);
      } else {
        setKeyString(param->getName(), param->getValueStr().c_str(), &hKey);
      }
    } catch (std::exception& e) {
      RegCloseKey(hKey);
      throw std::runtime_error(
        core::format(_("Failed to save \"%s\": %s"),
                     param->getName(), e.what()));
    }
  }

  // Remove read-only parameters to replicate the behaviour of Linux/macOS when they
  // store a config to disk. If the parameter hasn't been migrated at this point it
  // will be lost.
  for (core::VoidParameter* param : readOnlyParameterArray) {
    try {
      removeValue(param->getName(), &hKey);
    } catch (std::exception& e) {
      RegCloseKey(hKey);
      throw std::runtime_error(
        core::format(_("Failed to remove \"%s\": %s"),
                     param->getName(), e.what()));
    }
  }

  res = RegCloseKey(hKey);
  if (res != ERROR_SUCCESS)
    throw core::win32_error(_("Failed to close registry key"), res);
}

std::list<std::string> loadHistoryFromRegKey()
{
  HKEY hKey;
  std::list<std::string> serverHistory;

  LONG res = RegOpenKeyExW(HKEY_CURRENT_USER,
                           L"Software\\TigerVNC\\vncviewer\\history", 0,
                           KEY_READ, &hKey);
  if (res != ERROR_SUCCESS) {
    if (res == ERROR_FILE_NOT_FOUND) {
      // The key does not exist, defaults will be used.
      return serverHistory;
    }

    throw core::win32_error(_("Failed to open registry key"), res);
  }

  unsigned index;
  const DWORD buffersize = 256;
  char indexString[3];

  for (index = 0;;index++) {
    snprintf(indexString, 3, "%d", index);
    char servernameBuffer[buffersize];

    try {
      if (!getKeyString(indexString, servernameBuffer,
                        buffersize, &hKey))
        break;
    } catch (std::exception& e) {
      // Just ignore this entry and try the next one
      vlog.error(_("Failed to read server history entry %d: %s"),
                 (int)index, e.what());
      continue;
    }

    serverHistory.push_back(servernameBuffer);
  }

  res = RegCloseKey(hKey);
  if (res != ERROR_SUCCESS)
    throw core::win32_error(_("Failed to close registry key"), res);

  return serverHistory;
}

static void getParametersFromReg(core::VoidParameter* parameters[],
                                 size_t parameters_len, HKEY* hKey)
{
  const size_t buffersize = 256;
  int intValue = 0;
  char stringValue[buffersize];

  for (size_t i = 0; i < parameters_len; i++) {
    core::IntParameter* iparam;
    core::BoolParameter* bparam;

    iparam = dynamic_cast<core::IntParameter*>(parameters[i]);
    bparam = dynamic_cast<core::BoolParameter*>(parameters[i]);

    try {
      if (iparam != nullptr) {
        if (getKeyInt(iparam->getName(), &intValue, hKey))
          iparam->setParam(intValue);
      } else if (bparam != nullptr) {
        if (getKeyInt(bparam->getName(), &intValue, hKey))
          bparam->setParam(intValue);
      } else {
        if (getKeyString(parameters[i]->getName(), stringValue, buffersize, hKey))
          parameters[i]->setParam(stringValue);
      }
    } catch(std::exception& e) {
      // Just ignore this entry and continue with the rest
      vlog.error(_("Failed to read parameter \"%s\": %s"),
                 parameters[i]->getName(), e.what());
    }
  }
}

static char* loadFromReg() {

  HKEY hKey;

  LONG res = RegOpenKeyExW(HKEY_CURRENT_USER,
                           L"Software\\TigerVNC\\vncviewer", 0,
                           KEY_READ, &hKey);
  if (res != ERROR_SUCCESS) {
    if (res == ERROR_FILE_NOT_FOUND) {
      // The key does not exist, defaults will be used.
      return nullptr;
    }

    throw core::win32_error(_("Failed to open registry key"), res);
  }

  const size_t buffersize = 256;
  static char servername[buffersize];

  char servernameBuffer[buffersize];
  try {
    if (getKeyString("ServerName", servernameBuffer, buffersize, &hKey))
      snprintf(servername, buffersize, "%s", servernameBuffer);
  } catch(std::exception& e) {
    vlog.error(_("Failed to read parameter \"%s\": %s"),
               "ServerName", e.what());
    strcpy(servername, "");
  }

  getParametersFromReg(parameterArray,
                       sizeof(parameterArray) /
                         sizeof(core::VoidParameter*),
                       &hKey);
  getParametersFromReg(readOnlyParameterArray,
                       sizeof(readOnlyParameterArray) /
                         sizeof(core::VoidParameter*),
                       &hKey);

  res = RegCloseKey(hKey);
  if (res != ERROR_SUCCESS)
    throw core::win32_error(_("Failed to close registry key"), res);

  migrateDeprecatedOptions();

  return servername;
}
#endif // defined(_WIN32) && !defined(BUILD_PORTABLE_VIEWER)


void saveViewerParameters(const char *filename, const char *servername) {

  const size_t buffersize = 256;
  char filepath[PATH_MAX];
  char encodingBuffer[buffersize];

  // Write to the registry/INI or a predefined file if no filename was specified.
  if(filename == nullptr) {

#if defined(_WIN32) && !defined(BUILD_PORTABLE_VIEWER)
    saveToReg(servername);
    return;
#endif

#ifdef BUILD_PORTABLE_VIEWER
    savePortableConfig(servername, nullptr);
    return;
#endif

    const char* configDir = core::getvncconfigdir();
    if (configDir == nullptr)
      throw std::runtime_error(_("Could not determine VNC config directory path"));

    snprintf(filepath, sizeof(filepath), "%s/default.tigervnc", configDir);
  } else {
    snprintf(filepath, sizeof(filepath), "%s", filename);
  }

  /* Write parameters to file */
  FILE* f = fopen(filepath, "w+");
  if (!f)
    throw core::posix_error(
      core::format(_("Failed to open \"%s\""), filepath), errno);

  fprintf(f, "%s\n", IDENTIFIER_STRING);
  fprintf(f, "\n");

  if (!encodeValue(servername, encodingBuffer, buffersize)) {
    fclose(f);
    throw std::runtime_error(
      core::format(_("Failed to save \"%s\": %s"), "ServerName",
                   _("Could not encode parameter")));
  }
  fprintf(f, "ServerName=%s\n", encodingBuffer);

  for (core::VoidParameter* param : parameterArray) {
    if (param->isDefault())
      continue;
    if (!encodeValue(param->getValueStr().c_str(),
                     encodingBuffer, buffersize)) {
      fclose(f);
      throw std::runtime_error(
        core::format(_("Failed to save \"%s\": %s"), param->getName(),
                     _("Could not encode parameter")));
    }
    fprintf(f, "%s=%s\n", param->getName(), encodingBuffer);
  }
  fclose(f);
}

static bool findAndSetViewerParameterFromValue(
  core::VoidParameter* parameters[], size_t parameters_len,
  char* value, char* line)
{
  const size_t buffersize = 256;
  char decodingBuffer[buffersize];

  // Find and set the correct parameter
  for (size_t i = 0; i < parameters_len; i++) {
    if (strcasecmp(line, parameters[i]->getName()) == 0) {
      if(!decodeValue(value, decodingBuffer, sizeof(decodingBuffer)))
        throw std::runtime_error(_("Invalid format or too large value"));
      parameters[i]->setParam(decodingBuffer);
      return false;
    }
  }

  return true;
}

char* loadViewerParameters(const char *filename) {

  const size_t buffersize = 256;
  char filepath[PATH_MAX];
  char line[buffersize];
  char decodingBuffer[buffersize];
  static char servername[sizeof(line)];

  memset(servername, '\0', sizeof(servername));

  // Load from the registry/INI or a predefined file if no filename was specified.
  if(filename == nullptr) {

#if defined(_WIN32) && !defined(BUILD_PORTABLE_VIEWER)
    return loadFromReg();
#endif

#ifdef BUILD_PORTABLE_VIEWER
    PortableConfig config;
    if (!parsePortableIni(&config, ApplyPortableParameters::Yes))
      return nullptr;
    snprintf(servername, sizeof(servername), "%s", config.serverName.c_str());
    return servername;
#endif

    const char* configDir = core::getvncconfigdir();
    if (configDir == nullptr)
      throw std::runtime_error(_("Could not determine VNC config directory path"));

    snprintf(filepath, sizeof(filepath), "%s/default.tigervnc", configDir);
  } else {
    snprintf(filepath, sizeof(filepath), "%s", filename);
  }

  /* Read parameters from file */
  FILE* f = fopen(filepath, "r");
  if (!f) {
    if (!filename)
      return nullptr; // Use defaults.
    throw core::posix_error(
      core::format(_("Failed to open \"%s\""), filepath), errno);
  }

  int lineNr = 0;
  while (!feof(f)) {

    // Read the next line
    lineNr++;
    if (!fgets(line, sizeof(line), f)) {
      if (feof(f))
        break;

      fclose(f);
      throw core::posix_error(
        core::format(_("Failed to read line %d in file \"%s\""),
                     lineNr, filepath),
        errno);
    }

    if (strlen(line) == (sizeof(line) - 1)) {
      fclose(f);
      std::string msg = core::format(_("Failed to read line %d in "
                                       "file \"%s\""),
                                     lineNr, filepath);
      throw std::runtime_error(
        core::format("%s: %s", msg.c_str(), _("Line too long")));
    }

    // Make sure that the first line of the file has the file identifier string
    if(lineNr == 1) {
      if(strncmp(line, IDENTIFIER_STRING, strlen(IDENTIFIER_STRING)) == 0)
        continue;

      fclose(f);
      throw std::runtime_error(core::format(
        _("Configuration file %s is in an invalid format"), filepath));
    }

    // Skip empty lines and comments
    if ((line[0] == '\n') || (line[0] == '#') || (line[0] == '\r'))
      continue;

    int len = strlen(line);
    if (line[len-1] == '\n') {
      line[len-1] = '\0';
      len--;
    }
    if (line[len-1] == '\r') {
      line[len-1] = '\0';
      len--;
    }

    // Find the parameter value
    char *value = strchr(line, '=');
    if (value == nullptr) {
      std::string msg = core::format(_("Failed to read line %d in "
                                       "file \"%s\""),
                                     lineNr, filepath);
      vlog.error("%s: %s", msg.c_str(), _("Invalid format"));
      continue;
    }
    *value = '\0'; // line only contains the parameter name below.
    value++;
    
    bool invalidParameterName = true; // Will be set to false below if 
                                      // the line contains a valid name.

    try {
      if (strcasecmp(line, "ServerName") == 0) {

        if(!decodeValue(value, decodingBuffer, sizeof(decodingBuffer)))
          throw std::runtime_error(_("Invalid format or too large value"));
        snprintf(servername, sizeof(decodingBuffer), "%s", decodingBuffer);
        invalidParameterName = false;

      } else {
        invalidParameterName = findAndSetViewerParameterFromValue(
          parameterArray,
          sizeof(parameterArray) / sizeof(core::VoidParameter *),
          value, line);

        if (invalidParameterName) {
          invalidParameterName = findAndSetViewerParameterFromValue(
            readOnlyParameterArray,
            sizeof(readOnlyParameterArray) /
              sizeof(core::VoidParameter *),
            value, line);
        }
      }
    } catch(std::exception& e) {
      // Just ignore this entry and continue with the rest
      std::string msg = core::format(_("Failed to read line %d in "
                                       "file \"%s\""),
                                     lineNr, filepath);
      vlog.error("%s: %s", msg.c_str(), e.what());
      continue;
    }

    if (invalidParameterName) {
      std::string msg = core::format(_("Failed to read line %d in "
                                       "file \"%s\""),
                                     lineNr, filepath);
      vlog.error("%s: %s", msg.c_str(), _("Unknown parameter"));
    }
  }
  fclose(f);
  f = nullptr;

  migrateDeprecatedOptions();

  return servername;
}

void migrateDeprecatedOptions()
{
  if (fullScreenAllMonitors) {
    vlog.info(_("FullScreenAllMonitors is deprecated, set FullScreenMode to 'all' instead"));

    fullScreenMode.setParam("all");
  }
  if (dotWhenNoCursor) {
    vlog.info(_("DotWhenNoCursor is deprecated, set AlwaysCursor to 1 and CursorType to 'Dot' instead"));

    alwaysCursor.setParam(true);
    cursorType.setParam("Dot");
  }
}


#ifdef BUILD_PORTABLE_VIEWER
static std::string getPortableIniPath()
{
  const char* configDir = core::getvncconfigdir();
  if (configDir == nullptr)
    throw std::runtime_error(_("Could not determine VNC config directory path"));

#ifdef WIN32
  return std::string(configDir) + "\\vncviewer.ini";
#else
  return std::string(configDir) + "/vncviewer.ini";
#endif
}

#ifdef WIN32
static std::wstring portableWidePath(const std::string& path)
{
  int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                   path.c_str(), -1, nullptr, 0);
  if (length == 0)
    throw core::win32_error(
      core::format(_("Invalid UTF-8 path \"%s\""), path.c_str()),
      GetLastError());

  std::wstring widePath(length, L'\0');
  if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path.c_str(), -1,
                          &widePath[0], length) == 0)
    throw core::win32_error(
      core::format(_("Invalid UTF-8 path \"%s\""), path.c_str()),
      GetLastError());
  widePath.resize(length - 1);
  return widePath;
}
#endif

class PortableIniLock {
public:
  explicit PortableIniLock(const std::string& filepath)
#ifdef WIN32
    : handle(nullptr), acquired(false)
#else
    : fd(-1)
#endif
  {
#ifdef WIN32
    std::wstring widePath = portableWidePath(filepath);
    uint64_t hash = UINT64_C(1469598103934665603);
    for (wchar_t ch : widePath) {
      hash ^= static_cast<uint64_t>(ch);
      hash *= UINT64_C(1099511628211);
    }

    wchar_t mutexName[64];
    swprintf(mutexName, sizeof(mutexName) / sizeof(mutexName[0]),
             L"Local\\TigerVNC.PortableIni.%016llx",
             static_cast<unsigned long long>(hash));
    handle = CreateMutexW(nullptr, FALSE, mutexName);
    if (handle == nullptr)
      throw core::win32_error(_("Failed to create portable configuration lock"),
                              GetLastError());

    DWORD result = WaitForSingleObject(handle, INFINITE);
    if (result != WAIT_OBJECT_0 && result != WAIT_ABANDONED) {
      DWORD err = GetLastError();
      CloseHandle(handle);
      handle = nullptr;
      throw core::win32_error(_("Failed to lock portable configuration"), err);
    }
    acquired = true;
#else
    std::string lockpath = filepath + ".lock";
    fd = open(lockpath.c_str(), O_CREAT | O_RDWR, 0600);
    if (fd < 0)
      throw core::posix_error(
        core::format(_("Failed to open \"%s\""), lockpath.c_str()), errno);
    if (flock(fd, LOCK_EX) != 0) {
      int savedErrno = errno;
      close(fd);
      fd = -1;
      throw core::posix_error(_("Failed to lock portable configuration"),
                              savedErrno);
    }
#endif
  }

  ~PortableIniLock()
  {
#ifdef WIN32
    if (acquired)
      ReleaseMutex(handle);
    if (handle != nullptr)
      CloseHandle(handle);
#else
    if (fd >= 0) {
      flock(fd, LOCK_UN);
      close(fd);
    }
#endif
  }

private:
  PortableIniLock(const PortableIniLock&) = delete;
  PortableIniLock& operator=(const PortableIniLock&) = delete;

#ifdef WIN32
  HANDLE handle;
  bool acquired;
#else
  int fd;
#endif
};

static FILE* openPortableIni(const std::string& filepath, bool* notFound)
{
  *notFound = false;
#ifdef WIN32
  std::wstring widePath = portableWidePath(filepath);
  HANDLE handle = CreateFileW(widePath.c_str(), GENERIC_READ,
                              FILE_SHARE_READ | FILE_SHARE_WRITE |
                                FILE_SHARE_DELETE,
                              nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                              nullptr);
  if (handle == INVALID_HANDLE_VALUE) {
    DWORD err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND) {
      *notFound = true;
      return nullptr;
    }
    throw core::win32_error(
      core::format(_("Failed to open \"%s\""), filepath.c_str()), err);
  }

  int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle),
                           _O_RDONLY | _O_TEXT);
  if (fd < 0) {
    int savedErrno = errno;
    CloseHandle(handle);
    throw core::posix_error(
      core::format(_("Failed to open \"%s\""), filepath.c_str()),
      savedErrno);
  }
  FILE* file = _fdopen(fd, "r");
  if (file == nullptr) {
    int savedErrno = errno;
    _close(fd);
    throw core::posix_error(
      core::format(_("Failed to open \"%s\""), filepath.c_str()),
      savedErrno);
  }
  return file;
#else
  FILE* file = fopen(filepath.c_str(), "r");
  if (file == nullptr) {
    if (errno == ENOENT) {
      *notFound = true;
      return nullptr;
    }
    throw core::posix_error(
      core::format(_("Failed to open \"%s\""), filepath.c_str()), errno);
  }
  return file;
#endif
}

static FILE* createPortableTempFile(const std::string& filepath,
                                    std::string* tmppath)
{
#ifdef WIN32
  for (unsigned attempt = 0; attempt < 100; attempt++) {
    *tmppath = core::format("%s.tmp.%lu.%llu.%u", filepath.c_str(),
                            static_cast<unsigned long>(GetCurrentProcessId()),
                            static_cast<unsigned long long>(GetTickCount64()),
                            attempt);
    std::wstring widePath = portableWidePath(*tmppath);
    HANDLE handle = CreateFileW(widePath.c_str(), GENERIC_READ | GENERIC_WRITE,
                                0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL,
                                nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
      DWORD err = GetLastError();
      if (err == ERROR_FILE_EXISTS || err == ERROR_ALREADY_EXISTS)
        continue;
      throw core::win32_error(
        core::format(_("Failed to open \"%s\""), tmppath->c_str()), err);
    }

    int fd = _open_osfhandle(reinterpret_cast<intptr_t>(handle),
                             _O_RDWR | _O_TEXT);
    if (fd < 0) {
      int savedErrno = errno;
      CloseHandle(handle);
      DeleteFileW(widePath.c_str());
      throw core::posix_error(
        core::format(_("Failed to open \"%s\""), tmppath->c_str()),
        savedErrno);
    }
    FILE* file = _fdopen(fd, "w+");
    if (file == nullptr) {
      int savedErrno = errno;
      _close(fd);
      DeleteFileW(widePath.c_str());
      throw core::posix_error(
        core::format(_("Failed to open \"%s\""), tmppath->c_str()),
        savedErrno);
    }
    return file;
  }
  throw std::runtime_error(_("Failed to create a unique temporary file"));
#else
  std::string pattern = filepath + ".tmp.XXXXXX";
  int fd = mkstemp(&pattern[0]);
  if (fd < 0)
    throw core::posix_error(
      core::format(_("Failed to open \"%s\""), pattern.c_str()), errno);
  *tmppath = pattern;
  FILE* file = fdopen(fd, "w+");
  if (file == nullptr) {
    int savedErrno = errno;
    close(fd);
    remove(tmppath->c_str());
    throw core::posix_error(
      core::format(_("Failed to open \"%s\""), tmppath->c_str()),
      savedErrno);
  }
  return file;
#endif
}

static void removePortableFile(const std::string& filepath)
{
#ifdef WIN32
  try {
    std::wstring widePath = portableWidePath(filepath);
    DeleteFileW(widePath.c_str());
  } catch (std::exception&) {
  }
#else
  remove(filepath.c_str());
#endif
}

static void trimInPlace(char* s)
{
  char* end;
  size_t len;

  while (*s == ' ' || *s == '\t')
    memmove(s, s + 1, strlen(s));

  len = strlen(s);
  end = s + len;
  while (end > s &&
         (end[-1] == ' ' || end[-1] == '\t' ||
          end[-1] == '\r' || end[-1] == '\n')) {
    *--end = '\0';
  }
}

static void replacePortableIni(const std::string& tmppath,
                               const std::string& filepath)
{
#ifdef WIN32
  std::wstring wideTmp = portableWidePath(tmppath);
  std::wstring widePath = portableWidePath(filepath);
  if (!MoveFileExW(wideTmp.c_str(), widePath.c_str(),
                   MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    DWORD err = GetLastError();
    DeleteFileW(wideTmp.c_str());
    throw core::win32_error(
      core::format(_("Failed to replace \"%s\""), filepath.c_str()), err);
  }
#else
  if (rename(tmppath.c_str(), filepath.c_str()) != 0) {
    int savedErrno = errno;
    remove(tmppath.c_str());
    throw core::posix_error(
      core::format(_("Failed to replace \"%s\""), filepath.c_str()),
      savedErrno);
  }
#endif
}

static void writePortableIni(const char* servername,
                             const std::list<std::string>& history)
{
  const size_t buffersize = 256;
  char encodingBuffer[buffersize];
  std::string filepath = getPortableIniPath();
  std::string tmppath;
  FILE* f = createPortableTempFile(filepath, &tmppath);
  unsigned index = 0;

  fprintf(f, "; TigerVNC portable configuration\n");
  fprintf(f, "; Generated by vncviewer\n\n");
  fprintf(f, "[General]\n");
  fprintf(f, "Language=zh_CN\n\n");
  fprintf(f, "[Viewer]\n");

  if (!encodeValue(servername ? servername : "", encodingBuffer, buffersize)) {
    fclose(f);
    removePortableFile(tmppath);
    throw std::runtime_error(
      core::format(_("Failed to save \"%s\": %s"), "ServerName",
                   _("Could not encode parameter")));
  }
  fprintf(f, "ServerName=%s\n", encodingBuffer);

  for (core::VoidParameter* param : parameterArray) {
    if (param->isDefault())
      continue;
    if (!encodeValue(param->getValueStr().c_str(),
                     encodingBuffer, buffersize)) {
      fclose(f);
      removePortableFile(tmppath);
      throw std::runtime_error(
        core::format(_("Failed to save \"%s\": %s"), param->getName(),
                     _("Could not encode parameter")));
    }
    fprintf(f, "%s=%s\n", param->getName(), encodingBuffer);
  }

  fprintf(f, "\n[History]\n");
  for (const std::string& entry : history) {
    if (index >= SERVER_HISTORY_SIZE)
      break;
    if (!encodeValue(entry.c_str(), encodingBuffer, buffersize)) {
      fclose(f);
      removePortableFile(tmppath);
      throw std::runtime_error(
        core::format(_("Failed to save \"%s\": %s"), "History",
                     _("Could not encode parameter")));
    }
    fprintf(f, "%u=%s\n", index, encodingBuffer);
    index++;
  }

  if (ferror(f)) {
    int savedErrno = errno;
    fclose(f);
    removePortableFile(tmppath);
    throw core::posix_error(
      core::format(_("Failed to write \"%s\""), tmppath.c_str()),
      savedErrno);
  }
  if (fclose(f) != 0) {
    int savedErrno = errno;
    removePortableFile(tmppath);
    throw core::posix_error(
      core::format(_("Failed to write \"%s\""), tmppath.c_str()),
      savedErrno);
  }

  replacePortableIni(tmppath, filepath);
}

static bool parsePortableIni(PortableConfig* config,
                             ApplyPortableParameters applyParameters)
{
  const size_t buffersize = 256;
  char line[buffersize];
  char decodingBuffer[buffersize];
  std::string filepath = getPortableIniPath();
  bool notFound;
  FILE* f;
  enum { SEC_NONE, SEC_VIEWER, SEC_HISTORY } section = SEC_NONE;
  int lineNr = 0;

  config->serverName.clear();
  config->history.clear();
  f = openPortableIni(filepath, &notFound);
  if (notFound)
    return false;

  while (!feof(f)) {
    char *value;
    char *eq;

    lineNr++;
    if (!fgets(line, sizeof(line), f)) {
      if (feof(f))
        break;
      int savedErrno = errno;
      fclose(f);
      throw core::posix_error(
        core::format(_("Failed to read line %d in file \"%s\""),
                     lineNr, filepath.c_str()),
        savedErrno);
    }

    if (strlen(line) == (sizeof(line) - 1)) {
      fclose(f);
      std::string msg = core::format(_("Failed to read line %d in "
                                       "file \"%s\""),
                                     lineNr, filepath.c_str());
      throw std::runtime_error(
        core::format("%s: %s", msg.c_str(), _("Line too long")));
    }

    trimInPlace(line);
    if (line[0] == '\0' || line[0] == '#' || line[0] == ';')
      continue;

    if (line[0] == '[') {
      char* end = strchr(line, ']');
      if (end) {
        *end = '\0';
        if (strcasecmp(line + 1, "Viewer") == 0)
          section = SEC_VIEWER;
        else if (strcasecmp(line + 1, "History") == 0)
          section = SEC_HISTORY;
        else
          section = SEC_NONE;
      }
      continue;
    }

    eq = strchr(line, '=');
    if (eq == nullptr)
      continue;
    *eq = '\0';
    value = eq + 1;
    trimInPlace(line);
    trimInPlace(value);

    try {
      if (section == SEC_VIEWER) {
        if (strcasecmp(line, "ServerName") == 0) {
          if (!decodeValue(value, decodingBuffer, sizeof(decodingBuffer)))
            throw std::runtime_error(_("Invalid format or too large value"));
          config->serverName = decodingBuffer;
        } else if (applyParameters == ApplyPortableParameters::Yes) {
          bool invalidParameterName = findAndSetViewerParameterFromValue(
            parameterArray,
            sizeof(parameterArray) / sizeof(core::VoidParameter *),
            value, line);
          if (invalidParameterName) {
            invalidParameterName = findAndSetViewerParameterFromValue(
              readOnlyParameterArray,
              sizeof(readOnlyParameterArray) /
                sizeof(core::VoidParameter *),
              value, line);
          }
          if (invalidParameterName)
            vlog.error(_("Failed to read parameter \"%s\": %s"),
                       line, _("Unknown parameter"));
        }
      } else if (section == SEC_HISTORY) {
        if (!decodeValue(value, decodingBuffer, sizeof(decodingBuffer)))
          throw std::runtime_error(_("Invalid format or too large value"));
        if (decodingBuffer[0] != '\0' &&
            config->history.size() < SERVER_HISTORY_SIZE)
          config->history.push_back(decodingBuffer);
      }
    } catch (std::exception& e) {
      std::string msg = core::format(_("Failed to read line %d in "
                                       "file \"%s\""),
                                     lineNr, filepath.c_str());
      if (applyParameters == ApplyPortableParameters::No) {
        fclose(f);
        throw std::runtime_error(
          core::format("%s: %s", msg.c_str(), e.what()));
      }
      vlog.error("%s: %s", msg.c_str(), e.what());
    }
  }

  if (fclose(f) != 0)
    throw core::posix_error(
      core::format(_("Failed to close \"%s\""), filepath.c_str()), errno);
  if (applyParameters == ApplyPortableParameters::Yes)
    migrateDeprecatedOptions();
  return true;
}

static void savePortableConfig(const char* servername,
                               const std::list<std::string>* historyIn)
{
  std::string filepath = getPortableIniPath();
  PortableIniLock lock(filepath);
  PortableConfig config;

  if (historyIn != nullptr)
    config.history = *historyIn;
  else
    parsePortableIni(&config, ApplyPortableParameters::No);

  writePortableIni(servername ? servername : "", config.history);
}

static void savePortableHistory(const std::list<std::string>& history)
{
  std::string filepath = getPortableIniPath();
  PortableIniLock lock(filepath);
  PortableConfig config;

  parsePortableIni(&config, ApplyPortableParameters::No);
  writePortableIni(config.serverName.c_str(), history);
}

void savePortableViewerState(const char* servername,
                             const std::list<std::string>& serverHistory)
{
  std::string filepath = getPortableIniPath();
  PortableIniLock lock(filepath);
  writePortableIni(servername ? servername : "", serverHistory);
}
#endif // BUILD_PORTABLE_VIEWER

std::list<std::string> loadServerHistory()
{
#ifdef BUILD_PORTABLE_VIEWER
  PortableConfig config;
  parsePortableIni(&config, ApplyPortableParameters::No);
  return config.history;
#elif defined(_WIN32)
  return loadHistoryFromRegKey();
#else
  std::list<std::string> serverHistory;
  const char* stateDir = core::getvncstatedir();
  char filepath[PATH_MAX];
  FILE* f;
  int lineNr = 0;

  if (stateDir == nullptr)
    throw std::runtime_error(_("Could not determine VNC state directory path"));

  snprintf(filepath, sizeof(filepath), "%s/%s", stateDir, "tigervnc.history");
  f = fopen(filepath, "r");
  if (!f) {
    if (errno == ENOENT)
      return serverHistory;
    throw core::posix_error(
      core::format(_("Failed to open \"%s\""), filepath), errno);
  }

  while (!feof(f)) {
    char line[256];
    int len;

    lineNr++;
    if (!fgets(line, sizeof(line), f)) {
      if (feof(f))
        break;
      fclose(f);
      throw core::posix_error(
        core::format(_("Failed to read line %d in file \"%s\""),
                     lineNr, filepath),
        errno);
    }

    len = strlen(line);
    if (len == (sizeof(line) - 1)) {
      fclose(f);
      std::string msg = core::format(_("Failed to read line %d in "
                                       "file \"%s\""),
                                     lineNr, filepath);
      throw std::runtime_error(
        core::format("%s: %s", msg.c_str(), _("Line too long")));
    }
    if ((len > 0) && (line[len-1] == '\n')) {
      line[len-1] = '\0';
      len--;
    }
    if ((len > 0) && (line[len-1] == '\r')) {
      line[len-1] = '\0';
      len--;
    }
    if (len == 0)
      continue;
    serverHistory.push_back(line);
  }

  fclose(f);
  return serverHistory;
#endif
}

void saveServerHistory(const std::list<std::string>& serverHistory)
{
#ifdef BUILD_PORTABLE_VIEWER
  savePortableHistory(serverHistory);
#elif defined(_WIN32)
  saveHistoryToRegKey(serverHistory);
#else
  const char* stateDir = core::getvncstatedir();
  char filepath[PATH_MAX];
  FILE* f;
  size_t count = 0;

  if (stateDir == nullptr)
    throw std::runtime_error(_("Could not determine VNC state directory path"));

  snprintf(filepath, sizeof(filepath), "%s/%s", stateDir, "tigervnc.history");
  f = fopen(filepath, "w+");
  if (!f) {
    std::string msg = core::format(_("Failed to open \"%s\""), filepath);
    throw core::posix_error(msg.c_str(), errno);
  }

  for (const std::string& entry : serverHistory) {
    if (++count > SERVER_HISTORY_SIZE)
      break;
    fprintf(f, "%s\n", entry.c_str());
  }

  fclose(f);
#endif
}
