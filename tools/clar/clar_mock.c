/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

typedef struct MockListNode {
  struct MockListNode *next;
  struct MockListNode *prev;
  const char *func;
  uintmax_t value;
  ssize_t count;
} MockListNode;

static MockListNode s_mock_list_head = {
  &s_mock_list_head, &s_mock_list_head,
  NULL, 0, -1,
};

uintmax_t clar__mock(const char *const func, const char *const file, const size_t line)
{
  MockListNode *node;
  // Walk the list backwards, for FIFO behavior.
  for (node = s_mock_list_head.prev; node != &s_mock_list_head; node = node->prev) {
    if (node->func == NULL) {
      continue;
    } else if (strcmp(node->func, func) == 0) {
      break;
    }
  }
  char error_msg[128];
  snprintf(error_msg, sizeof(error_msg), "No more mock values available for '%s'!", func);
  cl_assert_(node != &s_mock_list_head, error_msg);

	// Save the value.
  uintmax_t value = node->value;
  cl_assert_(node->count != 0, "Mock node count is invalid");
  // If the node isn't permanent, lower its counter.
  // If the result is zero, we need to remove this mock value.
  if (node->count > 0 && --node->count == 0) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
    free(node);
  }
  return value;
}

void clar__will_return(const char *const func, const char *const file, const size_t line,
                       const uintmax_t value, const ssize_t count)
{
  MockListNode *node = calloc(1, sizeof(MockListNode));
  cl_assert_(func != NULL, "cl_will_return with invalid function name");
  node->func = func;
  node->value = value;
  node->count = count;

  // Add to beginning of list
  node->next = s_mock_list_head.next;
  node->prev = &s_mock_list_head;
  s_mock_list_head.next = node;
  // Fixup the next entry.
  // In the case that the list was empty before this call, this will make
  // s_mock_list_head.prev point to the new node, which is what we want.
  cl_assert_(node->next != NULL, "Mock list corrupted!");
  node->next->prev = node;
}

static void clar_mock_reset(void)
{
	s_mock_list_head.next = &s_mock_list_head;
	s_mock_list_head.prev = &s_mock_list_head;
	s_mock_list_head.func = NULL;
	s_mock_list_head.value = 0;
	s_mock_list_head.count = -1;
}

static void clar_mock_cleanup(void)
{
  MockListNode *node, *next;
  for (node = s_mock_list_head.next; node != &s_mock_list_head; node = next) {
    next = node->next;
    free(node);
  }
  clar_mock_reset();
}

// Who tests the test framework!
#if 0
int gack(void)
{
  return cl_mock_type(int);
}

int main(int argc, const char *argv[])
{
  cl_will_return(gack, 573);
  printf("gack() = %d\n", gack()); // 573
  cl_will_return(gack, 123);
  cl_will_return(gack, 456);
  cl_will_return(gack, 789);
  printf("gack() = %d\n", gack()); // 123
  printf("gack() = %d\n", gack()); // 456
  printf("gack() = %d\n", gack()); // 789
  cl_will_return_count(gack, 765, 3);
  printf("gack() = %d\n", gack()); // 765
  printf("gack() = %d\n", gack()); // 765
  printf("gack() = %d\n", gack()); // 765

  printf("gack() = %d\n", gack());
  return 0;
}
#endif
