/*****************************************************************************/
/*    Copyright (C) 2017-2018 cloonix@cloonix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "mdl.h"

/*****************************************************************************/
int scp_open_snd(char *src, char *dst, char *complete_dst)
{
  struct stat sb;
  char *bn;
  int result = -1;
  if (stat(src, &sb) == -1)
    printf("%s does not exist\n", src);
  else
    {
    if ((sb.st_mode & S_IFMT) == S_IFREG)
      {
      bn = basename(src);
      memset(complete_dst, 0, MAX_PATH_LEN);
      snprintf(complete_dst, MAX_PATH_LEN-1, "%s/%s", dst, bn);
      result = 0;
      }
    else
      printf("%s is not a regular file\n", src);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int scp_open_rcv(char *src, char *dst, char *complete_dst)
{
  struct stat sb;
  char *bn;
  int result = -1;
  if (stat(dst, &sb) == -1)
    printf("%s does not exist\n", dst);
  else
    {
    if ((sb.st_mode & S_IFMT) == S_IFDIR)
      {
      bn = basename(src);
      memset(complete_dst, 0, MAX_PATH_LEN);
      snprintf(complete_dst, MAX_PATH_LEN-1, "%s/%s", dst, bn);
      if (stat(complete_dst, &sb) != -1) 
        printf("%s exists already\n", complete_dst);
      else
        result = 0;
      }
    else
      printf("%s is not a directory\n", dst);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

