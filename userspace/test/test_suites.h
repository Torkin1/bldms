#ifndef TEST_SUITES_H_INCLUDED
#define TEST_SUITES_H_INCLUDED

int test_open_read_write_close();
int test_syscall(void);
int test_block_serialize(void);
int test_block_move(void);
int test_put_get();
int test_invalidate();

#endif // TEST_SUITES_H_INCLUDED
