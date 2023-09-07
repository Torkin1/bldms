#ifndef API_H_INCLUDED
#define API_H_INCLUDED

#include <stddef.h>

int call_kernelspace_test(int test_index);
int put_data(char * source, size_t size);
int get_data(int offset, char * destination, size_t size);
int invalidate_data(int offset);

#endif // API_H_INCLUDED