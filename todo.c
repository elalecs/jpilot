/*******************************************************************************
 * todo.c
 * A module of J-Pilot http://jpilot.org
 *
 * Copyright (C) 1999-2014 by Judd Montgomery
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 ******************************************************************************/

/********************************* Includes ***********************************/
#include "config.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pi-source.h>
#include <pi-socket.h>
#include <pi-todo.h>
#include <pi-dlp.h>

#include "i18n.h"
#include "utils.h"
#include "log.h"
#include "todo.h"
#include "prefs.h"
#include "libplugin.h"
#include "password.h"

/********************************* Constants **********************************/

/******************************* Global vars **********************************/
static struct ToDoAppInfo *glob_Ptodo_app_info;

/****************************** Prototypes ************************************/
static int todo_sort(ToDoList **todol, int sort_order);

/****************************** Main Code *************************************/
void free_ToDoList(ToDoList **todo)
{
   ToDoList *temp_todo, *temp_todo_next;

   for (temp_todo = *todo; temp_todo; temp_todo=temp_todo_next) {
      free_ToDo(&(temp_todo->mtodo.todo));
      temp_todo_next = temp_todo->next;
      free(temp_todo);
   }
   *todo = NULL;
}

int get_todo_app_info(struct ToDoAppInfo *ai)
{
   int num, r;
   int rec_size;
   unsigned char *buf;
#ifdef ENABLE_MANANA
   long ivalue;
#endif
   char DBname[32];

   memset(ai, 0, sizeof(*ai));
   buf=NULL;
   /* Put at least one entry in there */
   strcpy(ai->category.name[0], "Unfiled");

#ifdef ENABLE_MANANA
   get_pref(PREF_MANANA_MODE, &ivalue, NULL);
   if (ivalue) {
      strcpy(DBname, "MananaDB");
   } else {
      strcpy(DBname, "ToDoDB");
   }
#else
   strcpy(DBname, "ToDoDB");
#endif

   r = jp_get_app_info(DBname, &buf, &rec_size);
   if ((r != EXIT_SUCCESS) || (rec_size<=0)) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error reading application info %s\n"), __FILE__, __LINE__, DBname);
      if (buf) {
         free(buf);
      }
      return EXIT_FAILURE;
   }

   num = unpack_ToDoAppInfo(ai, buf, rec_size);
   if (buf) {
      free(buf);
   }
   if (num <= 0) {
      jp_logf(JP_LOG_WARN, _("%s:%d Error reading file: %s\n"), __FILE__, __LINE__, DBname);
      return EXIT_FAILURE;
   }

   return EXIT_SUCCESS;
}

int get_todos(ToDoList **todo_list, int sort_order)
{
   return get_todos2(todo_list, sort_order, 1, 1, 1, 1, CATEGORY_ALL);
}

/*
 * sort_order: SORT_ASCENDING | SORT_DESCENDING
 * modified, deleted, private, completed:
 *  0 for no, 1 for yes, 2 for use prefs
 */
int get_todos2(ToDoList **todo_list, int sort_order,
               int modified, int deleted, int privates, int completed,
               int category)
{
   GList *records;
   GList *temp_list;
   int recs_returned, num;
   struct ToDo todo;
   ToDoList *temp_todo_list;
   long keep_modified, keep_deleted, hide_completed;
   int keep_priv;
   buf_rec *br;
   long char_set;
   char *buf;
   pi_buffer_t *RecordBuffer;
#ifdef ENABLE_MANANA
   long ivalue;
#endif

   jp_logf(JP_LOG_DEBUG, "get_todos2()\n");
   if (modified==2) {
      get_pref(PREF_SHOW_MODIFIED, &keep_modified, NULL);
   } else {
      keep_modified = modified;
   }
   if (deleted==2) {
      get_pref(PREF_SHOW_DELETED, &keep_deleted, NULL);
   } else {
      keep_deleted = deleted;
   }
   if (privates==2) {
      keep_priv = show_privates(GET_PRIVATES);
   } else {
      keep_priv = privates;
   }
   if (completed==2) {
      get_pref(PREF_TODO_HIDE_COMPLETED, &hide_completed, NULL);
   } else {
      hide_completed = !completed;
   }
   get_pref(PREF_CHAR_SET, &char_set, NULL);

   *todo_list=NULL;
   recs_returned = 0;

#ifdef ENABLE_MANANA
   get_pref(PREF_MANANA_MODE, &ivalue, NULL);
   if (ivalue) {
      num = jp_read_DB_files("MananaDB", &records);
      if (-1 == num)
         return 0;
   } else {
      num = jp_read_DB_files("ToDoDB", &records);
      if (-1 == num)
         return 0;
   }
#else
   num = jp_read_DB_files("ToDoDB", &records);
   if (-1 == num)
      return 0;
#endif

   for (temp_list = records; temp_list; temp_list = temp_list->next) {
      if (temp_list->data) {
         br=temp_list->data;
      } else {
         continue;
      }
      if (!br->buf) {
         continue;
      }

      if ( ((br->rt==DELETED_PALM_REC)  && (!keep_deleted)) ||
           ((br->rt==DELETED_PC_REC)    && (!keep_deleted)) ||
           ((br->rt==MODIFIED_PALM_REC) && (!keep_modified)) ) {
         continue;
      }
      if ((keep_priv != SHOW_PRIVATES) &&
          (br->attrib & dlpRecAttrSecret)) {
         continue;
      }

      RecordBuffer = pi_buffer_new(br->size);
      memcpy(RecordBuffer->data, br->buf, br->size);
      RecordBuffer->used = br->size;

      if (unpack_ToDo(&todo, RecordBuffer, todo_v1) == -1) {
         pi_buffer_free(RecordBuffer);
         continue;
      }
      pi_buffer_free(RecordBuffer);

      if ( ((br->attrib & 0x0F) != category) && category != CATEGORY_ALL) {
         free_ToDo(&todo);
         continue;
      }

      if (hide_completed && todo.complete) {
         continue;
      }

      if (todo.description) {
         buf = charset_p2newj(todo.description, -1, char_set);
         if (buf) {
            free(todo.description);
            todo.description = buf;
         }
      }
      if (todo.note) {
         buf = charset_p2newj(todo.note, -1, char_set);
         if (buf) {
            free(todo.note);
            todo.note = buf;
         }
      }
      temp_todo_list = malloc(sizeof(ToDoList));
      if (!temp_todo_list) {
         jp_logf(JP_LOG_WARN, "get_todos2(): %s\n", _("Out of memory"));
         break;
      }
      memcpy(&(temp_todo_list->mtodo.todo), &todo, sizeof(struct ToDo));
      temp_todo_list->app_type = TODO;
      temp_todo_list->mtodo.rt = br->rt;
      temp_todo_list->mtodo.attrib = br->attrib;
      temp_todo_list->mtodo.unique_id = br->unique_id;
      temp_todo_list->next = *todo_list;
      *todo_list = temp_todo_list;
      recs_returned++;
   }

   jp_free_DB_records(&records);

   todo_sort(todo_list, sort_order);

   jp_logf(JP_LOG_DEBUG, "Leaving get_todos2()\n");

   return recs_returned;
}

/*
 * This function just checks some todo fields to make sure they are valid.
 * It truncates the description and note fields if necessary.
 */
static void pc_todo_validate_correct(struct ToDo *todo)
{
   if (todo->description) {
      if ((strlen(todo->description)+1 > MAX_TODO_DESC_LEN)) {
         jp_logf(JP_LOG_WARN, _("ToDo description text > %d, truncating to %d\n"), MAX_TODO_DESC_LEN, MAX_TODO_DESC_LEN-1);
         todo->description[MAX_TODO_DESC_LEN-1]='\0';
      }
   }
   if (todo->note) {
      if ((strlen(todo->note)+1 > MAX_TODO_NOTE_LEN)) {
         jp_logf(JP_LOG_WARN, _("ToDo note text > %d, truncating to %d\n"), MAX_TODO_NOTE_LEN, MAX_TODO_NOTE_LEN-1);
         todo->note[MAX_TODO_NOTE_LEN-1]='\0';
      }
   }
}

int pc_todo_write(struct ToDo *todo, PCRecType rt, unsigned char attrib,
                  unsigned int *unique_id)
{
   pi_buffer_t *RecordBuffer;
   buf_rec br;
   long char_set;
#ifdef ENABLE_MANANA
   long ivalue;
#endif

   get_pref(PREF_CHAR_SET, &char_set, NULL);
   if (char_set != CHAR_SET_LATIN1) {
      if (todo->description) {
         charset_j2p(todo->description, strlen(todo->description)+1, char_set);
      }
      if (todo->note) {
         charset_j2p(todo->note, strlen(todo->note)+1, char_set);
      }
   }

   pc_todo_validate_correct(todo);
   RecordBuffer = pi_buffer_new(0);
   if (pack_ToDo(todo, RecordBuffer, todo_v1) == -1) {
      PRINT_FILE_LINE;
      jp_logf(JP_LOG_WARN, "pack_ToDo %s\n", _("error"));
      pi_buffer_free(RecordBuffer);
      return EXIT_FAILURE;
   }
   br.rt=rt;
   br.attrib = attrib;
   br.buf = RecordBuffer->data;
   br.size = RecordBuffer->used;
   /* Keep unique ID intact */
   if (unique_id) {
      br.unique_id = *unique_id;
   } else {
      br.unique_id = 0;
   }

#ifdef ENABLE_MANANA
   get_pref(PREF_MANANA_MODE, &ivalue, NULL);
   if (ivalue) {
      jp_pc_write("MananaDB", &br);
   } else {
      jp_pc_write("ToDoDB", &br);
   }
#else
   jp_pc_write("ToDoDB", &br);
#endif
   if (unique_id) {
      *unique_id = br.unique_id;
   }

   pi_buffer_free(RecordBuffer);

   return EXIT_SUCCESS;
}

/*
 * sort by:
 * priority, due date
 * due date, priority
 * category, priority
 * category, due date
 */
static int todo_compare(const void *v1, const void *v2)
{
   time_t t1, t2;
   int r;
   int cat1, cat2;
   ToDoList **todol1, **todol2;
   struct ToDo *todo1, *todo2;
   int sort_by_priority;

   todol1=(ToDoList **)v1;
   todol2=(ToDoList **)v2;

   todo1=&((*todol1)->mtodo.todo);
   todo2=&((*todol2)->mtodo.todo);

   sort_by_priority = glob_Ptodo_app_info->sortByPriority;

   cat1 = (*todol1)->mtodo.attrib & 0x0F;
   cat2 = (*todol2)->mtodo.attrib & 0x0F;

   if (sort_by_priority == 0) {
      /* due date, priority */
      r = todo2->indefinite - todo1->indefinite;
      if (r) {
         return r;
      }
      if ( !(todo1->indefinite) && !(todo2->indefinite) ) {
         t1 = mktime(&(todo1->due));
         t2 = mktime(&(todo2->due));
         if ( t1 < t2 ) {
            return 1;
         }
         if ( t1 > t2 ) {
            return -1;
         }
      }
      if (todo1->priority < todo2->priority) {
         return 1;
      }
      if (todo1->priority > todo2->priority) {
         return -1;
      }
      /* If all else fails sort alphabetically */
      if (todo1->description && todo2->description) {
         return strcoll(todo2->description,todo1->description);
      }
   }

   if (sort_by_priority == 1) {
      /* priority, due date */
      if (todo1->priority < todo2->priority) {
         return 1;
      }
      if (todo1->priority > todo2->priority) {
         return -1;
      }
      r = todo2->indefinite - todo1->indefinite;
      if (r) {
         return r;
      }
      if ( !(todo1->indefinite) && !(todo2->indefinite) ) {
         t1 = mktime(&(todo1->due));
         t2 = mktime(&(todo2->due));
         if ( t1 < t2 ) {
            return 1;
         }
         if ( t1 > t2 ) {
            return -1;
         }
      }
      /* If all else fails sort alphabetically */
      if (todo1->description && todo2->description) {
         return strcoll(todo2->description,todo1->description);
      }
   }

   if (sort_by_priority == 2) {
      /* category, priority */
      r = strcoll(glob_Ptodo_app_info->category.name[cat2],
                  glob_Ptodo_app_info->category.name[cat1]);
      if (r) {
         return r;
      }
      if (todo1->priority < todo2->priority) {
         return 1;
      }
      if (todo1->priority > todo2->priority) {
         return -1;
      }
      /* If all else fails sort alphabetically */
      if (todo1->description && todo2->description) {
         return strcoll(todo2->description,todo1->description);
      }
   }

   if (sort_by_priority == 3) {
      /* category, due date */
      r = strcoll(glob_Ptodo_app_info->category.name[cat2],
                  glob_Ptodo_app_info->category.name[cat1]);
      if (r) {
         return r;
      }
      r = todo2->indefinite - todo1->indefinite;
      if (r) {
         return r;
      }
      if ( !(todo1->indefinite) && !(todo2->indefinite) ) {
         t1 = mktime(&(todo1->due));
         t2 = mktime(&(todo2->due));
         if ( t1 < t2 ) {
            return 1;
         }
         if ( t1 > t2 ) {
            return -1;
         }
      }
      /* If all else fails sort alphabetically */
      if (todo1->description && todo2->description) {
         return strcoll(todo2->description,todo1->description);
      }
   }

   return 0;
}

static int todo_sort(ToDoList **todol, int sort_order)
{
   ToDoList *temp_todol;
   ToDoList **sort_todol;
   struct ToDoAppInfo ai;
   int count, i;

   /* Count the entries in the list */
   for (count=0, temp_todol=*todol; temp_todol; temp_todol=temp_todol->next, count++) {}

   if (count<2) {
      /* No need to sort 0 or 1 items */
      return EXIT_SUCCESS;
   }

   get_todo_app_info(&ai);

   glob_Ptodo_app_info = &ai;

   /* Allocate an array to be qsorted */
   sort_todol = calloc(count, sizeof(ToDoList *));
   if (!sort_todol) {
      jp_logf(JP_LOG_WARN, "todo_sort(): %s\n", _("Out of memory"));
      return EXIT_FAILURE;
   }

   /* Set our array to be a list of pointers to the nodes in the linked list */
   for (i=0, temp_todol=*todol; temp_todol; temp_todol=temp_todol->next, i++) {
      sort_todol[i] = temp_todol;
   }

   /* qsort them */
   qsort(sort_todol, count, sizeof(ToDoList *), todo_compare);

   /* Put the linked list in the order of the array */
   if (sort_order==SORT_ASCENDING) {
      for (i=count-1; i>0; i--) {
         sort_todol[i]->next=sort_todol[i-1];
      }
      sort_todol[0]->next = NULL;
      *todol = sort_todol[count-1];
   } else {
      /* Descending order */
      sort_todol[count-1]->next = NULL;
      for (i=count-1; i; i--) {
         sort_todol[i-1]->next=sort_todol[i];
      }
      *todol = sort_todol[0];
   }

   free(sort_todol);

   return EXIT_SUCCESS;
}

