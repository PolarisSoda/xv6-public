#ifndef VM_H
#define VM_H

//#include "types.h"
//#include "mmu.h"

int mappages(pde_t *pgdir, void *va, uint size, uint pa, int perm);
pte_t* walkpgdir(pde_t *pgdir, const void *va, int alloc);

#endif //XV6_PAGING_VM_H