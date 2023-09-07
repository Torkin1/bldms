#ifndef USCTM_H_INCLUDED
#define USCTM_H_INCLUDED

struct usctm_syscall_tbl {
    unsigned long *hacked_ni_syscall;
    unsigned long **hacked_syscall_tbl;
    unsigned long sys_call_table_address;
    unsigned long sys_ni_syscall_address;
};

struct usctm_syscall_tbl *usctm_get_syscall_tbl_ref(void);
int usctm_register_syscall(struct usctm_syscall_tbl *syscall_tbl,
 unsigned long syscall, char *syscall_name);
void usctm_unregister_syscall(struct usctm_syscall_tbl *syscall_tbl,
 int syscall_index);

int usctm_init(char *registered_syscalls_dirname);
void usctm_cleanup(void);

#define usctm_get_syscall_symbol(syscall) __x64_sys_##syscall
#define usctm_get_string_from_symbol(symbol) #symbol

#endif // USCTM_H_INCLUDED