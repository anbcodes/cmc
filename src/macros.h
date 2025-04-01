#pragma once

#define DEBUG(...) { printf("[%s:%d] ", __FILE__, __LINE__); printf(__VA_ARGS__); }
