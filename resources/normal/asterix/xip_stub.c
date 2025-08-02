/* these must be static and const -- otherwise they may go out to the GOT, which does not exist! */
static const char *s1 = "Hello world";
static const char *s2 = "from a -fPIC XIP stub";

int _entry(void (*print_callback)(const char *)) {
    print_callback(s1);
    print_callback(s2);
    return 42;
}
