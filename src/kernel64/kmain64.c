// Minimal 64-bit C file to keep toolchain happy and as a placeholder.
#include <stdint.h>

__attribute__((used)) static void dummy_use(uint64_t x) { (void)x; }

void kmain64_dummy(void) {
    dummy_use(42);
}
