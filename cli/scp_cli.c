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

static int g_scp_fd;

/*****************************************************************************/
static int scp_open_snd(char *src, char *dst, char *complete_dst)
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
static int scp_open_rcv(char *src, char *dst, char *complete_dst)
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

/****************************************************************************/
static int send_msg_type_scp_open_snd(int s, char *src, char *remote_dir)
{
  t_msg msg;
  char complete_dst[MAX_PATH_LEN];
  int result = scp_open_snd(src, remote_dir, complete_dst);
  if (result == 0)
    {
    msg.type = msg_type_scp_open_snd;
    msg.len = sprintf(msg.buf, "%s %s", src, complete_dst) + 1;
    if (mdl_queue_write_msg(s, &msg))
      KERR("%d", msg.len);
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_msg_type_scp_open_rcv(int s, char *src, char *local_dir)
{
  t_msg msg;
  char complete_dst[MAX_PATH_LEN];
  int result = scp_open_rcv(src, local_dir, complete_dst);
  if (result != -1)
    {
    msg.type = msg_type_scp_open_rcv;
    msg.len = sprintf(msg.buf, "%s %s", src, complete_dst) + 1;
    if (mdl_queue_write_msg(s, &msg))
      KERR("%d", msg.len);
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_scp_data_end(int sock_fd)
{
  t_msg msg;
  msg.type = msg_type_scp_data_end;
  msg.len = 0;
  if (mdl_queue_write_msg(sock_fd, &msg))
    KERR("%d", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_scp_data_begin(int sock_fd)
{
  t_msg msg;
  msg.type = msg_type_scp_data_begin;
  msg.len = 0;
  if (mdl_queue_write_msg(sock_fd, &msg))
    KERR("%d", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int send_scp_data(int sock_fd)
{
  int result = -1;
  t_msg msg;
  int len = read(g_scp_fd, msg.buf, MAX_MSG_LEN); 
  if (len < 0)
    KOUT("%d", errno);
  if (len == MAX_MSG_LEN)
    result = 0;
  msg.type = msg_type_scp_data;
  msg.len = len;
  if (mdl_queue_write_msg(sock_fd, &msg))
    KERR("%d", msg.len);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_scp_data_end(int sock_fd)
{
  t_msg msg;
  if (g_scp_fd == -1)
    KOUT(" ");
  close(g_scp_fd);
  g_scp_fd = -1;
  msg.type = msg_type_scp_data_end_ack;
  msg.len = 0;
  if (mdl_queue_write_msg(sock_fd, &msg))
    KERR("%d", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_scp_data(t_msg *msg)
{
  int len;
  if (g_scp_fd == -1)
    KOUT(" ");
  len = write(g_scp_fd, msg->buf, msg->len);
  if ((len < 0) || (len != msg->len))
    KOUT("%d %d %d", len, msg->len, errno);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_scp_ready(int sock_fd, int type, t_msg *msg)
{
  char src[MAX_PATH_LEN];
  char complete_dst[MAX_PATH_LEN];
  if (msg->buf[0] == 'K')
    {
    printf("%s", msg->buf);
    exit(1);
    }
  if (sscanf(msg->buf, "OK %s %s", src, complete_dst) != 2)
    KOUT("%s", msg->buf);
  if (type == msg_type_scp_ready_to_rcv)
    {
    g_scp_fd = open(src, O_RDONLY);
    if (g_scp_fd == -1)
      KOUT("%s %d", src, errno);
    }
  else
    {
    g_scp_fd = open(complete_dst, O_CREAT|O_EXCL|O_WRONLY, 0655);
    if (g_scp_fd == -1)
      KOUT("%s %d", complete_dst, errno);
    send_scp_data_begin(sock_fd);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_scp_err_cb (void *ptr, char *err)
{
  (void) ptr;
  KOUT("%s", err);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_scp_msg_cb(void *ptr, int sock_fd, t_msg *msg)
{
  (void) ptr;
  switch(msg->type)
    {
    case msg_type_end_cli:
      exit(msg->buf[0]);
      break;
    case msg_type_scp_ready_to_snd:
    case msg_type_scp_ready_to_rcv:
      recv_scp_ready(sock_fd, msg->type, msg);
      break;
    case msg_type_scp_data:
      recv_scp_data(msg);
      break;
    case msg_type_scp_data_end:
      recv_scp_data_end(sock_fd);
      break;
    case msg_type_scp_data_end_ack:
      exit(0);
      break;
    default:
      KOUT("%d", msg->type);
    }
  return 0;
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void select_loop_snd_rcv(int is_snd, int sock_fd)
{
  fd_set readfds;
  fd_set writefds;
  int n;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  FD_SET(sock_fd, &readfds);
  if (mdl_queue_write_not_empty(sock_fd))
    FD_SET(sock_fd, &writefds);
  else if ((is_snd) && (g_scp_fd >= 0))
    {
    if (send_scp_data(sock_fd))
      {
      send_scp_data_end(sock_fd);
      close(g_scp_fd);
      g_scp_fd = -1;
      }
    if (mdl_queue_write_not_empty(sock_fd))
      FD_SET(sock_fd, &writefds);
    }
  n = select(sock_fd + 1, &readfds, &writefds, NULL, NULL);
  if (n <= 0)
    {
    if ((errno != EINTR) && (errno != EAGAIN))
      KOUT(" ");
    }
  else
    {
    if (FD_ISSET(sock_fd, &writefds))
      mdl_write(sock_fd);
    if (FD_ISSET(sock_fd, &readfds))
      mdl_read(NULL, sock_fd, rx_scp_msg_cb, rx_scp_err_cb);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void scp_loop(int is_snd, int sock_fd, char *src, char *dst)
{
  g_scp_fd = -1;
  if (is_snd)
    {
    if (!send_msg_type_scp_open_snd(sock_fd, src, dst))
      {
      for(;;)
        select_loop_snd_rcv(is_snd, sock_fd);
      }
    }
  else
    {
    if (!send_msg_type_scp_open_rcv(sock_fd, src, dst))
      {
      for(;;)
        select_loop_snd_rcv(is_snd, sock_fd);
      }
    }
}
/*--------------------------------------------------------------------------*/

