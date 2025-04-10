#include "logging.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <wchar.h>
#include <wctype.h>
#include "datatypes.h"

void aprintf(const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  int len = strlen(fmt);

  bool isformat = false;
  char cur_format[20] = {0};
  int j = 0;
  for (int i = 0; i < len; i++) {
    if (isformat) {
      cur_format[j] = fmt[i];
      bool finished_format = true;
      if (cur_format[j - 1] == 's' && cur_format[j] == 'b') {
        String val = va_arg(ap, String);
        print_string(val);
      // Standard C output
      } else if (cur_format[j] == 'c') {
        if (cur_format[j-1] == 'l') {
          wint_t val = va_arg(ap, wint_t);
          printf(cur_format, val);
        } else {
          int val = va_arg(ap, int);
          printf(cur_format, val);
        }
      } else if (cur_format[j] == 's' && fmt[i+1] != 'b') {
        if (cur_format[j-1] == 'l') {
          wchar_t* val = va_arg(ap, wchar_t*);
          printf(cur_format, val);
        } else {
          char* val = va_arg(ap, char*);
          printf(cur_format, val);
        }
      } else if ((cur_format[j - 1] == 'd' && cur_format[j] == 'i') || cur_format[j] == 'd') {
        if (cur_format[j-3] == 'l' && cur_format[j-2] == 'l') {
          long long val = va_arg(ap, long long);
          printf(cur_format, val);
        } else if (cur_format[j-1] == 'l') {
          long val = va_arg(ap, long);
          printf(cur_format, val);
        }
        int val = va_arg(ap, int);
        printf(cur_format, val);
      } else if (cur_format[j] == 'u' || cur_format[j] == 'o' || cur_format[j] == 'x' || cur_format[j] == 'X') {
        if (cur_format[j-2] == 'l' && cur_format[j-1] == 'l') {
          unsigned long long val = va_arg(ap, unsigned long long);
          printf(cur_format, val);
        } else if (cur_format[j-1] == 'l') {
          unsigned long val = va_arg(ap, unsigned long);
          printf(cur_format, val);
        }
        unsigned int val = va_arg(ap, unsigned int);
        printf(cur_format, val);
      } else if (cur_format[j] == 'f') {
        double val = va_arg(ap, double);
        printf(cur_format, val);
      } else if (cur_format[j] == 'p') {
        void* val = va_arg(ap, void*);
        printf(cur_format, val);
      } else {
        finished_format = false;
      }

      if (finished_format) {
        isformat = false;
        j = 0;
        memset(cur_format, 0, 20);
      } else {
        j++;
      }
    } else if (fmt[i] == '%') {
      isformat = true;
      cur_format[j] = fmt[i];
      j++;
    } else {
      putchar(fmt[i]);
    }
  }

  va_end(ap);    //Release pointer. Normally do_nothing
}
