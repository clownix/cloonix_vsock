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

/*****************************************************************************/
static int scp_rx_open_snd(char *src, char *complete_dst, char *resp)
{
  struct stat sb;
  char dupdst[MAX_PATH_LEN];
  char *dir_path;
  int result = -1;
  memset(dupdst, 0, MAX_PATH_LEN);
  memset(resp, 0, MAX_PATH_LEN);
  snprintf(dupdst, MAX_PATH_LEN-1, "%s", complete_dst); 
  if (stat(complete_dst, &sb) == -1)
    {
    dir_path = dirname(dupdst);
    if (stat(dir_path, &sb) != -1)
      {
      snprintf(resp, MAX_PATH_LEN-1, "OK %s %s", src, complete_dst); 
      result = 0;
      }
    else 
      snprintf(resp, MAX_PATH_LEN-1, 
               "KO distant %s directory does not exist", dir_path);
    }
  else
    snprintf(resp, MAX_PATH_LEN-1, 
             "KO distant %s exists already\n", complete_dst);
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
    snprintf(resp, MAX_PATH_LEN-1, "KO distant %s does not exist\n", src);
  else
    {
    snprintf(resp, MAX_PATH_LEN-1, "OK %s %s", src, complete_dst); 
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


/*****************************************************************************/
static void scp_cli_snd(int *fd, char *src, char *complete_dst, char *resp)
{
  if (!scp_rx_open_snd(src, complete_dst, resp))
    {
    *fd = open(complete_dst, O_CREAT|O_EXCL|O_WRONLY, 0655);
    if (*fd == -1)
      {
      resp[0] = 'K';
      resp[1] = 'O';
      KERR("%s %d", complete_dst, errno);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static void scp_cli_rcv(int *fd, char *src, char *complete_dst, char *resp)
{ 
  if (!scp_rx_open_rcv(src, complete_dst, resp))
    *fd = open(src, O_RDONLY);
    if (*fd == -1)
      {
      resp[0] = 'K';
      resp[1] = 'O';
      KERR("%s %d", src, errno);
      }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_resp_cli(int type, int sock_fd, char *resp)
{
  t_msg msg;
  msg.type = type;
  msg.len = sprintf(msg.buf, "%s", resp) + 1;
  if (mdl_queue_write_msg(sock_fd, &msg))
    KERR("%d", msg.len);
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
static int send_scp_data(int scp_fd, int sock_fd)
{
  int result = -1;
  t_msg msg;
  int len = read(scp_fd, msg.buf, MAX_MSG_LEN);
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
void recv_scp_data_end(int scp_fd, int sock_fd)
{
  t_msg msg;
  if (scp_fd == -1)
    KERR(" ");
  close(scp_fd);
  scp_fd = -1;
  msg.type = msg_type_scp_data_end_ack;
  msg.len = 0;
  if (mdl_queue_write_msg(sock_fd, &msg))
    KERR("%d", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void recv_scp_data(int scp_fd, t_msg *msg)
{
  int len;
  if (scp_fd == -1)
    KERR(" ");
  else
    {
    len = write(scp_fd, msg->buf, msg->len);
    if ((len < 0) || (len != msg->len))
      KERR("%d %d %d", len, msg->len, errno);
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int recv_scp_open(int type, int sock_fd, int *cli_scp_fd, char *buf)
{
  int result = 0;
  char resp[MAX_PATH_LEN];
  char src[MAX_PATH_LEN];
  char complete_dst[MAX_PATH_LEN];
  if (sscanf(buf, "%s %s", src, complete_dst) != 2)
    {
    KERR("%s",  buf);
    result = -1;
    }
  else
    {
    if (type == msg_type_scp_open_snd)
      {
      scp_cli_snd(cli_scp_fd, src, complete_dst, resp);
      send_resp_cli(msg_type_scp_ready_to_rcv, sock_fd, resp);
      }
    else
      {
      scp_cli_rcv(cli_scp_fd, src, complete_dst, resp);
      send_resp_cli(msg_type_scp_ready_to_snd, sock_fd, resp);
      }
    }
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
int send_scp_to_cli(int scp_fd, int sock_fd)
{
  int result = 0;
  if (send_scp_data(scp_fd, sock_fd))
    {
    send_scp_data_end(sock_fd);
    result = -1;
    }
  return result;
}
/*--------------------------------------------------------------------------*/


