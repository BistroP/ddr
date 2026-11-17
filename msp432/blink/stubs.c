// Empty C-runtime hooks. Our own startup file is used (-nostartfiles), and newlib's
// __libc_init_array() still calls _init()/_fini(), so provide do-nothing versions.
void _init(void) {}
void _fini(void) {}
