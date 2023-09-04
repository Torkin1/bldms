#ifndef TESTS_H_INCLUDED
#define TESTS_H_INCLUDED

#include "device/device.h"

int bldms_tests_init(struct bldms_device *dev);
void bldms_tests_cleanup(void);

#endif // TESTS_H_INCLUDED