#ifndef _MYMALLOC_H_
#define _MYMALLOC_H_


typedef unsigned int size_t;
#define NULL 0

struct block_t;
typedef struct block_t Block;

/**************************************
	功能: 将break指针直接设置为某个地址
	返回值: 成功返回0; 不成功返回-1
**************************************/
static int my_brk(char *addr);

/***********************************************************
	功能: 将break指针从当前位置移动increment增量
	返回值: 成功返回break移动之前指向的地址(本次分配空间的首地址); 
	       失败返回-1
************************************************************/
static void* my_sbrk(size_t increment);


/**************************************************
	功能: 采用First fit策略，即找到第一个满足要求的块
	last: 指向满足要求块的前一个块
	返回值: 指向第一个满足要求块的指针(若都不满足,则为空)
***************************************************/
static Block* findBlock(Block **last, size_t size);

/**************************************************
	功能: 从堆申请新的物理块
	返回值: 成功时返回指向申请块的指针
	       失败时返回NULL
***************************************************/
static Block* extendBlock(Block* last, size_t size);

/*****************************************
	功能: 当一个块过大时将其分裂
	返回值: 空
******************************************/
static void splitBlock(Block *b, size_t size);

/***********************************************************************
	功能: 保证新分配内存大小是4的倍数, 若不是则扩展到大于size的最小的4个倍数
	返回值: 调整之后的大小
	备注: 默认32位机，所以要保证是4的倍数
************************************************************************/
static size_t align4(size_t x);

/******************************************
	功能: 根据首地址得到相应的Block信息
	返回值: addr对应的Block块
*******************************************/
static Block* getBlock(void *addr);

/***************************************************
	功能: 验证将要free的地址addr是否是否有效, 包括两方面:
	     1、地址应该在first_block和当前break指针范围内
	     2、地址确实是我们分配的, 通过Magic Pointer来验证
	返回值: 合法返回1，不合法返回0
****************************************************/
static int validAddr(void *addr);

/**********************************
	功能: 合并当前块及其后面的块
	返回值: 无
**********************************/
static void	mergeBlock(Block *b);


/*********************************************
	功能: 将原src块的内容拷贝至目的des块
	返回值: 无
**********************************************/
static void copyBlock(Block *des, Block *src);


//下面4个函数供外部调用
void* my_malloc(size_t size);
void* my_calloc(size_t number, size_t size);
void* my_realloc(void *addr, size_t newSize);
void  my_free(void *addr);


#endif
