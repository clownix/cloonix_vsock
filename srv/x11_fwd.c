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


typedef struct t_conn_x11
{
  int x11_fd; 
  int is_valid;
} t_conn_x11;

typedef struct t_display_x11
{
  int disp_idx;
  int sock_fd;
  int display;
  int x11_listen_fd;
  int pool_fifo[MAX_IDX_X11-1];
  int pool_read;
  int pool_write;
  t_conn_x11 *conn[MAX_IDX_X11];
} t_display_x11;


static t_display_x11 g_display[MAX_DISPLAY_X11];


/****************************************************************************/
static char *x11_display_path(int val)
{
  static char x11_path[MAX_PATH_LEN];
  memset(x11_path, 0, MAX_PATH_LEN);
  snprintf(x11_path, MAX_PATH_LEN-1, UNIX_X11_SOCKET_PREFIX, val);
  return x11_path;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_display_x11 *find_disp(int sock_fd)
{
  int i;
  t_display_x11 *result = NULL;
  for (i=1; i<MAX_DISPLAY_X11; i++)
    {
    if ((sock_fd != -1) && (g_display[i].sock_fd == sock_fd))
      {
      result = &(g_display[i]);
      break;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static t_display_x11 *find_empy(int *display)
{
  int i;
  t_display_x11 *result = NULL;
  for (i=1; i<MAX_DISPLAY_X11; i++)
    {
    if (g_display[i].sock_fd == -1)
      {
      result = &(g_display[i]);
      g_display[i].disp_idx = i;
      *display = X11_ID_OFFSET + i;
      break;
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void init_pool(t_display_x11 *disp)
{
  int i;
  for(i = 0; i < MAX_IDX_X11 - 1; i++)
    disp->pool_fifo[i] = i+1;
  disp->pool_read = 0;
  disp->pool_write =  MAX_IDX_X11 - 2;
  memset(disp->conn, 0, (sizeof(t_conn_x11 *)) * MAX_IDX_X11);
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static int alloc_pool_idx(t_display_x11 *disp)
{
  int idx = 0;
  if(disp->pool_read != disp->pool_write)
    {
    idx = disp->pool_fifo[disp->pool_read];
    disp->pool_read = (disp->pool_read + 1) % (MAX_IDX_X11 - 1);
    if (disp->conn[idx])
      KOUT(" ");
    disp->conn[idx] = (t_conn_x11 *) malloc(sizeof(t_conn_x11));
    memset(disp->conn[idx], 0, sizeof(t_conn_x11));
    }
  return idx;
}
/*---------------------------------------------------------------------------*/

/*****************************************************************************/
static void free_pool_idx(t_display_x11 *disp, int idx)
{
  disp->pool_fifo[disp->pool_write] =  idx;
  disp->pool_write = (disp->pool_write + 1) % (MAX_IDX_X11 - 1);
  if (!disp->conn[idx])
    KOUT(" ");
  free(disp->conn[idx]);
  disp->conn[idx] = NULL;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_x11_connect(int s, int disp_idx, int conn_idx)
{
  t_msg msg;
  msg.type = ((disp_idx<<24)&0xFF000000) |
             ((conn_idx<<16)&0x00FF0000) |
              (msg_type_x11_connect & 0xFFFF);
  msg.len = 0;
  if (mdl_queue_write_msg(s, &msg))
    KERR("%d", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_x11_init(int s, char *txt)
{
  t_msg msg;
  msg.type = msg_type_x11_init;
  msg.len = sprintf(msg.buf, "%s", txt) + 1;
  if (mdl_queue_write_msg(s, &msg))
    KERR("%d", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void disconnect_conn_idx(t_display_x11 *disp, int conn_idx)
{
  t_msg msg;
  msg.type = ((disp->disp_idx<<24)&0xFF000000) |
             ((conn_idx<<16)&0x00FF0000) |
              (msg_type_x11_disconnect & 0xFFFF);
  msg.len = 0;
  if (mdl_queue_write_msg(disp->sock_fd, &msg))
    KERR("%d", msg.len);
  close(disp->conn[conn_idx]->x11_fd);
  free_pool_idx(disp, conn_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void x11_listen_action(t_display_x11 *disp, int disp_idx)
{
  int fd, conn_idx;
  fd = accept(disp->x11_listen_fd, NULL, NULL);
  if (fd < 0)
    KERR("%s", strerror(errno));
  else
    {
    conn_idx = alloc_pool_idx(disp);
    if (conn_idx == 0)
      {
      close(fd);
      KERR(" ");
      }
    else
      {
      disp->conn[conn_idx]->x11_fd = fd;
      disp->conn[conn_idx]->is_valid = 0;
      send_msg_type_x11_connect(disp->sock_fd, disp_idx, conn_idx);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int x11_action_fd(int disp_idx, int conn_idx, int x11_fd, int sock_fd)
{
  int result = -1;
  t_msg msg;
  int len = read(x11_fd, msg.buf, MAX_MSG_LEN);
  if (len <= 0)
    KERR("%d", errno);
  else
    {
    msg.type = ((disp_idx<<24)&0xFF000000) |
               ((conn_idx<<16)&0x00FF0000) |
                (msg_type_x11_data & 0xFFFF);
    msg.len = len;
    if (mdl_queue_write_msg(sock_fd, &msg))
      KERR("%d", msg.len);
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_connect_ack(int disp_idx, int conn_idx, char *txt)
{
  t_display_x11 *disp;
  if ((disp_idx > 0) && (disp_idx < MAX_DISPLAY_X11))
    {
    if ((conn_idx > 0) && (conn_idx < MAX_IDX_X11))
      {
      disp = &(g_display[disp_idx]);
      if (disp->sock_fd == -1)
        KERR("%d %d", disp_idx, conn_idx);
      else if (!(disp->conn[conn_idx]))
        KERR("%d %d", disp_idx, conn_idx);
      else if (!strcmp(txt, "OK"))
        disp->conn[conn_idx]->is_valid = 1;
      else
        {
        KERR("%s", txt);
        disconnect_conn_idx(disp, conn_idx);
        }
      }
    else
      KERR("%d", conn_idx);
    }
  else
    KERR("%d", disp_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_get_max_fd(int max)
{
  int i, j, result = max;
  t_display_x11 *disp;
  for (i=1; i<MAX_DISPLAY_X11; i++)
    {
    if (g_display[i].sock_fd != -1)
      {
      disp = &(g_display[i]);
      if (disp->x11_listen_fd > result)
        result = disp->x11_listen_fd;
      for (j=1; j<MAX_IDX_X11; j++)
        {
        if (disp->conn[j])
          {
          if (disp->conn[j]->x11_fd > result)
            result = disp->conn[j]->x11_fd;
          }
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_fdset(fd_set *readfds)
{
  int i, j;
  t_display_x11 *disp;
  for (i=1; i<MAX_DISPLAY_X11; i++)
    {
    if (g_display[i].sock_fd != -1)
      {
      if (g_display[i].x11_listen_fd < 0)
        KERR(" ");
      else
        {
        disp = &(g_display[i]); 
        FD_SET(disp->x11_listen_fd, readfds);
        for (j=1; j<MAX_IDX_X11; j++)
          {
          if (disp->conn[j])
            {
            if (disp->conn[j]->x11_fd < 0)
              KERR(" ");
            else
              {
              if (disp->conn[j]->is_valid)
                FD_SET(disp->conn[j]->x11_fd, readfds);
              }
            }
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_fd_isset(fd_set *readfds)
{
  int i, j;
  t_display_x11 *disp;
  for (i=1; i<MAX_DISPLAY_X11; i++)
    {
    if (g_display[i].sock_fd != -1)
      {
      if (g_display[i].x11_listen_fd < 0)
        KERR(" ");
      else
        {
        disp = &(g_display[i]);
        if (FD_ISSET(disp->x11_listen_fd, readfds))
          x11_listen_action(disp, i);
        for (j=1; j<MAX_IDX_X11; j++)
          {
          if (disp->conn[j])
            {
            if (disp->conn[j]->x11_fd < 0)
              KERR(" ");
            else
              {
              if (FD_ISSET(disp->conn[j]->x11_fd, readfds))
                {
                if (x11_action_fd(i, j, disp->conn[j]->x11_fd, disp->sock_fd))
                  {
                  KERR("%d", disp->sock_fd);
                  disconnect_conn_idx(disp, j);
                  }
                }
              }
            }
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_data_rx(int disp_idx, int conn_idx, char *buf, int len)
{
  int wlen;
  t_display_x11 *disp;
  if ((disp_idx > 0) && (disp_idx < MAX_DISPLAY_X11))
    {
    if ((conn_idx > 0) && (conn_idx < MAX_IDX_X11))
      {
      disp = &(g_display[disp_idx]);
      if (disp->sock_fd == -1)
        KERR("%d %d", disp_idx, conn_idx);
      else if (!(disp->conn[conn_idx]))
        KERR("%d %d", disp_idx, conn_idx);
      else if (!disp->conn[conn_idx]->is_valid)
        {
        KERR("%d %d", disp_idx, conn_idx);
        disconnect_conn_idx(disp, conn_idx);
        }
      else
        {
        wlen = write(disp->conn[conn_idx]->x11_fd, buf, len);
        if (len != wlen)
          {
          KERR("%d %d %s", len, wlen, strerror(errno));
          disconnect_conn_idx(disp, conn_idx);
          }
        }
      }
    else
      KERR("%d", conn_idx);
    }
  else
    KERR("%d", disp_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_init_cli_msg(int sock_fd)
{
  int fd;
  char *display_path = x11_display_path(X11_ID_OFFSET);
  fd = open_listen_usock(display_path);
  if (fd < 0)
    {
    KERR("%s", strerror(errno));
    send_msg_type_x11_init(sock_fd, "KO");
    }
  else
    {
    close(fd);
    unlink(display_path);
    send_msg_type_x11_init(sock_fd, "OK");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int x11_alloc_display(int sock_fd)
{
  int fd, display, result = 0;
  t_display_x11 *disp = find_disp(sock_fd);
  char *display_path;
  if (disp)
    KERR("%d", sock_fd);
  else
    {
    disp = find_empy(&display);
    if (!disp)
      KERR("%d", sock_fd);
    else
      {
      display_path = x11_display_path(display);
      fd = open_listen_usock(display_path);
      if (fd < 0)
        KERR("%s %s", display_path, strerror(errno));
      else
        {
        disp->sock_fd = sock_fd;
        disp->display = display;
        disp->x11_listen_fd = fd;
        init_pool(disp);
        result = display;
        }
      }
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_free_display(int sock_fd)
{
  int i;
  t_display_x11 *disp = find_disp(sock_fd);
  char *display_path;
  if (disp)
    {
    close (disp->x11_listen_fd);
    for (i=1; i<MAX_IDX_X11; i++)
      {
      if (disp->conn[i])
        {
        close (disp->conn[i]->x11_fd); 
        free(disp->conn[i]);
        }
      }
    display_path = x11_display_path(disp->display);
    unlink(display_path);
    memset(disp, 0, sizeof(t_display_x11));
    disp->sock_fd = -1;
    }
  else
    KERR("%d", sock_fd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_init_display(void)
{
  int i;
  memset(g_display, 0, MAX_DISPLAY_X11 * sizeof(t_display_x11));
  for (i=0; i<MAX_DISPLAY_X11; i++)
    {
    g_display[i].sock_fd = -1;
    }
}
/*--------------------------------------------------------------------------*/
