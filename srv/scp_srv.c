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
static int scp_rx_open_snd(char *src, char *complete_dst, char *resp)
{
  struct stat sb;
  char *dir_path;
  int result = -1;
  memset(resp, 0, MAX_PATH_LEN);
  if (stat(complete_dst, &sb) == -1)
    {
    dir_path = dirname(complete_dst);
    if (stat(dir_path, &sb) != -1)
      {
      snprintf(resp, MAX_PATH_LEN-1, "OK %s %s", src, complete_dst); 
      result = 0;
      }
    else 
      snprintf(resp, MAX_PATH_LEN-1, 
               "KO %s directory does not exist", dir_path);
    }
  else
    snprintf(resp, MAX_PATH_LEN-1, 
             "KO %s exists already\n", complete_dst);
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int scp_rx_open_rcv(char *src, char *complete_dst, char *resp)
{
  struct stat sb;
  char *bn;
  int result = -1;
  memset(resp, 0, MAX_PATH_LEN);
  if (stat(src, &sb) == -1)
    snprintf(resp, MAX_PATH_LEN-1, "KO %s does not exist\n", src);
  else
    {
    snprintf(resp, MAX_PATH_LEN-1, "OK %s %s", src, complete_dst); 
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void scp_cli_snd(int *fd, char *src, char *complete_dst, char *resp)
{
  if (!scp_rx_open_snd(src, complete_dst, resp))
    KERR("OOOKKK SND %s %s %s", src, complete_dst, resp); 
  else
    KERR("KKK SND %s %s %s", src, complete_dst, resp); 
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
void scp_cli_rcv(int *fd, char *src, char *complete_dst, char *resp)
{ 
  if (!scp_rx_open_rcv(src, complete_dst, resp))
    KERR("OOOKKK RCV %s %s %s", src, complete_dst, resp); 
  else
    KERR("KKK RCV %s %s %s", src, complete_dst, resp); 
}
/*--------------------------------------------------------------------------*/


