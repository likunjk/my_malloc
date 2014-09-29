#include "MyMalloc.h"

//http://blog.jobbole.com/75656/
/******************使用一个大数组来模拟堆**************/
#define MEM_SIZE 10240
static char rawData[MEM_SIZE];	//在C中的const表示只读，不代表常量
static char *pBreak = rawData;	//下一次分配的起始地址

struct block_t
{
	size_t size;	//真实数据区的大小
	struct block_t *pre;	//指向上一个块
	struct block_t *next;	//指向下一个块
	int isEmpty;	//是否是空闲块 1-空闲  0-非空闲
	void *magicPtr;	//Magic Pointer, 指向data
	char data[1];	//这是一个虚拟字段，表示数据块的第一个字节，长度不应计入Block的size
};
static const size_t BLOCK_SIZE = 20;
static const size_t BLOCK_SPLIT = 512;
static Block* firstBlock = NULL;


/**************************************
	功能: 将break指针直接设置为某个地址
	返回值: 成功返回0; 不成功返回-1
**************************************/
static int my_brk(char *addr)
{
	if(addr >= rawData && addr <= rawData+MEM_SIZE)
	{
		pBreak = addr;
		return 0;
	}
	return -1;
}


/***********************************************************
	功能: 将break指针从当前位置移动increment增量
	返回值: 成功返回break移动之前指向的地址(本次分配空间的首地址); 
	       失败返回-1
************************************************************/
static void* my_sbrk(size_t increment)	
{
	void *p = (void*)-1;
	if(pBreak+increment <= rawData+MEM_SIZE)
	{
		p = (void*)pBreak;
		pBreak += increment;
		return p;
	}
	return p;
}


/**************************************************
	功能: 采用First fit策略，即找到第一个满足要求的块
	last: 指向满足要求块的前一个块
	返回值: 指向第一个满足要求块的指针(若都不满足,则为空)
***************************************************/
static Block* findBlock(Block **last, size_t size)
{
	Block *b = firstBlock;
	while( !b && !(b->isEmpty && b->size >= size) )
	{
		*last = b;
		b = b->next;
	}
	return b;
}


/**************************************************
	功能: 从堆申请新的物理块
	返回值: 成功时返回指向申请块的指针
	       失败时返回NULL
***************************************************/
static Block* extendBlock(Block* last, size_t size)
{
	Block* b;
	b = (Block*)my_sbrk(0);	//小技巧，获取当前堆的break指针

	if( my_sbrk(BLOCK_SIZE + size) == (void*)-1 )
		return NULL;

	b->size = size;
	b->next = NULL;
	b->magicPtr = b->data;
	b->isEmpty = 0;	//申请的块肯定是要拿来用的
	if(last)
	{
		last->next = b;
		b->pre = last;
	}
	else
	{
		b->pre = NULL;
	}

	return b;
}


/*****************************************
	功能: 当一个块过大时将其分裂
	返回值: 空
******************************************/
static void splitBlock(Block *b, size_t size)
{
	Block *newB;
	newB = (Block*)(b->data + size);
	newB->size = b->size - size - BLOCK_SIZE;
	newB->isEmpty = 1;	//刚分裂出来的块肯定是没有使用的
	newB->magicPtr = newB->data;
	
	newB->next = b->next;
	b->next->pre = newB;
	b->next = newB;
	newB->pre = b;

	b->size = size;	//修改原块的大小
}


/***********************************************************************
	功能: 保证新分配内存大小是4的倍数, 若不是则扩展到大于size的最小的4个倍数
	返回值: 调整之后的大小
	备注: 默认32位机，所以要保证是4的倍数
************************************************************************/
static size_t align4(size_t x)
{
	if( (x & 0x3) == 0)
		return x;
	else
		return ( ( x >> 2 ) + 1 ) << 2;
}


/******************************************
	功能: 根据首地址得到相应的Block信息
	返回值: addr对应的Block块
*******************************************/
static Block* getBlock(void *addr)
{
	// 此处不能将addr转换成Block*，因为不同类型的指针做加减法时步长是不一样的
	char *tmp = (char*)addr;
	
	Block *b = (Block*)(tmp - BLOCK_SIZE);

	return b;
}


/***************************************************
	功能: 验证将要free的地址addr是否是否有效, 包括两方面:
	     1、地址应该在first_block和当前break指针范围内
	     2、地址确实是我们分配的, 通过Magic Pointer来验证
	返回值: 合法返回1，不合法返回0
****************************************************/
static int validAddr(void *addr)
{
	if(firstBlock)
	{
		if(addr > (void*)firstBlock && addr < my_sbrk(0))
			return addr == getBlock(addr)->magicPtr;
	}
	return 0;	//C语言中没有Bool类型
}

/**********************************
	功能: 合并当前块及其后面的块
	返回值: 无
**********************************/
static void	mergeBlock(Block *b)
{
	Block *tt;
	
	b->size += (b->next->size + BLOCK_SIZE);	// 这里一定要先加
	
	tt = b->next->next;
	b->next = tt;
	tt->pre = b;
}


/*********************************************
	功能: 将原src块的内容拷贝至目的des块
	返回值: 无
**********************************************/
static void copyBlock(Block *des, Block *src)
{
	size_t i;
	size_t *p1, *p2;
	p1 = (size_t*)des->data;
	p2 = (size_t*)src->data;

	for(i=0; i<src->size; i+=4)
		p1[i] = p2[i];
}


//下面4个函数供外部调用
void* my_malloc(size_t size)
{
	Block *b, *last;
	size = align4(size);

	if(firstBlock)
	{
		last = (Block*)firstBlock;
		b = findBlock(&last, size);
		if(b)
		{
			if( b->size - size >= BLOCK_SIZE+BLOCK_SPLIT )	//自定义的分裂规则
				splitBlock(b, size);
			b->isEmpty = 0;
		}
		else
		{
			b = extendBlock(last, size);
			if(!b)
				return NULL;
		}
	}
	else	//说明现在还没有一个分配过数据块
	{
		b = extendBlock(NULL, size);
		b->pre = NULL;
		if(!b)
			return NULL;
		firstBlock = b;
	}
	return b->data;
}
void* my_calloc(size_t number, size_t size)
{
	size_t *p;
	size_t i, k;
	p = my_malloc(number*size);
	if(p)
	{
		k = align4(number*size) >> 2;
		for(i=0; i<k; ++i)	//4个字节一起赋值，加快速度
			p[i] = 0;
	}
	return p;
}
/******************************************************************************
	功能: 改变指针addr所指向的内存大小
	备注: 最简单的做法是首先my_malloc一段内存，然后将数据拷贝过去，最后释放掉原来的内存。
	      但是可以做一些优化：
		  (1)如果当前Block的数据区大小大于等于newSize，那么不做任何操作
		  (2)如果newSize较以前变得很小，那么可以考虑拆分当前Block
		  (3)如果当前块不够存放，但其后继块是空闲的，并且合并之后可以满足要求，进行合并。
*******************************************************************************/
void* my_realloc(void *addr, size_t newSize)
{
	Block *b, *newB;
	void *newAddr;
	// 根据标准文档，当addr为NULL时，相当于调用malloc
	if(!addr)
		return my_malloc(newSize);

	if(validAddr(addr)) //进行合法性判断
	{
		newSize = align4(newSize);
		b = getBlock(addr);
		if(b->size >= newSize)
		{
			// 如果超过一定界限，则进行拆分
			if(b->size >= newSize + BLOCK_SPLIT)
				splitBlock(b, newSize);
			return addr;
		}
		else
		{
			// 先尝试后面的是否可以合并
			if(b->next!=NULL && b->next->isEmpty==1 && b->size+b->next->size+BLOCK_SIZE >= newSize)
			{
				mergeBlock(b);
				if(b->size >= newSize + BLOCK_SPLIT)
					splitBlock(b, newSize);
				return addr;
			}
			else
			{	
				// 分配一块大内存
				newAddr = my_malloc(newSize);
				if(!newAddr)
					return NULL;
				newB = getBlock(newAddr);
				copyBlock(newB, b);

				my_free(b);	//释放原来的块

				return newAddr;
			}
		}
	}
	return NULL;
}

void  my_free(void *addr)
{
	if( 1 == validAddr(addr) )
	{
		Block *b = getBlock(addr); 
		b->isEmpty = 1;
		if(b->next==NULL)	//说明当前块是最后一个块, 特殊处理
		{
			// 回退break指针, 也就是还给堆
			my_brk((char*)b);
			if(b->pre == NULL)	//同时也是第一个块
				firstBlock = NULL;
			else
				b->pre->next = NULL;
		}
		else
		{
			// 说明下一个块是空闲块，那么进行合并
			if(b->next->isEmpty==0)
				mergeBlock(b);

			// 说明上一个块存在，并且是空闲块
			if(b->pre!=NULL && b->pre->isEmpty==1)
				mergeBlock(b->pre);
		}
	}
}


