#include "c.h"

/**
 * 内存块头结构
 */
struct block {
	struct block *next; /* 下一块内存 */
	char *limit; /* 本块内可用内存最大值 */
	char *avail; /* 当前的可用内存块首地址 */
};

/**
 * 内存对齐结构
 */
union align {
	long l;
	char *p;
	double d;
	int (*f)(void);
};

/**
 * 对齐的内存块头结构
 */
union header {
	struct block b;
	union align a; // 用来使b域对齐
};

#ifdef PURIFY
union header *arena[3];

/**
 * allocate - 在a内存区内分配一块大小为n字节的内存
 * @n: 要分配的字节数
 * @a: 内存区
 * 返回值: 分配的内存首地址
 */
void *allocate(unsigned long n, unsigned a) {
	union header *new = malloc(sizeof *new + n);

	assert(a < NELEMS(arena));
	if (new == NULL) {
		error("insufficient memory\n");
		exit(1);
	}
	new->b.next = (void *)arena[a];
	arena[a] = new;
	return new + 1;
}

/**
 * deallocate - 释放内存区a内的所有内存
 * @a: 内存区
 * 返回值: 无.
 */
void deallocate(unsigned a) {
	union header *p, *q;

	assert(a < NELEMS(arena));
	for (p = arena[a]; p; p = q) {
		q = (void *)p->b.next;
		free(p);
	}
	arena[a] = NULL;
}

/**
 * newarray - 在a内存区里,申请一个数组,有m个元素,每个元素n个字节.
 * @m: 元素的个数
 * @n: 每个元素的大小
 * @a: 内存区
 * 返回值: 新分配内存的首地址.
 */
void *newarray(unsigned long m, unsigned long n, unsigned a) {
	return allocate(m*n, a);
}
#else
static struct block
	 first[] = {  { NULL },  { NULL },  { NULL } },
	*arena[] = { &first[0], &first[1], &first[2] };
static struct block *freeblocks;

/**
 * allocate - 在a内存区内分配一块大小为n字节的内存
 * @n: 要分配的字节数
 * @a: 内存区
 * 返回值: 分配的内存首地址
 */
void *allocate(unsigned long n, unsigned a) {
	struct block *ap;

	assert(a < NELEMS(arena));
	assert(n > 0);
	ap = arena[a];
	n = roundup(n, sizeof (union align));
	while (n > ap->limit - ap->avail) {
		if ((ap->next = freeblocks) != NULL) {
			freeblocks = freeblocks->next;
			ap = ap->next;
		} else
			{
				unsigned m = sizeof (union header) + n + roundup(10*1024, sizeof (union align));
				ap->next = malloc(m);
				ap = ap->next;
				if (ap == NULL) {
					error("insufficient memory\n");
					exit(1);
				}
				ap->limit = (char *)ap + m;
			}
		ap->avail = (char *)((union header *)ap + 1);
		ap->next = NULL;
		arena[a] = ap;

	}
	ap->avail += n;
	return ap->avail - n;
}

/**
 * newarray - 在a内存区里,申请一个数组,有m个元素,每个元素n个字节.
 * @m: 元素的个数
 * @n: 每个元素的大小
 * @a: 内存区
 * 返回值: 新分配内存的首地址.
 */
void *newarray(unsigned long m, unsigned long n, unsigned a) {
	return allocate(m*n, a);
}

/**
 * deallocate - 释放内存区a内的所有内存
 * @a: 内存区
 * 返回值: 无.
 */
void deallocate(unsigned a) {
	assert(a < NELEMS(arena));
	arena[a]->next = freeblocks;
	freeblocks = first[a].next;
	first[a].next = NULL;
	arena[a] = &first[a];
}
#endif
