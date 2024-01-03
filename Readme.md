## Principles of Secure Coding - Week 3 Activity

### Overview

This directory contains the learning materials and activities for Week 3 of the Coursera course "Principles of Secure Coding." The activity focuses on modifying a statically implemented queue library to support dynamic size expansion.

### Original Course Files

- `fqlib_static.h`
- `qlib_static.c`

### Implementation of Dynamic Version

Implemented in the following files:
- `fqlib_dynamic.h`
- `qlib_dynamic.c`
- `test_dynamic.c`

Major changes are in `qlib_dynamic.c`, including the implementation of functions `double_queues_size`, `double_que_size`, `initialize_queues`, and `reset_queues`. Additionally, the internals of `create_queue`, `put_on_queues`, and `delete_queues` have been modified.

### Compilation

Compile the files using the following command, which includes gdb debug and sanitizer options:

```bash
gcc -std=gnu11 -g -fsanitize=address -fsanitize=undefined test_dynamic.c qlib_dynamic.c fqlib_dynamic.h
```

### Data Structure Brief

- **Array `queues`**: Each entry contains a pointer to a queue.
- **`QUEUE`**: Contains metadata for a queue and the actual payload.
- **`Queue.que`**: Contains the actual payload input from the user.


### Robustness of the Program
Almost no pointers are passed around. Every queue is encapsulated in a ticket, which is the only visible interface to the user. Furthermore, every parameter and return value are checked with thorough scrutiny. This robust approach is inherited from the original static version of the program. Special thanks to Professor Matt Bishop for the demonstration!
