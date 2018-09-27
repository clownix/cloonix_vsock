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
#include "mdl.h"
#include "x11_fwd.h"

static char g_x11_path[MAX_PATH_LEN];
static int g_x11_port;
static char g_x11_magic[MAX_PATH_LEN];

typedef struct t_conn_cli_x11
{
  int disp_idx;
  int x11_fd;
  int sock_fd;
} t_conn_cli_x11;

static t_conn_cli_x11 *g_conn[MAX_IDX_X11];

int connect_isock(char *ip, int port);

/****************************************************************************/
static void send_msg_type_x11_init(int s)
{
  t_msg msg;
  msg.type = msg_type_x11_init;
  msg.len = 0;
  if (mdl_queue_write_msg(s, &msg))
    KERR("%ld", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_x11_connect_ack(int s, int disp_idx,
                                          int conn_idx, char *txt)
{
  t_msg msg;
  msg.type = ((disp_idx<<24)&0xFF000000) |
             ((conn_idx<<16)&0x00FF0000) | 
              (msg_type_x11_connect_ack & 0xFFFF);
  msg.len = sprintf(msg.buf, "%s", txt) + 1;
  if (mdl_queue_write_msg(s, &msg))
    KERR("%ld", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_data_rx(int disp_idx, int conn_idx, char *buf, int len)
{
  int wlen;
  if ((disp_idx <= 0) || (disp_idx >= MAX_DISPLAY_X11))
    KOUT("%d %d %d", disp_idx, conn_idx, len);
  if ((conn_idx <= 0) || (conn_idx >= MAX_IDX_X11))
    KOUT("%d %d %d", disp_idx, conn_idx, len);
  if (g_conn[conn_idx])
    {
    if (g_conn[conn_idx]->disp_idx != disp_idx)
      KERR("%d %d", g_conn[conn_idx]->disp_idx, disp_idx);
    else
      {
      wlen = write(g_conn[conn_idx]->x11_fd, buf, len);
      if (len != wlen)
        KERR("%d %d %s", len, wlen, strerror(errno));
      }
    }
  else
    KERR("%d %d %d", disp_idx, conn_idx, len);
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
      KERR("%ld", msg.len);
    else
      result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static int get_xauth_magic(char *display, char *err)
{
  int result = -1;
  char cmd[MAX_PATH_LEN];
  char buf[MAX_PATH_LEN];
  char *ptr, *end;
  FILE *fp;
  memset(err, 0, MAX_PATH_LEN);
  memset(cmd, 0, MAX_PATH_LEN);
  memset(buf, 0, MAX_PATH_LEN);
  snprintf(cmd, MAX_PATH_LEN-1, "xauth list %s", display);
  fp = popen(cmd, "r");
  if (fp == NULL)
    snprintf(err, MAX_PATH_LEN-1, "%s, popen", cmd);
  else
    {
    if (!fgets(buf, MAX_PATH_LEN-1, fp))
      snprintf(err, MAX_PATH_LEN-1, "%s, fgets", cmd);
    else
      {
      if (!strlen(buf)) 
        snprintf(err, MAX_PATH_LEN-1, "%s, strlen", cmd);
      else
        {
        ptr = strstr(buf, "MIT-MAGIC-COOKIE-1");
        if (!ptr)
          snprintf(err, MAX_PATH_LEN-1, "%s, MIT-MAGIC", buf);
        else
          {
          ptr += strlen("MIT-MAGIC-COOKIE-1");
          ptr += strspn(ptr, " \t");
          if (!strlen(ptr) > 1)
            snprintf(err, MAX_PATH_LEN-1, "%s, MIT-MAGIC strlen1", buf);
          else
            {
            end = strchr(ptr, '\r');
            if (end) 
              *end = 0;
            end = strchr(ptr, '\n');
            if (end) 
              *end = 0;
            if (!strlen(ptr) > 1)
              snprintf(err, MAX_PATH_LEN-1, "%s, MIT-MAGIC strlen2", buf);
            else
              {
              memset(g_x11_magic, 0, MAX_PATH_LEN);
              strncpy(g_x11_magic, ptr, MAX_PATH_LEN-1);
              result = 0;
              }
            }
          }
        }
      }
    if (pclose(fp))
      snprintf(err, MAX_PATH_LEN-1, "%s, pclose", cmd);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void local_disconnect(int conn_idx)
{
  if (g_conn[conn_idx])
    {
    close(g_conn[conn_idx]->x11_fd);
    free(g_conn[conn_idx]);
    g_conn[conn_idx] = NULL;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_fdset(fd_set *readfds, int *max)
{
  int i;
  for (i=1; i<MAX_IDX_X11; i++)
    {
    if (g_conn[i])
      {
      if (g_conn[i]->x11_fd > *max)
        *max = g_conn[i]->x11_fd;
      FD_SET(g_conn[i]->x11_fd, readfds);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_fd_isset(fd_set *readfds)
{
  int i;
  for (i=1; i<MAX_IDX_X11; i++)
    {
    if (g_conn[i])
      {
      if (FD_ISSET(g_conn[i]->x11_fd, readfds))
        {
        if (x11_action_fd(g_conn[i]->disp_idx, i, 
                          g_conn[i]->x11_fd, g_conn[i]->sock_fd))
          {
          KERR("%d", g_conn[i]->sock_fd);
          local_disconnect(i);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_disconnect(int disp_idx, int conn_idx)
{
  if ((disp_idx <= 0) || (disp_idx >= MAX_DISPLAY_X11))
    KOUT("%d %d", disp_idx, conn_idx);
  if ((conn_idx <= 0) || (conn_idx >= MAX_IDX_X11))
    KOUT("%d %d", disp_idx, conn_idx);
  if (!g_conn[conn_idx])
    KERR("%d %d", disp_idx, conn_idx);
  local_disconnect(conn_idx);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void create_conn_and_ack(int fd, int sock_fd,int disp_idx,int conn_idx)
{
  char msg[MAX_PATH_LEN];
  g_conn[conn_idx] = (t_conn_cli_x11 *) malloc(sizeof(t_conn_cli_x11));
  memset(g_conn[conn_idx], 0, sizeof(t_conn_cli_x11));
  g_conn[conn_idx]->disp_idx = disp_idx;
  g_conn[conn_idx]->x11_fd   = fd;
  g_conn[conn_idx]->sock_fd  = sock_fd;
  memset(msg, 0, MAX_PATH_LEN);
  snprintf(msg, MAX_PATH_LEN-1, "OK %s", g_x11_magic);
  send_msg_type_x11_connect_ack(sock_fd, disp_idx, conn_idx, msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_connect(int disp_idx, int conn_idx, int sock_fd)
{
  int fd = -1;
  if ((disp_idx <= 0) || (disp_idx >= MAX_DISPLAY_X11))
    KOUT("%d %d %d", disp_idx, conn_idx, sock_fd);
  if ((conn_idx <= 0) || (conn_idx >= MAX_IDX_X11))
    KOUT("%d %d %d", disp_idx, conn_idx, sock_fd);
  if (strlen(g_x11_path))
    { 
    fd = connect_usock(g_x11_path);
    if (fd < 0)
      {
      KERR("%s", strerror(errno));
      send_msg_type_x11_connect_ack(sock_fd, disp_idx, conn_idx, "KO");
      memset(g_x11_path, 0, MAX_PATH_LEN);
      }
    else
      {
      create_conn_and_ack(fd, sock_fd, disp_idx, conn_idx);
      }
    }
  else if (g_x11_port)
    {
    fd = connect_isock("127.0.0.1", g_x11_port);
    if (fd >= 0)
      {
      create_conn_and_ack(fd, sock_fd, disp_idx, conn_idx);
      }
    else
      {
      KERR("Cannot open X11 ssh pathway port: %d", g_x11_port);
      send_msg_type_x11_connect_ack(sock_fd, disp_idx, conn_idx, "KO");
      g_x11_port = 0;
      }
    }
  else
    {
    KERR("No display unix path");
    send_msg_type_x11_connect_ack(sock_fd, disp_idx, conn_idx, "KO");
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void x11_init(int sock_fd)
{
  int fd, val;
  char err[MAX_PATH_LEN];
  char *display = getenv("DISPLAY");
  g_x11_port = 0;
  memset(g_x11_path, 0, MAX_PATH_LEN);
  memset(g_x11_magic, 0, MAX_PATH_LEN);
  memset(g_conn, 0, MAX_IDX_X11 * sizeof(t_conn_cli_x11 *));
  if (!display)
    KERR("X11 no DISPLAY");
  else
    {
    if ((sscanf(display, ":%d", &val) == 1) ||
        (sscanf(display, "unix:%d.0", &val) == 1))
      {
      snprintf(g_x11_path, MAX_PATH_LEN-1, UNIX_X11_SOCKET_PREFIX, val);
      if (access(g_x11_path, F_OK))
        {
        KERR("X11 socket not found: %s", g_x11_path);
        memset(g_x11_path, 0, MAX_PATH_LEN);
        }
      else
        {
        if (sscanf(display, "unix:%d.0", &val) == 1)
          get_xauth_magic(display, err);
        send_msg_type_x11_init(sock_fd);
        }
      }
    else if (sscanf(display, "localhost:%d.0", &val) == 1)
      {
      g_x11_port = 6000+val;
      fd = connect_isock("127.0.0.1", g_x11_port);
      if (fd < 0)
        {
        KERR("X11 port not working: %d", g_x11_port);
        g_x11_port = 0;
        }
      else
        {
        close(fd);
        if (get_xauth_magic(display, err))
          {
          KERR("Cannot get X11 ssh magic cookie for %s %s", display, err);
          g_x11_port = 0;
          }
        else
          {
          send_msg_type_x11_init(sock_fd);
          }
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

