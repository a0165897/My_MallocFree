/************************************************* 
Author: 李明锟 李德健 宛哲纯
Date:2017/11/19 
Description: 自定义的内存分配器
**************************************************/ 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "mem.h"

/*管理内存块的状态*/
typedef struct memory_control_block{
	int is_available;/*1-内存块为空 0-内存块已被占用*/
	int size;/*内存块大小(字节)*/
}MCB;

/*注意取消指针的偏移量与指针类型的关系*/
MCB *region_start = NULL;/*内存列表的起始内存管理块地址*/
MCB *region_end = NULL;/*内存列表的末尾地址,不调用*/
int m_error;/*异常处理类型*/
int flag_init = 1;/*内存列表初始化flag*/
/*1-未初始化 0-已初始化*/


/************************************************* 
Function:       // mem_init 
Description:    // 初始化内存列表
Input:          // int size_of_region 内存列表需要管理的内存大小(字节) 
Return:         // 发生异常时设置m_error,返回-1
Others:         // 只能初始化一次,通过flag_init实现
*************************************************/  
int mem_init(int size_of_region){

	if(flag_init == 0 || size_of_region<=0){/*不能初始化*/
		m_error = E_BAD_ARGS;
		return -1;
	}

	int page_size = getpagesize();/*获得页大小,默认4096字节*/
	int region_size = page_size*( (size_of_region+sizeof(MCB)-1)/page_size +1);/*内存列表申请总内存的实际大小,按页申请*/

	// open the /dev/zero device
	int fd = open("/dev/zero", O_RDWR);
	// size_of_region (in bytes) needs to be evenly divisible by the page size
	void *ptr = mmap(NULL, region_size, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);/*申请失败*/
	if (ptr == MAP_FAILED) { 
		perror("mmap"); exit(1); /*退出程序*/
	}
	// close the device (don't worry, mapping should be unaffected)
	close(fd);
	
	/*设置内存列表起始内存管理块*/
	region_start = (MCB*)ptr;
	region_start->is_available = 1;
	region_start->size = region_size;

	/*设置内存列表尾*/
	region_end = region_start + region_size / sizeof(MCB);
	flag_init = 0;
	return 0;
}

/************************************************* 
Function:       // mem_alloc
Description:    // 分配内存块
Input:          // int size  内存块大小(字节) int style 查询方式 
Return:         // 没有空间返回空指针,有空间返回指向内存块指针
*************************************************/  
void *mem_alloc(int size, int style){

	int alloc_size = 8*((size + sizeof(MCB)-1)/8+1);/*8字节对齐*/
	MCB *winner_current = NULL;/*内存块指针*/
	MCB *mcb_current = region_start;

	switch(style){
		case M_FIRSTFIT:/*第一块空间足够的内存块*/
			while(mcb_current!=region_end){
				if( mcb_current->is_available && mcb_current->size >= alloc_size ){/*前块可用 当前块空间足够*/
					winner_current = mcb_current;
					break;
				} 
				mcb_current += mcb_current->size/sizeof(MCB);
			}
			break;
		case M_BESTFIT:/*冗余空间最小*/
			while(mcb_current!=region_end){
				if( mcb_current->is_available && 
					mcb_current->size >= alloc_size &&
					(winner_current == NULL || mcb_current->size < winner_current->size)){/*当前块可用 当前块空间足够 第一块或当前最小块*/
					winner_current = mcb_current;
				} 
				mcb_current += mcb_current->size/sizeof(MCB);
			}
			break;
		case M_WORSTFIT:/*冗余空间最大*/
			while(mcb_current!=region_end){
				if( mcb_current->is_available && 
					mcb_current->size >= alloc_size &&
					(winner_current == NULL || mcb_current->size > winner_current->size)){/*当前块可用 当前块空间足够 第一块或当前最大块*/
					winner_current = mcb_current;
				} 
				mcb_current += mcb_current->size/sizeof(MCB);
			}
			break;
		default:
			break;
	}
	if(winner_current == NULL){/*空间不够*/
		m_error = E_NO_SPACE;
		return NULL;
	}

	if(winner_current->size > alloc_size){/*冗余空间初始化为空内存块*/
		MCB *mcb_next = winner_current + alloc_size/sizeof(MCB);
		mcb_next->is_available = 1;
		mcb_next->size =winner_current->size -alloc_size;//int
	}

	winner_current->is_available = 0;
	winner_current->size = alloc_size;
	return ++winner_current;
}

/************************************************* 
Function:       // mem_free
Description:    // 释放内存块
Input:          // void *ptr 需要被释放的内存块
Return:         // 发生异常时返回-1
*************************************************/   
int mem_free(void *ptr){
	if( ptr == NULL){/*空指针*/
		m_error = E_BAD_POINTER;
		return -1;
	}
	MCB *mcb_current = (MCB*)ptr -1;/*指针重新指向内存管理块*/

	mcb_current->is_available = 1;

	/*合并下一个空内存块*/
	MCB *mcb_next = mcb_current + mcb_current->size/sizeof(MCB);
	if((mcb_current+mcb_current->size)<region_end && mcb_next->is_available==1){/*不在表外&&下一个内存块空*/
	 	mcb_current->size += mcb_next->size;//int
		mcb_next->size = 0;
		mcb_next->is_available = 0;
	}

	/*合并上一个空内存块*/
	MCB *mcb_pre = region_start;
	while(mcb_pre<mcb_current){
		if(mcb_pre+mcb_pre->size == mcb_current && mcb_pre->is_available == 1){
			mcb_pre->size += mcb_current->size;
			mcb_current->size = 0;
			mcb_current->is_available = 0;
		}
		mcb_pre += mcb_pre->size/sizeof(MCB);/*合并为mcb_pre*/
	}

	ptr = NULL;/*指针清空*/
	return 0;
}

/************************************************* 
Function:       // mem_dump 
Description:    // 展示当前所有可用内存块
*************************************************/  
void mem_dump(){
	MCB *region_current = region_start;
	while(region_current != region_end){
		if(region_current->is_available){
			printf("free region: %p ~ %p\n",region_current,region_current + region_current->size/sizeof(MCB) );
		}
		region_current += region_current->size / sizeof(MCB);
	}
}

