/**
 * @file list.h
 * doubled-linked list helpers
 * @note this code is a rip of Linux kernel source (include/linux/list.h)
 */
/*
 * This file is part of rdp2tcp
 *
 * Copyright (C) 2010-2011, Nicolas Collignon
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __R2T_LIST__
#define __R2T_LIST__

#include "compiler.h"

/** double-linked list */
struct list_head {
	struct list_head *next, *prev;
};

/**
 * define an initialized double-linked list
 */
#define LIST_HEAD_INIT(name) \
	struct list_head name = { &(name), &(name) }

/**
 * check whether the list pointer is valid
 */
#define valid_list_head(l) ((l) && (l)->next && (l)->prev)

static inline void list_init(struct list_head *head)
{
	head->next = head;
	head->prev = head;
}

/**
 * list_empty - tests whether a list is empty
 * @head: the list to test.
 */
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

/**
 * list_add_tail - add a new entry
 * @new: new entry to be added
 * @head: list head to add it before
 *
 * Insert a new entry before the specified head.
 */
static inline void list_add_tail(struct list_head *lst, struct list_head *head)
{
	head->prev->next = lst;
	lst->next = head;
	lst->prev = head->prev;
	head->prev = lst;
}

/**
 * list_del - deletes entry from list.
 * @entry: the element to delete from the list.
 * Note: list_empty() on entry does not return true after this, the entry is
 * in an undefined state.
 */
static inline void list_del(struct list_head *lst)
{
	lst->next->prev = lst->prev;
	lst->prev->next = lst->next;
}

/**
 * iterate over a list
 * @param[in] pos the struct list_head to use as a loop cursor
 * @param[in] head the head for the list
 */
#define list_for_each(pos, head) \
	for (pos = (typeof(pos))(head)->next; \
			((struct list_head *)pos) != (head); \
			pos = (typeof(pos)) ((struct list_head *)pos)->next)

/**
 * iterate over a list safe against removal of list entry
 * @param[in] pos the struct list_head to use as a loop cursor
 * @param[in] n another struct list_head to use as temporary storage
 * @param[in] head the head for the list
 */
#define list_for_each_safe(pos, n, head) \
	for (pos = (typeof(pos))(head)->next, \
			n = (typeof(pos))((struct list_head *)pos)->next; \
			((struct list_head *)pos) != (head); \
		pos = n, n = (typeof(pos)) ((struct list_head *)pos)->next)

#endif
