#pragma once
#include <stdint.h>

// Run a simple walking-ones/zeros memory test over [start, length) in bytes.
// Returns number of errors detected (0 = pass). Non-destructive by default if destructive==0.
// If destructive!=0, contents will not be restored.
uint64_t memtest_run(uint64_t start_phys, uint64_t length_bytes, int destructive);

// Convenience: test a limited safe range (e.g., 16 MiB starting past 1 MiB)
uint64_t memtest_quick(void);
