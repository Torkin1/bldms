# Block-Level Data Management System
This specification is related to a Linux device driver implementing block-level maintenance of
user data, in particular of user messages.

## How to build
Binary build with debug informations and page cache write-back policy:
```
make -C kernelspace debug
```
Binary build with debug informations and page cache write-through policy:
```
make -C kernelspace debug-wt
```
Build test driver:
```
make -C userspace ./bin/test
```
To change test to launch, edit `userspace/test/test_main.c`

## Usage

Once built, the module can be loaded using `ìnsmod`. Default values for module params are included in module source.

To load the module with default param values:
```
make -C kernelspace load
```
After loading the module, assuming default values for module params, a new block device will appear at `/dev/bldmsdisk`. Users can format and mount such device with bldms using the functions declared in `userspace/logic/devkeeper/devkeeper.h`.

Note that there is no strict need to use such device as the bldms support. Users can use whatever device they want, even a regular file, given that it is correctly formatted using the devkeeper.

Users are expected to build their clients using apis declared in `userspace/logic/api/api.h` if they want to access vfs unsupported operations.

## Kernelspace design

The design boils down to the interaction between two main components:
 - `ops`: implements interactions from userspace to the bldms system. It stems in additional two sub-components:
    - `ops/vfs_unsupported`: Implements the vfs-unsupported operations of `put_data()`, `get_data()` and `invalidate_data()` as system calls;
    - `ops/vfs_supported`: Implement a custom implementation of vfs `read()`.
 - `block_layer`: Provides a block-view of the underlying device, along with methods to interact with it while keeping consistent among concurrent usages;

The following components help the previous ones to do their job:
 - `usctm`: provides facilities to register new syscalls in free entries of the syscall table;
 - `singlefilefs`: provides a file system used to mount bldms and interact with it using vfs-supported ops (such as `read()`)

Additionally, the `device` component implements a block device that users can use as the underlying device of bldms.

System-wide configuration params and module params default values can be found in `config.h`. Components can define additional configs in their corresponding sources.

Further implementation details can be found in comments in source files.
