#ifndef NEXUS_SYSCALL_ELF_LOADER_H
#define NEXUS_SYSCALL_ELF_LOADER_H

#include <stdint.h>
#include "sched/process.h"
#include "fs/vfs.h"

int elf_load(process_t *proc, vfs_node_t *file, uint64_t *entry_out);

#endif /* NEXUS_SYSCALL_ELF_LOADER_H */
