/* This file is part of the UniSet project
 * Copyright (c) 2009 Free Software Foundation, Inc.
 * Copyright (c) 2009 Ivan Donchevskiy
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
// --------------------------------------------------------------------------
/*! \file
 *  \author Ivan Donchevskiy
 *  \date   $Date: 2009/07/15 15:55:00 $
 *  \version $Id: Jrn.h,v 1.0 2009/07/15 15:55:00 vpashka Exp $
 */
// --------------------------------------------------------------------------

#ifndef Storages_H_
#define Storages_H_

#include <stdio.h>
#include <string.h>
#include <UniXML.h>

#define key_size 20

struct TableStorageElem
{
	char status;
	char key[key_size];
} __attribute__((__packed__));

struct TableBlockStorageElem
{
	int count;
	char key[key_size];
} __attribute__((__packed__));

struct CycleStorageElem
{
	char status;
} __attribute__((__packed__));

class TableStorage
{
	FILE *file;
	int seekpos, inf_size;
	int head;
	public:
		int size;
		TableStorage(const char* name, int inf_sz, int sz, int seek);
		~TableStorage();
		int AddRow(char* key, char* val);
		int DelRow(char* key);
		char* FindKeyValue(char* key, char* val);
};

class TableBlockStorage
{
	FILE *file;
	int inf_size;
	int max,lim;
	public:
		int size,cur_block,block_size;
		TableBlockStorage(const char* name, int inf_sz, int sz, int block_num, int block_lim);
		~TableBlockStorage();
		int AddRow(char* key, char* val);
		int DelRow(char* key);
		char* FindKeyValue(char* key, char* val);
};

class CycleStorage
{
	FILE *file;
	int seekpos, inf_size;
	int head,tail;
	public:
		int size, iter;
		CycleStorage(const char* name, int inf_sz, int sz, int seek);
		~CycleStorage();
		int AddRow(char* str);
		int DelRow(int row);
		int DelAllRows(void);
		int ViewRows(int beg, int num);
		int ExportToXML(const char* name);
};

#endif