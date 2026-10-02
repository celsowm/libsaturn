/* Must NOT link cleanly: g_value needs dynamic initialization, which the
 * Saturn startup code never performs. */
extern "C" int libsaturn_ctor_trap_value(void);

static int g_value = libsaturn_ctor_trap_value();

extern "C" int libsaturn_ctor_trap_value(void) {
    static volatile int source = 7;
    return source;
}

extern "C" int main(void) {
    return g_value;
}
