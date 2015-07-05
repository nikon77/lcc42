#include "c.h"

static char rcsid[] = "$Id$";

static List freenodes;		/* 指向空闲节点列表 */

/**
 * append - 将x附加到list上,并返回新的list头节点
 * @x: 要附加到链表的元素
 * @list: 链表头
 * 返回值:
 */
List append(void *x, List list) {
	List new;

	if ((new = freenodes) != NULL)
		freenodes = freenodes->link;
	else
		NEW(new, PERM);
	if (list) {
		new->link = list->link;
		list->link = new;
	} else
		new->link = new;
	new->x = x;
	return new;
}

/**
 * length - 求一个链表(List)的长度
 * @list: 链表头
 * 返回值: 列表中元素的个数
 * NOTE: 链表为单向循环结构.
 */
int length(List list) {
	int n = 0;

	if (list) {
		List lp = list;
		do
			n++;
		while ((lp = lp->link) != list);
	}
	return n;
}

/**
 * ltov - 将一个链表中的元素转化成一个指针数组保存在某个arena中(数组以NULL标记结尾)
 * list to vector
 * @list: 链表
 * @arena: 存储区下标
 * 返回值: 数组的首地址
 */
void *ltov(List *list, unsigned arena) {
	int i = 0;
	void **array = newarray(length(*list) + 1, sizeof array[0], arena);

	if (*list) {
		List lp = *list;
		do {
			lp = lp->link;
			array[i++] = lp->x;
		} while (lp != *list);
#ifndef PURIFY
		lp = (*list)->link;
		(*list)->link = freenodes;
		freenodes = lp;
#endif
	}
	*list = NULL;
	array[i] = NULL;
	return array;
}
