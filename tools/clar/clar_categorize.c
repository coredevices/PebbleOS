/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#define CLAR_CATEGORY_DEFAULT "default"

typedef struct {
	const char **names;
	int count;
	int alloc;
} clar_category_list;

static clar_category_list _clar_categorize_enabled;

static int clar_category_in_list(clar_category_list *list, const char *cat)
{
	int i;
	for (i = 0; i < list->count; ++i)
		if (strcasecmp(cat, list->names[i]) == 0)
			return 1;
	return 0;
}

static void clar_category_add_to_list(clar_category_list *list, const char *cat)
{
	if (clar_category_in_list(list, cat))
		return;

	if (list->count >= list->alloc) {
		list->alloc += 10;
		list->names = realloc(list->names, list->alloc * sizeof(const char *));
	}

	list->names[list->count++] = cat;
}

static void clar_category_enable(const char *category)
{
	clar_category_add_to_list(&_clar_categorize_enabled, category);
}

static void clar_category_enable_all(size_t suite_count, const struct clar_suite *suites)
{
	size_t i;
	const char **cat;

	clar_category_enable(CLAR_CATEGORY_DEFAULT);

	for (i = 0; i < suite_count; i++)
		for (cat = suites[i].categories; cat && *cat; cat++)
			clar_category_enable(*cat);
}

static int clar_category_cmp(const void *a, const void *b)
{
	return - strcasecmp(a,b);
}

static void clar_category_print_enabled(const char *prefix)
{
	int i;

	qsort(_clar_categorize_enabled.names, _clar_categorize_enabled.count,
		sizeof(const char *), clar_category_cmp);

	for (i = 0; i < _clar_categorize_enabled.count; ++i)
		printf("%s%s\n", prefix, _clar_categorize_enabled.names[i]);
}

static int clar_category_is_suite_enabled(const struct clar_suite *suite)
{
	const char **scan;

	if (!_clar_categorize_enabled.count)
		clar_category_enable(CLAR_CATEGORY_DEFAULT);

	if (!suite->categories)
		return clar_category_in_list(
			&_clar_categorize_enabled, CLAR_CATEGORY_DEFAULT);

	for (scan = suite->categories; *scan != NULL; scan++)
		if (clar_category_in_list(&_clar_categorize_enabled, *scan))
			return 1;

	return 0;
}
