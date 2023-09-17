#ifndef API_H_INCLUDED
#define API_H_INCLUDED

#include <stddef.h>

int call_kernelspace_test(int test_index);
int put_data(char * source, size_t size);
int get_data(int offset, char * destination, size_t size);
int invalidate_data(int offset);
int get_int_param(char *param_name);
int get_string_param(char *param_name, char *buf);

#endif // API_H_INCLUDED