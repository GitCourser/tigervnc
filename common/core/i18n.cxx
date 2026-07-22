/* Copyright (C) 2002-2005 RealVNC Ltd.  All Rights Reserved.
 * Copyright 2011-2023 Pierre Ossman <ossman@cendio.se> for Cendio AB
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

#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef WIN32
#include <windows.h>
#ifdef BUILD_PORTABLE_VIEWER
#include "../../vncviewer/resource.h"
#endif
#endif

#ifdef __APPLE__
#include <Carbon/Carbon.h>
#endif

#include <core/i18n.h>

// Restore original functions

#undef dgettext
#undef dcgettext
#undef dngettext
#undef dcngettext
#undef pgettext_aux
#undef npgettext_aux

static bool initialized = false;

#if defined(WIN32) && defined(BUILD_PORTABLE_VIEWER)
struct PortableTranslation {
  const char* text;
  size_t length;
  bool found;
};

static const void* getPortableCatalog(size_t* catalogSize)
{
  HRSRC resource;
  HGLOBAL loadedResource;
  DWORD resourceSize;
  const void* resourceData;

  resource = FindResourceW(nullptr, MAKEINTRESOURCEW(IDR_PORTABLE_ZH_CN_MO),
                           MAKEINTRESOURCEW(10));
  if (resource == nullptr)
    return nullptr;

  resourceSize = SizeofResource(nullptr, resource);
  if (resourceSize == 0)
    return nullptr;

  loadedResource = LoadResource(nullptr, resource);
  if (loadedResource == nullptr)
    return nullptr;

  resourceData = LockResource(loadedResource);
  if (resourceData == nullptr)
    return nullptr;

  *catalogSize = resourceSize;
  return resourceData;
}

static uint32_t readCatalogUInt32(const unsigned char* data, bool swapped)
{
  if (swapped) {
    return (static_cast<uint32_t>(data[0]) << 24) |
           (static_cast<uint32_t>(data[1]) << 16) |
           (static_cast<uint32_t>(data[2]) << 8) |
           static_cast<uint32_t>(data[3]);
  }

  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

static PortableTranslation portableLookup(const char* msgid,
                                           size_t msgidLength)
{
  const unsigned char* catalog;
  size_t catalogSize;
  bool swapped;
  uint32_t stringCount;
  uint32_t originalTableOffset;
  uint32_t translationTableOffset;
  uint32_t low;
  uint32_t high;
  PortableTranslation notFound = {msgid, msgidLength, false};

  catalog = static_cast<const unsigned char*>(getPortableCatalog(&catalogSize));
  if (catalog == nullptr || catalogSize < 28)
    return notFound;

  if (readCatalogUInt32(catalog, false) == UINT32_C(0x950412de))
    swapped = false;
  else if (readCatalogUInt32(catalog, false) == UINT32_C(0xde120495))
    swapped = true;
  else
    return notFound;

  if ((readCatalogUInt32(catalog + 4, swapped) >> 16) != 0)
    return notFound;

  stringCount = readCatalogUInt32(catalog + 8, swapped);
  originalTableOffset = readCatalogUInt32(catalog + 12, swapped);
  translationTableOffset = readCatalogUInt32(catalog + 16, swapped);
  if (originalTableOffset > catalogSize ||
      translationTableOffset > catalogSize ||
      stringCount > (catalogSize - originalTableOffset) / 8 ||
      stringCount > (catalogSize - translationTableOffset) / 8)
    return notFound;

  low = 0;
  high = stringCount;
  while (low < high) {
    uint32_t middle = low + (high - low) / 2;
    const unsigned char* originalEntry =
      catalog + originalTableOffset + middle * 8;
    uint32_t originalLength = readCatalogUInt32(originalEntry, swapped);
    uint32_t originalOffset = readCatalogUInt32(originalEntry + 4, swapped);
    const char* original;
    size_t commonLength;
    int comparison;

    if (originalOffset > catalogSize ||
        originalLength >= catalogSize - originalOffset ||
        catalog[originalOffset + originalLength] != '\0')
      return notFound;

    original = reinterpret_cast<const char*>(catalog + originalOffset);
    commonLength = msgidLength < originalLength ? msgidLength : originalLength;
    comparison = memcmp(msgid, original, commonLength);
    if (comparison == 0) {
      if (msgidLength < originalLength)
        comparison = -1;
      else if (msgidLength > originalLength)
        comparison = 1;
    }

    if (comparison < 0) {
      high = middle;
    } else if (comparison > 0) {
      low = middle + 1;
    } else {
      const unsigned char* translationEntry =
        catalog + translationTableOffset + middle * 8;
      uint32_t translationLength =
        readCatalogUInt32(translationEntry, swapped);
      uint32_t translationOffset =
        readCatalogUInt32(translationEntry + 4, swapped);
      PortableTranslation translation;

      if (translationOffset > catalogSize ||
          translationLength >= catalogSize - translationOffset ||
          translationLength == 0 ||
          catalog[translationOffset + translationLength] != '\0')
        return notFound;

      translation.text =
        reinterpret_cast<const char*>(catalog + translationOffset);
      translation.length = translationLength;
      translation.found = true;
      return translation;
    }
  }

  return notFound;
}

static const char* portableLookup(const char* msgid)
{
  PortableTranslation translation = portableLookup(msgid, strlen(msgid));
  return translation.found ? translation.text : msgid;
}

static const char* portableLookupPlural(const char* lookupSingular,
                                        const char* msgid,
                                        const char* msgidPlural,
                                        unsigned long int n)
{
  size_t singularLength = strlen(lookupSingular);
  size_t pluralLength = strlen(msgidPlural);
  size_t lookupLength;
  char* lookupKey;
  PortableTranslation translation;

  if (singularLength > SIZE_MAX - pluralLength - 1)
    return n == 1 ? msgid : msgidPlural;

  lookupLength = singularLength + 1 + pluralLength;
  lookupKey = new char[lookupLength];
  memcpy(lookupKey, lookupSingular, singularLength + 1);
  memcpy(lookupKey + singularLength + 1, msgidPlural, pluralLength);
  translation = portableLookup(lookupKey, lookupLength);
  delete [] lookupKey;

  if (!translation.found)
    return n == 1 ? msgid : msgidPlural;

  /* The embedded catalog is fixed to zh_CN, whose Plural-Forms has one form. */
  return translation.text;
}
#endif

#ifndef BUILD_PORTABLE_VIEWER
static const char* getlocaledir()
{
#if defined(WIN32)
  static char localebuf[PATH_MAX];
  char *slash;

  GetModuleFileName(nullptr, localebuf, sizeof(localebuf));

  slash = strrchr(localebuf, '\\');
  if (slash == nullptr)
    return nullptr;

  *slash = '\0';

  if ((strlen(localebuf) + strlen("\\locale")) >= sizeof(localebuf))
    return nullptr;

  strcat(localebuf, "\\locale");

  return localebuf;
#elif defined(__APPLE__)
  CFBundleRef bundle;
  CFURLRef localeurl;
  CFStringRef localestr;
  Boolean ret;

  static char localebuf[PATH_MAX];

  bundle = CFBundleGetMainBundle();
  if (bundle == nullptr)
    return nullptr;

  localeurl = CFBundleCopyResourceURL(bundle, CFSTR("locale"),
                                      nullptr, nullptr);
  if (localeurl == nullptr)
    return nullptr;

  localestr = CFURLCopyFileSystemPath(localeurl, kCFURLPOSIXPathStyle);

  CFRelease(localeurl);

  ret = CFStringGetCString(localestr, localebuf, sizeof(localebuf),
                           kCFStringEncodingUTF8);
  if (!ret)
    return nullptr;

  return localebuf;
#else
  return CMAKE_INSTALL_FULL_LOCALEDIR;
#endif
}
#endif

static void initTranslations()
{
#ifndef BUILD_PORTABLE_VIEWER
  const char* localedir;
  const char* curlocale;
#endif

  if (initialized)
    return;

  // Set first to prevent recursion
  initialized = true;

#ifdef BUILD_PORTABLE_VIEWER
  /* Portable Chinese build: force Simplified Chinese regardless of OS locale. */
  setlocale(LC_MESSAGES, "zh_CN.UTF-8");
  if (setlocale(LC_MESSAGES, nullptr) == nullptr ||
      strcmp(setlocale(LC_MESSAGES, nullptr), "C") == 0)
    setlocale(LC_MESSAGES, "zh_CN");
#ifdef WIN32
  SetEnvironmentVariableA("LANGUAGE", "zh_CN");
  SetEnvironmentVariableA("LC_ALL", "zh_CN.UTF-8");
  SetEnvironmentVariableA("LANG", "zh_CN.UTF-8");
#else
  setenv("LANGUAGE", "zh_CN", 1);
  setenv("LC_ALL", "zh_CN.UTF-8", 1);
  setenv("LANG", "zh_CN.UTF-8", 1);
#endif
#else
  localedir = getlocaledir();
  if (localedir == nullptr) {
    fprintf(stderr, "Failed to determine locale directory\n");
    return;
  }

  // We translate things in global object constructors, which means that
  // we need to figure out the locale before main() has had a chance to
  // run setlocale()
  curlocale = setlocale(LC_MESSAGES, NULL);
  if (strcmp(curlocale, "C") == 0)
    setlocale(LC_MESSAGES, "");

  bindtextdomain(DEFAULT_TEXT_DOMAIN, localedir);

  // Set gettext codeset to what our GUI toolkit uses. Since we are
  // passing strings from strerror/gai_strerror to the GUI, these must
  // be in GUI codeset as well.
  bind_textdomain_codeset(DEFAULT_TEXT_DOMAIN, "UTF-8");
  bind_textdomain_codeset("libc", "UTF-8");
#endif
}

const char *dgettext_rfb(const char *domainname, const char *msgid)
{
  initTranslations();
#if defined(WIN32) && defined(BUILD_PORTABLE_VIEWER)
  (void)domainname;
  return portableLookup(msgid);
#else
  return dgettext(domainname, msgid);
#endif
}

const char *dcgettext_rfb(const char *domainname, const char *msgid,
                          int category)
{
  initTranslations();
#if defined(WIN32) && defined(BUILD_PORTABLE_VIEWER)
  (void)domainname;
  (void)category;
  return portableLookup(msgid);
#else
  return dcgettext(domainname, msgid, category);
#endif
}

const char *dngettext_rfb(const char *domainname, const char *msgid,
                          const char *msgid_plural,
                          unsigned long int n)
{
  initTranslations();
#if defined(WIN32) && defined(BUILD_PORTABLE_VIEWER)
  (void)domainname;
  return portableLookupPlural(msgid, msgid, msgid_plural, n);
#else
  return dngettext(domainname, msgid, msgid_plural, n);
#endif
}

const char *dcngettext_rfb(const char *domainname, const char *msgid,
                           const char *msgid_plural,
                           unsigned long int n, int category)
{
  initTranslations();
#if defined(WIN32) && defined(BUILD_PORTABLE_VIEWER)
  (void)domainname;
  (void)category;
  return portableLookupPlural(msgid, msgid, msgid_plural, n);
#else
  return dcngettext(domainname, msgid, msgid_plural, n, category);
#endif
}

const char *pgettext_rfb(const char *domain,
                         const char *msg_ctxt_id, const char *msgid,
                         int category)
{
  initTranslations();
#if defined(WIN32) && defined(BUILD_PORTABLE_VIEWER)
  const char* translation;
  (void)domain;
  (void)category;
  translation = portableLookup(msg_ctxt_id);
  return translation == msg_ctxt_id ? msgid : translation;
#else
  return pgettext_aux(domain, msg_ctxt_id, msgid, category);
#endif
}

const char *npgettext_rfb(const char *domain,
                          const char *msg_ctxt_id, const char *msgid,
                          const char *msgid_plural,
                          unsigned long int n, int category)
{
  initTranslations();
#if defined(WIN32) && defined(BUILD_PORTABLE_VIEWER)
  (void)domain;
  (void)category;
  return portableLookupPlural(msg_ctxt_id, msgid, msgid_plural, n);
#else
  return npgettext_aux(domain, msg_ctxt_id, msgid, msgid_plural,
                       n, category);
#endif
}
