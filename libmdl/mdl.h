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

#define MAX_MSG_LEN 4096
#define MAX_PATH_LEN 300

#include <string.h>
#include <syslog.h>
#include <libgen.h>


#define KERR(format, a...)                                     \
 do {                                                          \
    syslog(LOG_ERR | LOG_USER, "WARN %s line:%d " format "\n", \
           basename(__FILE__), __LINE__, ## a);            \
    } while (0)

#define KOUT(format, a...)                                     \
 do {                                                          \
    printf("KILL %s line:%d " format "\n", \
           basename(__FILE__), __LINE__, ## a);            \
    syslog(LOG_ERR | LOG_USER, "KILL %s line:%d " format "\n", \
           basename(__FILE__), __LINE__, ## a);            \
    exit(-1);                                                  \
    } while (0)

enum
{
  msg_type_data = 7,
  msg_type_open_bash,
  msg_type_open_bashcmd,
  msg_type_win_size,
  msg_type_kill,
  msg_type_data2cli,
  msg_type_end2cli,
};
typedef struct t_msg
{
  long cafe;
  long type;
  long len;
  char buf[MAX_MSG_LEN];
} t_msg;

static const int g_msg_header_len = (3*sizeof(long));

int mdl_parse_val(const char *str_val);
int ip_string_to_int (int *inet_addr, char *ip_string);

int mdl_queue_write(int s, t_msg *msg);
int mdl_queue_write_not_empty(int s);
int mdl_queue_write_saturated(int s);
void mdl_write(int s);


typedef void (*t_rx_msg_cb)(void *ptr, t_msg *msg);
typedef void (*t_rx_err_cb)(void *ptr, char *err);
void mdl_read(void *ptr, int s, t_rx_msg_cb rx, t_rx_err_cb err);
int mdl_open(int s);
int mdl_close(int s);

