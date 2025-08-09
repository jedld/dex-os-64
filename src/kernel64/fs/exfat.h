#pragma once

// Minimal formatting hook for exFAT. Implemented in exfat.c
int exfat_format_device(const char* dev_name, const char* label_opt);
