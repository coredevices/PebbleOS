Doxygen pro tips
---

- Define top-level groups and other doxygen constructs in `docs/common.dox`.
- The main page can be found in `docs/mainpage_sdk.dox`
- Use \ref to create a cross reference in the documentation to another group, function, struct, or any kind of symbol, for example: `Use \ref app_event_loop() to do awesome shit.` will create a clickable link to the documentation of app_event_loop. Don't forget to add the () parens if the symbol is a function! Using angle brackets like <app_event_loop()> doesn't seem to work reliably, nor does the automatic detection of symbols.
- Use the directive `@internal` to indicate that a piece of documentation is internal and should not be included in the public SDK documentation. You can add the @internal halfway, so that the first part of your docstrings will be included in the public SDK documentation, and the part after the @internal directive will also get included in our internal docs.
- Use `@param param_name Description of param` to document a function parameter.
- Use `@return Description of return value` to document the return value of a function.
- If you need to add a file or directory that doxygen should index, add its path to `INPUT` in the Doxyfile-SDK configuration file.
- If you want to make a cross-reference to an external doc page (the conceptual pages on developer.getpebble.com), create an .html file in /docs/external_refs, containing only a <a> link to the page. Then use `\htmlinclude my_ref.html` to include that link in the docs. This extra level of indirection will make it easy to relocate external pages later on.
