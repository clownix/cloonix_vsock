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
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pty.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/vm_sockets.h>

#include "mdl.h"

static struct termios g_orig_term;
static struct termios g_cur_term;
static int g_win_chg_write_fd;
static char g_sock_path[MAX_PATH_LEN];

/****************************************************************************/
static void restore_term(void)
{
  tcsetattr(0, TCSADRAIN, &g_orig_term);
  printf("\033[?25h");
  fflush(stdout);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cli_exit(int sig)
{
  KOUT("%d", sig);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void win_size_chg(int sig)
{
  char buf[1];
  buf[0] = 'w';
  write(g_win_chg_write_fd, buf, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_open_bash(int s, char *cmd)
{
  t_msg msg;
  if (cmd)
    {
    msg.type = msg_type_open_bashcmd;
    msg.len = strlen(cmd) + 1;
    memcpy(msg.buf, cmd, msg.len);
    }
  else
    {
    msg.type = msg_type_open_bash;
    msg.len = 0;
    }
  mdl_queue_write(s, &msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_win_size(int s)
{
  t_msg msg;
  msg.type = msg_type_win_size;
  msg.len = sizeof(struct winsize);
  ioctl(0, TIOCGWINSZ, msg.buf);
  mdl_queue_write(s, &msg);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_kill(int s)
{
  t_msg msg;
  msg.type = msg_type_kill;
  msg.len = 0;
  mdl_queue_write(s, &msg);
} 
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void config_sigs(void)
{
  if (signal(SIGPIPE, cli_exit))
    KOUT("%d", errno);
  if (signal(SIGHUP, cli_exit))
    KOUT("%d", errno);
  if (signal(SIGTERM, cli_exit))
    KOUT("%d", errno);
  if (signal(SIGINT, cli_exit))
    KOUT("%d", errno);
  if (signal(SIGQUIT, cli_exit))
    KOUT("%d", errno);
  if (signal(SIGWINCH, win_size_chg))
    KOUT("%d", errno);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void config_term(void)
{
  if (atexit(restore_term))
    KOUT("%d", errno);
  g_cur_term = g_orig_term;
  g_cur_term.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
  g_cur_term.c_iflag &= ~(IXON|IXOFF);
  g_cur_term.c_oflag &= ~(OPOST);
  g_cur_term.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
  g_cur_term.c_cflag &= ~(CSIZE|PARENB);
  g_cur_term.c_cflag |= CS8;
  g_cur_term.c_cc[VMIN] = 1;
  g_cur_term.c_cc[VTIME] = 0;
  tcsetattr(0, TCSADRAIN, &g_cur_term);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int connect_unix_socket(char *name)
{
  int s, result = -1;
  struct sockaddr_un sockun;
  s = socket(PF_UNIX, SOCK_STREAM, 0);
  if (s >= 0)
    {
    sockun.sun_family = AF_UNIX;
    strcpy(sockun.sun_path, name);
    if (connect(s, (struct sockaddr*)&sockun, sizeof(sockun)) == 0)
      result = s;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int connect_vsock(int cid, int port)
{
  int s, result = -1;
  struct sockaddr_vm vsock;
  memset(&vsock, 0, sizeof(vsock));
  s = socket(AF_VSOCK, SOCK_STREAM, 0);
  if (s >= 0)
    {
    vsock.svm_family = AF_VSOCK,
    vsock.svm_cid = cid;
    vsock.svm_port = port;
    if (connect(s, (struct sockaddr*)&vsock, sizeof(vsock)) == 0)
      result = s;
    else
      KOUT("Connect error cid: %d port: %d\n", cid, port);
    }
  else
    KOUT("socket AF_VSOCK SOCK_STREAM create error");
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void action_win_chg(int sock_fd, int win_chg_read_fd)
{
  int len;
  char buf[16];
  len = read(win_chg_read_fd, buf, sizeof(buf));
  if ((len != 1) || (buf[0] != 'w'))
    KOUT("%d %s", len, buf);
  send_msg_type_win_size(sock_fd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_err_cb (void *ptr, char *err)
{
  (void) ptr;
  KOUT("%s", err);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_msg_cb(void *ptr, t_msg *msg)
{
  int len;
  (void) ptr;
  if (msg->type == msg_type_data2cli) 
    {
    len = write(1, msg->buf, msg->len);
    if (len != msg->len)
      KOUT("%d %d %s", len, msg->len, strerror(errno)); 
    }
  else if (msg->type == msg_type_end2cli)
    {
    exit(msg->buf[0]);
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void action_input_rx(int sock_fd)
{
  int len;
  t_msg msg;
  len = read(0, msg.buf, sizeof(msg.buf));
  if (len <= 0)
    KOUT(" ");
  msg.type = msg_type_data;
  msg.len = len;
  mdl_queue_write(sock_fd, &msg);
}

/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void select_loop(int sock_fd, int win_chg_read_fd, int max)
{
  fd_set readfds;
  fd_set writefds;
  int n;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  if (!mdl_queue_write_saturated(sock_fd))
    {
    FD_SET(0, &readfds);
    FD_SET(win_chg_read_fd, &readfds);
    FD_SET(sock_fd, &readfds);
    if (mdl_queue_write_not_empty(sock_fd))
      FD_SET(sock_fd, &writefds);
    }
  else
    FD_SET(sock_fd, &writefds);
  n = select(max + 1, &readfds, &writefds, NULL, NULL);
  if (n <= 0)
    {
    if ((errno != EINTR) && (errno != EAGAIN))
      KOUT(" ");
    }
  else
    {
    if (FD_ISSET(sock_fd, &writefds))
      mdl_write(sock_fd);
    if (FD_ISSET(win_chg_read_fd, &readfds))
      action_win_chg(sock_fd, win_chg_read_fd);
    if (FD_ISSET(0, &readfds))
      action_input_rx(sock_fd);
    if (FD_ISSET(sock_fd, &readfds))
      mdl_read(NULL, sock_fd, rx_msg_cb, rx_err_cb);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void loop_cli(int sock_fd, char *cmd, int kill)
{
  int pipe_fd[2];
  int max;
  if (pipe(pipe_fd) < 0)
    printf("Error pipe\n");
  else if (tcgetattr(0, &g_orig_term) < 0)
    printf("Error No pty\n");
  else if (mdl_open(sock_fd)) 
    printf("Error too big fd\n");
  else
    {
    g_win_chg_write_fd = pipe_fd[1];
    config_term();
    config_sigs();
    send_msg_type_open_bash(sock_fd, cmd);
    send_msg_type_win_size(sock_fd);
    if (kill)
      send_msg_type_kill(sock_fd);
    max = sock_fd;
    if (pipe_fd[0] > max)
      max = pipe_fd[0];
    for(;;)
      {
      select_loop(sock_fd, pipe_fd[0], max);
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void usage(char *name)
{
  printf("\n%s -u <unix_sock>", name);
  printf("\n%s -u <unix_sock> -c \"<cmd>\"", name);
  printf("\n%s -v <vsock_cid> <vsock_port>", name);
  printf("\n%s -v <vsock_cid> <vsock_port> -c \"<cmd>\"\n", name);
  exit(1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void main_usock(int argc, char **argv)
{
  char *cmd, unix_sock_path[MAX_PATH_LEN];
  int sock_fd;
  memset(unix_sock_path, 0, MAX_PATH_LEN);
  strncpy(unix_sock_path, argv[2], MAX_PATH_LEN-1);
  sock_fd = connect_unix_socket(unix_sock_path);
  if (sock_fd < 0)
    KOUT(" %s: %s\n", unix_sock_path, strerror(errno));
  if (argc == 3)
    {
    loop_cli(sock_fd, NULL, 0);
    }
  else if ((argc == 5) && !strcmp(argv[3], "-c"))
    {
    cmd = argv[4];
    if (cmd && (strlen(cmd) >= MAX_MSG_LEN))
      KOUT("%d %s", strlen(cmd), cmd);
    loop_cli(sock_fd, cmd, 0);
    }
  else if ((argc == 4) && !strcmp(argv[3], "-k"))
    {
    loop_cli(sock_fd, NULL, 1);
    }
  else
    {
    usage(argv[0]);
    }
}
/*--------------------------------------------------------------------------*/


/****************************************************************************/
static void main_vsock(int argc, char **argv)
{
  char *cmd;
  int sock_fd, vsock_cid, vsock_port;
  if (argc < 4)
      usage(argv[0]);
  vsock_cid = mdl_parse_val(argv[2]);
  vsock_port = mdl_parse_val(argv[3]);
  sock_fd = connect_vsock(vsock_cid, vsock_port);
  if (argc == 4)
    {
    loop_cli(sock_fd, NULL, 0);
    }
  else if ((argc == 6) && !strcmp(argv[4], "-c"))
    {
    cmd = argv[5];
    if (cmd && (strlen(cmd) >= MAX_MSG_LEN))
      KOUT("%d %s", strlen(cmd), cmd);
    loop_cli(sock_fd, cmd, 0);
    }
  else if ((argc == 5) && !strcmp(argv[4], "-k"))
    {
    loop_cli(sock_fd, NULL, 1);
    }
  else
    {
    usage(argv[0]);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char **argv)
{
  if (argc < 3)
    usage(argv[0]);
  if (!strcmp(argv[1], "-u"))
    main_usock(argc, argv);
  else if (!strcmp(argv[1], "-v"))
    main_vsock(argc, argv);
  else
    usage(argv[0]);
  return 0;
}
/*--------------------------------------------------------------------------*/
