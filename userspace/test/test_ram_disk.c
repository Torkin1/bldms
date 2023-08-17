#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#include "test_suites.h"
#include "logger/logger.h"

const char *device_name = "/dev/bldmsdisk";
#define TRD_NR_SECTORS 8 * 16
#define TRD_SECTOR_SIZE 512

static char buffer[TRD_SECTOR_SIZE + 1];
static char buffer_copy[TRD_SECTOR_SIZE + 1];

int test_sector(int fd, size_t sector)
{
	unsigned long i;

	for (i = 0; i < TRD_SECTOR_SIZE; i++)
		buffer[i] = 'a' + rand() % ('z' - 'a');

	lseek(fd, sector * TRD_SECTOR_SIZE, SEEK_SET);
	write(fd, buffer, sizeof(buffer));

	fsync(fd);

	lseek(fd, sector * TRD_SECTOR_SIZE, SEEK_SET);
	read(fd, buffer_copy, sizeof(buffer_copy));

	ON_ERROR_LOG_AND_RETURN((memcmp(buffer, buffer_copy, sizeof(buffer_copy)) != 0), -1, "expected: %s, got: %s\n", buffer, buffer_copy);

	return 0;

}
int test_open_read_write_close(){

    int fd;
	int res = 0;

    memset(buffer, 0, TRD_SECTOR_SIZE + 1);
	memset(buffer_copy, 0, TRD_SECTOR_SIZE + 1);
	
	fd = open(device_name, O_RDWR);
    ON_ERROR_LOG_ERRNO_AND_RETURN(fd < 0, -1, "Failed to open device %s", device_name);

    srand(time(NULL));
	for (int i = 0; i < TRD_NR_SECTORS; i++)
		if (test_sector(fd, i)){
            LOG_ERROR("Sector %d failed\n", i);
            res = -1;
			break;
        }

	close(fd);

    return res;

}