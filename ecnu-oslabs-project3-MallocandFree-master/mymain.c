#include<stdio.h>
#include"mem.h"
int main(){
	mem_init(1000);
	mem_dump();
	mem_alloc(128,M_FIRSTFIT);
	void* p = mem_alloc(256,M_FIRSTFIT);
	mem_dump();
	mem_free(p);
	mem_dump();
	return 0;
}