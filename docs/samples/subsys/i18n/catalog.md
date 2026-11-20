# Internationalization: Translations

This sample translates strings with a French gettext catalog, using
{doc}`internationalization <../../../subsystems/i18n>`. The build compiles
`po/fr_FR.po` with `msgfmt`, and `#embed` puts the result in the image; a
catalog is read through a callback, so it could as well be in flash or a
file.

Strings are looked up by their English original, and those without a
translation stay in English, as all do without a catalog. A context tells
apart originals that are the same word: the noun and the verb *Watch*.
`pbl_i18n_get()` caches translations for an owner until it frees them;
`pbl_i18n_get_with_buffer()` copies one into a buffer instead, truncated to
fit.

```{pebbleos-sample}
:path: samples/subsys/i18n/catalog
:sources: po/fr_FR.po src/main.c CMakeLists.txt
:output:
```
