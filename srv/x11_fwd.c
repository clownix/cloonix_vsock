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
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include "mdl.h"
#include "x11_fwd.h"

/****************************************************************************/
void x11_listen_action(int x11_fwd_listen_fd, int *x11_fwd_fd)
{
  int fd;
  fd = accept(x11_fwd_listen_fd, NULL, NULL);
  if ((*x11_fwd_fd) >= 0)
    KERR("ONLY ONE X11 PER CLI");
  else if (fd < 0)
    KERR("%s", strerror(errno));
  else
    *x11_fwd_fd = fd;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_listen(int *x11_fwd_fd)
{
  int result = -1;
  int fd = open_listen_usock(UNIX_X11_SOCKET_PREFIX);
  if (fd < 0) 
    KERR("%s", strerror(errno));
  else 
    {
    *x11_fwd_fd = fd;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_recv_data(int x11_fd, char *buf, int len)
{
  int wlen, result = -1;
  if (x11_fd < 0)
    KERR(" ");
  else
    {
    wlen = write(x11_fd, buf, len);
    if (len != wlen)
      KERR("%d %d %s", len, wlen, strerror(errno));
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_action(int x11_fwd_fd, int sock_fd)
{
  int result = -1;
  t_msg msg;
  int len = read(x11_fwd_fd, msg.buf, MAX_MSG_LEN);
  if (len <= 0)
    KERR("%d", errno);
  else
    {
    result = 0;
    msg.type = msg_type_x11_fwd_data;
    msg.len = len;
    if (mdl_queue_write_msg(sock_fd, &msg))
      KERR("%d", msg.len);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

