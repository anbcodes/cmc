#pragma once

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))

#define LOG_LEVEL 1

#define TRACE_NN(...) { \
  printf("TRACE [%s:%d] ", __FILE__, __LINE__); printf(__VA_ARGS__); \
}

#define DEBUG_NN(...) { \
  printf("DEBUG [%s:%d] ", __FILE__, __LINE__); printf(__VA_ARGS__); \
}

#define INFO_NN(...) { \
  printf("INFO  [%s:%d] ", __FILE__, __LINE__); printf(__VA_ARGS__); \
}

#define WARN_NN(...) { \
  printf("WARN  [%s:%d] ", __FILE__, __LINE__); printf(__VA_ARGS__); \
}

#define ERROR_NN(...) { \
  printf("ERROR [%s:%d] ", __FILE__, __LINE__); printf(__VA_ARGS__); \
}

#define FATAL_NN(...) { \
  printf("FATAL [%s:%d] ", __FILE__, __LINE__); printf(__VA_ARGS__); \
}

#define TRACE(...) { \
  TRACE_NN(__VA_ARGS__); printf("\n"); \
}

#define DEBUG(...) { \
  DEBUG_NN(__VA_ARGS__); printf("\n"); \
}

#define INFO(...) { \
  INFO_NN(__VA_ARGS__); printf("\n"); \
}

#define WARN(...) { \
  WARN_NN(__VA_ARGS__); printf("\n"); \
}

#define ERROR(...) { \
  ERROR_NN(__VA_ARGS__); printf("\n"); \
}

#define FATAL(...) { \
  FATAL_NN(__VA_ARGS__); printf("\n"); \
}


#if LOG_LEVEL > 0
#undef TRACE
#undef TRACE_NN
#define TRACE(...) {}
#define TRACE_NN(...) {}
#endif

#if LOG_LEVEL > 1
#undef DEBUG
#undef DEBUG_NN
#define DEBUG(...) {}
#define DEBUG_NN(...) {}
#endif

#if LOG_LEVEL > 2
#undef INFO
#undef INFO_NN
#define INFO(...) {}
#define INFO_NN(...) {}
#endif

#if LOG_LEVEL > 3
#undef WARN
#undef WARN_NN
#define WARN(...) {}
#define WARN_NN(...) {}
#endif

#if LOG_LEVEL > 4
#undef ERROR
#undef ERROR_NN
#define ERROR(...) {}
#define ERROR_NN(...) {}
#endif

#if LOG_LEVEL > 5
#undef FATAL
#undef FATAL_NN
#define FATAL(...) {}
#define FATAL_NN(...) {}
#endif
