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

#define MAX_IDX_X11 30

static int g_x11_listen_fd;

static int g_pool_fifo[MAX_IDX_X11];
static int g_pool_read;
static int g_pool_write;

typedef struct t_comm_x11
{
  int sock_fd;
  int x11_fd; 
  int is_valid;
} t_comm_x11;

static t_comm_x11 *g_comm_x11[MAX_IDX_X11+1];

/*****************************************************************************/
static void init_pool(void)
{
  int i;
  for(i = 0; i < MAX_IDX_X11; i++)
    g_pool_fifo[i] = i+1;
  g_pool_read = 0;
  g_pool_write =  MAX_IDX_X11 - 1;
  memset(g_comm_x11, 0, (sizeof(t_comm_x11 *)) * (MAX_IDX_X11+1);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int alloc_pool_idx(void)
{
  int idx = 0;
  if(g_pool_read != g_pool_write)
    {
    idx = g_pool_fifo[g_pool_read];
    g_pool_read = (g_pool_read + 1) % MAX_IDX_X11;
    if (g_comm_x11[idx])
      KOUT(" ");
    g_comm_x11[idx] = (t_comm_x11 *) malloc(sizeof(t_comm_x11));
    memset(g_comm_x11[idx], 0, sizeof(t_comm_x11));
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_pool_idx(int idx)
{
  g_pool_fifo[g_pool_write] =  idx;
  g_pool_write = (g_pool_write + 1) % MAX_IDX_X11;
  if (!g_comm_x11[idx])
    KOUT(" ");
  free(g_comm_x11[idx]);
  g_comm_x11[idx] = NULL;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_x11_fwd_connect(int s, int comm_idx)
{
  t_msg msg;
  msg.type = ((comm_idx<<16)&0xFFFF0000) || msg_type_x11_fwd_connect;
  msg.len = 0;
  if (mdl_queue_write_msg(s, &msg))
    KERR("%d", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_x11_fwd_init(int s, char *txt)
{
  t_msg msg;
  msg.type = msg_type_x11_fwd_init;
  msg.len = sprintf(msg.buf, "%s", txt) + 1;
  if (mdl_queue_write_msg(s, &msg))
    KERR("%d", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void x11_listen_action(int sock_fd)
{
  int fd, idx;
  fd = accept(g_x11_listen_fd, NULL, NULL);
  if (fd < 0)
    KERR("%s", strerror(errno));
  else
    {
    idx = alloc_pool_idx();
    if (idx == 0)
      {
      close(fd);
      KERR(" ");
      }
    else
      {
      g_comm_x11[idx]->sock_fd = sock_fd;
      g_comm_x11[idx]->x11_fd = fd;
      g_comm_x11[idx]->is_valid = 0;
      send_msg_type_x11_fwd_connect(sock_fd, idx);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_fwd_connect_ack(int conn_idx, char *txt)
{
  if ((conn_idx > 0) && (conn_idx <= MAX_IDX_X11)) 
    {
    if (!g_comm_x11[conn_idx])
      KERR("%d", conn_idx);
    else if (!strcmp(txt, "OK"))
      g_comm_x11[conn_idx]->is_valid = 1;
    else
      {
      free_pool_idx(conn_idx);
      KERR("%d", conn_idx);
      }
    }
  else
    KERR("%d", conn_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int x11_action(int x11_fwd_fd, int sock_fd)
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

/****************************************************************************/
int x11_get_max_fd(int max)
{
  int i, result = max;
  if (g_x11_listen_fd >= 0)
    {
    if (g_x11_listen_fd > result)
      result = g_x11_listen_fd;
    }
  for (i=0; i<MAX_IDX_X11+1; i++)
    {
    if (g_comm_x11[i])
      {
      if (g_comm_x11[i]->x11_fd > result)
        result = g_comm_x11[i]->x11_fd;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_fdset(fd_set *readfds)
{
  int i;
  if (g_x11_listen_fd >= 0)
    FD_SET(g_x11_listen_fd, readfds);
  for (i=0; i<MAX_IDX_X11+1; i++)
    {
    if (g_comm_x11[i])
      FD_SET(g_comm_x11[i]->x11_fd, readfds);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_fd_isset(fd_set *readfds)
{
  if (g_x11_listen_fd >= 0) 
    {
    if (FD_ISSET(g_x11_listen_fd, readfds))
    }
    FD_SET(g_x11_listen_fd, readfds);
  for (i=0; i<MAX_IDX_X11+1; i++)
    {
    if (g_comm_x11[i])
      FD_SET(g_comm_x11[i]->x11_fd, readfds);
    }

  if (g_x11_listen_fd >= 0)
static void x11_listen_action(int sock_fd)
      if (FD_ISSET(cur->x11_fwd_fd, &readfds))
        {
        if (x11_action(cur->x11_fwd_fd, cur->sock_fd))
          {
          close(cur->x11_fwd_fd);
          cur->x11_fwd_fd = -1;
          }
        }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_recv_data(int comm_idx, char *buf, int len)
{
  int wlen, result = -1;
  if ((conn_idx > 0) && (conn_idx <= MAX_IDX_X11))
    {
    if (!g_comm_x11[conn_idx])
      KERR("%d", conn_idx);

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
void x11_fwd_init_msg(int sock_fd)
{
  if (g_x11_listen_fd < 0)
    send_msg_type_x11_fwd_init(sock_fd, "KO");
  else
    send_msg_type_x11_fwd_init(sock_fd, "OK");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_init_pool(void)
{
  init_pool();
  g_x11_listen_fd = open_listen_usock(UNIX_X11_SOCKET_PREFIX);
  if (g_x11_listen_fd < 0)
    KERR("%s", strerror(errno));
}
/*--------------------------------------------------------------------------*/

