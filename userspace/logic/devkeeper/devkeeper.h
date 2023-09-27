#ifndef DEVKEEPER_H
#define DEVKEEPER_H

#include <stdint.h>

int devkeeper_mount_device(char *dev_path, char *mount_point);
int devkeeper_format_device(char * dev_path, int block_size, int nr_blocks);
int devkeeper_create_mountpoint(char *mount_point, unsigned int mode);

#endif // DEVKEEPER_H