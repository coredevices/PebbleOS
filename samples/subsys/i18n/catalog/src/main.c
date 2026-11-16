/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <pbl/i18n/i18n.h>

/* A gettext MO file, compiled from po/fr_FR.po at build time. */
static const uint8_t s_fr_mo[] = {
#embed "fr_FR.mo"
};

/* Catalogs are read through a callback, so they can live in flash, a file or memory. */
static int prv_read(void *data, uint32_t offset, void *buf, size_t length) {
  if (offset > sizeof(s_fr_mo) || length > sizeof(s_fr_mo) - offset) {
    return -EINVAL;
  }

  memcpy(buf, &s_fr_mo[offset], length);

  return 0;
}

static const struct pbl_i18n_catalog s_catalog = {
  .read = prv_read,
  .size = sizeof(s_fr_mo),
};

/* Translations from pbl_i18n_get() are cached per owner, here this file, until freed. */
static const int s_owner;

static void prv_print(const char *when) {
  printf("i18n: %s: %s / %s / %s / %s\n", when, pbl_i18n_get("Hello", &s_owner),
         pbl_i18n_ctx_get("noun", "Watch", &s_owner), pbl_i18n_ctx_get("verb", "Watch", &s_owner),
         pbl_i18n_get("Good night", &s_owner));
  pbl_i18n_free_all(&s_owner);
}

int main(void) {
  struct pbl_i18n_catalog_info info;
  char buf[16];
  int ret;

  prv_print("no catalog");

  ret = pbl_i18n_catalog_get_info(&s_catalog, &info);
  if (ret < 0) {
    fprintf(stderr, "i18n: not a catalog (%d)\n", ret);
    return ret;
  }
  printf("i18n: catalog %s, %s, version %u\n", info.locale, info.name, info.version);

  ret = pbl_i18n_set_catalog(&s_catalog);
  if (ret < 0) {
    fprintf(stderr, "i18n: catalog not set (%d)\n", ret);
    return ret;
  }
  printf("i18n: locale is now %s\n", pbl_i18n_get_locale());

  /* Strings without a translation stay in English. */
  prv_print("French catalog");

  /* No caching: the translation is copied into a buffer, truncated to fit. */
  pbl_i18n_get_with_buffer("Your watch is charging", buf, sizeof(buf));
  printf("i18n: %u characters, in %u bytes: %s\n",
         (unsigned int)pbl_i18n_get_length("Your watch is charging"), (unsigned int)sizeof(buf),
         buf);

  (void)pbl_i18n_set_catalog(nullptr);
  prv_print("catalog removed");

  printf("Sample done\n");

  return 0;
}
