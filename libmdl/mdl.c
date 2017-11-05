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
#include <unistd.h>
#include "mdl.h"

#define MAX_FD_NUM 100
#define MAX_QUEUE_MDL_RX 10000
#define MAX_QUEUE_MDL_TX 100000

typedef struct t_mdl
{
  char bufrx[MAX_QUEUE_MDL_RX];
  int  rxoffst;
  char buftx[MAX_QUEUE_MDL_TX];
  int  txoffst_start;
  int  txoffst_end;
} t_mdl;

static t_mdl *g_mdl[MAX_FD_NUM];

/****************************************************************************/
int mdl_parse_val(const char *str_val)
{
  int result = -1;
  char *end = NULL;
  long val = strtol(str_val, &end, 10);
  if ((str_val != end) && (*end == '\0'))
    result = (int) val;
  else
    KOUT("%s", str_val);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mdl_open(int s)
{
  int result = -1;
  if ((s < MAX_FD_NUM) && (!g_mdl[s]))
    {
    g_mdl[s] = (t_mdl *) malloc(sizeof(t_mdl));
    memset(g_mdl[s], 0, sizeof(t_mdl));
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mdl_close(int s)
{
  int result = -1;
  if ((s < MAX_FD_NUM) && (g_mdl[s]))
    {
    free(g_mdl[s]);
    g_mdl[s] = NULL;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mdl_write(int s)
{
  t_mdl *mdl = g_mdl[s];
  int len;
  if ((mdl) && (mdl->txoffst_end - mdl->txoffst_start))
    {
    len = write(s, mdl->buftx, mdl->txoffst_end - mdl->txoffst_start);
    if (len > 0)
      {
      mdl->txoffst_start += len;
      if (mdl->txoffst_start == mdl->txoffst_end)
        {
        mdl->txoffst_start = 0;
        mdl->txoffst_end = 0;
        }
      }
    }
  else if (mdl)
    KERR("%d %d", mdl->txoffst_end, mdl->txoffst_start);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mdl_queue_write_saturated(int s)
{
  int result = 0;
  t_mdl *mdl = g_mdl[s];
  if ((mdl) && (mdl->txoffst_end > MAX_QUEUE_MDL_TX/2))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mdl_queue_write_not_empty(int s)
{
  int result = 0;
  t_mdl *mdl = g_mdl[s];
  if ((mdl) && (mdl->txoffst_end - mdl->txoffst_start))
    result = 1;
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int mdl_queue_write(int s, t_msg *msg)
{
  int result = -1;
  int len = msg->len + g_msg_header_len;
  t_mdl *mdl = g_mdl[s];
  if (!mdl) 
    KERR(" ");
  else if (len >= MAX_QUEUE_MDL_TX - mdl->txoffst_end)
    KERR("%d %d %d", len, MAX_QUEUE_MDL_TX, mdl->txoffst_end);
  else
    {
    msg->cafe = 0xCAFEDECA;
    memcpy(mdl->buftx + mdl->txoffst_end, msg, len);
    mdl->txoffst_end += len;
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void do_cb(t_mdl *mdl, void *ptr, t_rx_msg_cb rx_cb, t_rx_err_cb err_cb)
{
  t_msg *msg = (t_msg *) mdl->bufrx;
  char *tmp;
  int done, rxoffst = mdl->rxoffst;
  if (rxoffst >= g_msg_header_len)
    {
    if (msg->cafe != 0xCAFEDECA)
      KOUT("header id is %X", msg->cafe);
    if ((msg->len < 0) || (msg->len > MAX_MSG_LEN))
      KOUT("header len: %d", msg->len); 
    if (rxoffst >= msg->len + g_msg_header_len)
      {
      do
        {
        if (msg->cafe != 0xCAFEDECA)
          KOUT("header id is %X", msg->cafe);
        rx_cb(ptr, msg);
        done = msg->len + g_msg_header_len;
        msg = (t_msg *)((char *)msg + done);
        rxoffst -= done;
        } while((rxoffst >= g_msg_header_len) &&
                (rxoffst >= msg->len + g_msg_header_len));
      }
    if (rxoffst)
      {
      if (rxoffst >= g_msg_header_len)
        {
        if (msg->cafe != 0xCAFEDECA)
          KOUT("header id is %X", msg->cafe);
        if ((msg->len < 0) || (msg->len > MAX_MSG_LEN))
          KOUT("header len: %d", msg->len); 
        }
      tmp = (char *) malloc(rxoffst);
      memcpy(tmp, msg, rxoffst);
      memcpy(mdl->bufrx, tmp, rxoffst);
      free(tmp);
      if (rxoffst >= g_msg_header_len)
        {
        msg = (t_msg *) mdl->bufrx;
        if (msg->cafe != 0xCAFEDECA)
          KOUT("header id is %X", msg->cafe);
        if ((msg->len < 0) || (msg->len > MAX_MSG_LEN))
          KOUT("header len: %d", msg->len);
        }
      mdl->rxoffst = rxoffst;
      }
    else
      mdl->rxoffst = 0;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
void mdl_read(void *ptr, int s, t_rx_msg_cb rx_cb, t_rx_err_cb err_cb)
{
  char err[MAX_PATH_LEN];
  int len, wlen, max_to_read;
  t_mdl *mdl = g_mdl[s];
  if (!mdl) 
    err_cb(ptr, "Context mdl not found");
  else
    {
    if (MAX_QUEUE_MDL_RX - mdl->rxoffst > 0)
      {
      max_to_read = MAX_QUEUE_MDL_RX - mdl->rxoffst;
      len = read(s, mdl->bufrx + mdl->rxoffst, max_to_read);
      if (len == 0)
        err_cb(ptr, "read len is 0");
      else if (len < 0)
        {
        if ((errno != EAGAIN) && (errno != EINTR))
          {
          snprintf(err, MAX_PATH_LEN-1, "read error: %s", strerror(errno));
          err[MAX_PATH_LEN-1] = 0;
          err_cb(ptr, err);
          }
        }
      else
        {
        mdl->rxoffst += len;
        do_cb(mdl, ptr, rx_cb, err_cb);
        }
      }
    else
      {
      KERR("%d", mdl->rxoffst);
      do_cb(mdl, ptr, rx_cb, err_cb);
      }
    }
}
/*--------------------------------------------------------------------------*/


