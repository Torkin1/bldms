/*
* 
* This is free software; you can redistribute it and/or modify it under the
* terms of the GNU General Public License as published by the Free Software
* Foundation; either version 3 of the License, or (at your option) any later
* version.
* 
* This module is distributed in the hope that it will be useful, but WITHOUT ANY
* WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
* A PARTICULAR PURPOSE. See the GNU General Public License for more details.
* 
* @file usctm.c 
* @brief This is the main source for the Linux Kernel Module which implements
* 	 the runtime discovery of the syscall table position and of free entries (those 
* 	 pointing to sys_ni_syscall) 
*
* @author Francesco Quaglia (adapted by Torkin for the bldms project in August 2023)
*
* @date November 22, 2020
*/

#define EXPORT_SYMTAB
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/kprobes.h>
#include <linux/mutex.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/apic.h>
#include <linux/syscalls.h>
#include <linux/sysfs.h>
#include "./include/vtpmo.h"

#include "usctm.h"

#define MODNAME "USCTM"

extern int sys_vtpmo(unsigned long vaddr);


#define ADDRESS_MASK 0xfffffffffffff000//to migrate

#define START 			0xffffffff00000000ULL		// use this as starting address --> this is a biased search since does not start from 0xffff000000000000
#define MAX_ADDR		0xfffffffffff00000ULL
#define FIRST_NI_SYSCALL	134
#define SECOND_NI_SYSCALL	174
#define THIRD_NI_SYSCALL	182 
#define FOURTH_NI_SYSCALL	183
#define FIFTH_NI_SYSCALL	214	
#define SIXTH_NI_SYSCALL	215	
#define SEVENTH_NI_SYSCALL	236	

#define ENTRIES_TO_EXPLORE 256

#define MAX_FREE 15

struct usctm_registered_syscall{
    int syscall_desc;
    struct kobj_attribute kobj_attr;
};

ssize_t usctm_registered_syscall_show(struct kobject *kobj, struct kobj_attribute *kobj_attr, char *buf);

static struct usctm_syscall_tbl syscall_tbl = {0};
static struct kobject *registered_syscalls_kobj = NULL;
static struct usctm_registered_syscall registered_syscalls[MAX_FREE];

static int good_area(unsigned long * addr){

	int i;
	
	for(i=1;i<FIRST_NI_SYSCALL;i++){
		if(addr[i] == addr[FIRST_NI_SYSCALL]) goto bad_area;
	}	

	return 1;

bad_area:

	return 0;

}

/* This routine checks if the page contains the begin of the syscall_table.  */
static int validate_page(unsigned long *addr){
	int i = 0;
	unsigned long page 	= (unsigned long) addr;
	unsigned long new_page 	= (unsigned long) addr;
	for(; i < PAGE_SIZE; i+=sizeof(void*)){		
		new_page = page+i+SEVENTH_NI_SYSCALL*sizeof(void*);
			
		// If the table occupies 2 pages check if the second one is materialized in a frame
		if( 
			( (page+PAGE_SIZE) == (new_page & ADDRESS_MASK) )
			&& sys_vtpmo(new_page) == NO_MAP
		) 
			break;
		// go for patter matching
		addr = (unsigned long*) (page+i);
		if(
			   ( (addr[FIRST_NI_SYSCALL] & 0x3  ) == 0 )		
			   && (addr[FIRST_NI_SYSCALL] != 0x0 )			// not points to 0x0	
			   && (addr[FIRST_NI_SYSCALL] > 0xffffffff00000000 )	// not points to a locatio lower than 0xffffffff00000000	
	//&& ( (addr[FIRST_NI_SYSCALL] & START) == START ) 	
			&&   ( addr[FIRST_NI_SYSCALL] == addr[SECOND_NI_SYSCALL] )
			&&   ( addr[FIRST_NI_SYSCALL] == addr[THIRD_NI_SYSCALL]	 )	
			&&   ( addr[FIRST_NI_SYSCALL] == addr[FOURTH_NI_SYSCALL] )
			&&   ( addr[FIRST_NI_SYSCALL] == addr[FIFTH_NI_SYSCALL] )	
			&&   ( addr[FIRST_NI_SYSCALL] == addr[SIXTH_NI_SYSCALL] )
			&&   ( addr[FIRST_NI_SYSCALL] == addr[SEVENTH_NI_SYSCALL] )	
			&&   (good_area(addr))
		){
			syscall_tbl.hacked_ni_syscall = (void*)(addr[FIRST_NI_SYSCALL]);				// save ni_syscall
			syscall_tbl.sys_ni_syscall_address = (unsigned long)syscall_tbl.hacked_ni_syscall;
			syscall_tbl.hacked_syscall_tbl = (void*)(addr);				// save syscall_table address
			syscall_tbl.sys_call_table_address = (unsigned long) syscall_tbl.hacked_syscall_tbl;
			return 1;
		}
	}
	return 0;
}

/* This routines looks for the syscall table.  */
static void syscall_table_finder(void){
	unsigned long k; // current page
	unsigned long candidate; // current page

	for(k=START; k < MAX_ADDR; k+=4096){	
		candidate = k;
		if(
			(sys_vtpmo(candidate) != NO_MAP) 	
		){
			// check if candidate maintains the syscall_table
			if(validate_page( (unsigned long *)(candidate)) ){
				printk("%s: syscall table found at %px\n",MODNAME,(void*)(syscall_tbl.hacked_syscall_tbl));
				printk("%s: sys_ni_syscall found at %px\n",MODNAME,(void*)(syscall_tbl.hacked_ni_syscall));
				break;
			}
		}
	}
	
}

static inline void
write_cr0_forced(unsigned long val)
{
    unsigned long __force_order;

    /* __asm__ __volatile__( */
    asm volatile(
        "mov %0, %%cr0"
        : "+r"(val), "+m"(__force_order));
}

static inline void
protect_memory(unsigned long cr0)
{
    write_cr0_forced(cr0);
}

static inline void
unprotect_memory(unsigned long cr0)
{
    write_cr0_forced(cr0 & ~X86_CR0_WP);
}

/* returns syscall index corresponding to the given pseudo file in sysfs*/
ssize_t usctm_registered_syscall_show(struct kobject *kobj, struct kobj_attribute *kobj_attr, char *buf){

	int i;

	for (i = 0; i < MAX_FREE; i ++){
		if (!strcmp(kobj_attr->attr.name, registered_syscalls[i].kobj_attr.attr.name)){
			return sysfs_emit(buf, "%d\n%c", registered_syscalls[i].syscall_desc, '\0');
		}
	}

	pr_err("%s: failed to find syscall %s\n",__func__,kobj_attr->attr.name);
	return -1;
}

/**
 * Singleton reference to syscall table and ni_syscall addresses
*/
struct usctm_syscall_tbl *usctm_get_syscall_tbl_ref(void)
{
	if (!syscall_tbl.hacked_syscall_tbl) {
		syscall_table_finder();
		if(!syscall_tbl.hacked_syscall_tbl){
			printk("%s: failed to find the sys_call_table\n",MODNAME);
			return NULL;
		}
	}
	return &syscall_tbl;
}

/** traverses syscall table to get the index of the first free entry
  * encountered
  */
static int usctm_get_syscall_table_first_free_entry(struct usctm_syscall_tbl *syscall_tbl)
{		
	int i;
	for (i = 0; i < ENTRIES_TO_EXPLORE; i ++){
		if(syscall_tbl->hacked_syscall_tbl[i] == syscall_tbl->hacked_ni_syscall){
			printk("%s: found sys_ni_syscall entry at syscall_table[%d]\n",MODNAME,i);	
			return i;
		}
	}
	pr_err("%s: no free entries found\n",__func__);
	return -1;
}

int usctm_register_syscall(struct usctm_syscall_tbl *syscall_tbl,
 unsigned long syscall, char *syscall_name)
{
	int syscall_desc;
	unsigned long cr0;
	int res;
	int i;
	
	// gets a free entry from the syscall table
	syscall_desc = usctm_get_syscall_table_first_free_entry(syscall_tbl);
	if (syscall_desc < 0) {
		pr_err("%s: unable to get a free entry from the syscall table\n",__func__);
		return -1;
	}

	// installs the syscall in the syscall tbl
	cr0 = read_cr0();
	unprotect_memory(cr0);
	syscall_tbl->hacked_syscall_tbl[syscall_desc] = (unsigned long*)syscall;
	protect_memory(cr0);

	// exposes the installed syscalls in a pseudofile in sys
	// if we arrive here we have a free entry in the syscall table,
	// so we must have a free entry in the registered_syscalls array too
	for(i = 0; i < MAX_FREE; i ++){
		if (registered_syscalls[i].syscall_desc < 0) break;
	}
	pr_debug("%s: registering syscall %s at index %d with desc %d\n",
	 __func__,syscall_name,i,syscall_desc );
	registered_syscalls[i].syscall_desc = syscall_desc;
	registered_syscalls[i].kobj_attr.attr.name = syscall_name;
	res = sysfs_create_file(registered_syscalls_kobj, &registered_syscalls[i].kobj_attr.attr);
	if (res) {
		pr_err("%s: failed to create sysfs file for syscall %s\n",__func__,syscall_name);
		return -1;
	}

	return syscall_desc;

}

void usctm_unregister_syscall(struct usctm_syscall_tbl *syscall_tbl,
 int syscall_desc)
{
	unsigned long cr0;
	int i;

	// check if desc corresponds to a syscall registered
	// with usctm
	for (i = 0; i < MAX_FREE; i ++){
		if (registered_syscalls[i].syscall_desc == syscall_desc){
			break;
		}
	}
	if (i == MAX_FREE){
		pr_warn("%s: syscall with desc %d is not registered with usctm\n",__func__,syscall_desc);
		return;
	}
	
	// invalidate syscall tbl entry
	cr0 = read_cr0();
	unprotect_memory(cr0);
	syscall_tbl->hacked_syscall_tbl[syscall_desc] = (unsigned long*)
	 syscall_tbl->hacked_ni_syscall;
	protect_memory(cr0);

	// unregister corresponding sysfs file
	if (registered_syscalls[i].syscall_desc == syscall_desc){
		pr_debug("%s: unregistering syscall %s at index %d with desc %d\n",
			__func__,registered_syscalls[i].kobj_attr.attr.name,i,
			registered_syscalls[i].syscall_desc);
		/**
		 * FIXME: sysfs_remove_file crashes kernel when invoked. Don't know why lol
		 * */
		sysfs_remove_file(registered_syscalls_kobj, &registered_syscalls[i].kobj_attr.attr);
		registered_syscalls[i].syscall_desc = -1;
		registered_syscalls[i].kobj_attr.attr.name = NULL;
	}	
}

int usctm_init(char *registered_syscalls_dirname){

	int i;
	memset(registered_syscalls, 0, sizeof(struct usctm_registered_syscall) * MAX_FREE);
	for(i = 0; i < MAX_FREE; i++){
		registered_syscalls[i].syscall_desc = -1;
		registered_syscalls[i].kobj_attr.attr.mode = 0444;	// read only
		registered_syscalls[i].kobj_attr.show = usctm_registered_syscall_show;
	}

	registered_syscalls_kobj = kobject_create_and_add(registered_syscalls_dirname, kernel_kobj);
	if (!registered_syscalls_kobj) {
		pr_err("%s: failed to create registered_syscalls kobject\n",__func__);
		return -ENOMEM;
	}

	return 0;
}

void usctm_unregister_all_syscalls(void){
	int i;
	int current_syscall_desc;
	
	for (i = 0; i < MAX_FREE; i ++){
		current_syscall_desc = registered_syscalls[i].syscall_desc;
		if (current_syscall_desc >= 0){
			usctm_unregister_syscall(usctm_get_syscall_tbl_ref(), current_syscall_desc);
		}
	}
}

void usctm_cleanup(void){

	usctm_unregister_all_syscalls();
	kobject_put(registered_syscalls_kobj);
	
}