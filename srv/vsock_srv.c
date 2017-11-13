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
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <linux/vm_sockets.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "mdl.h"
#include "scp_srv.h"
#include "x11_fwd.h"
#define MAX_FD_IDENT 100

enum {
  cli_type_bash = 0,
  cli_type_cmd,
  cli_type_scp_reg,
  cli_type_scp_dir,
};

typedef struct t_cli
{
  int pid;
  int sock_fd;
  int pty_master_fd;
  int cli_type;
  int x11_fwd_listen_fd;
  int x11_fwd_fd;
  int scp_fd;
  int scp_begin;
  char scp_path[MAX_PATH_LEN];
  struct t_cli *prev;
  struct t_cli *next;
}t_cli;

static t_cli *g_cli_head;
static int g_nb_cli;
static int g_sig_write_fd;


/****************************************************************************/
static void cli_alloc(int fd)
{
  t_cli *cli = (t_cli *) malloc(sizeof(t_cli));
  memset(cli, 0, sizeof(t_cli));
  g_nb_cli += 1;
  cli->sock_fd = fd; 
  cli->pty_master_fd = -1;
  cli->x11_fwd_listen_fd = -1;
  cli->x11_fwd_fd = -1;
  cli->scp_fd = -1;
  cli->scp_begin = 0;
  if (g_cli_head)
    g_cli_head->prev = cli;
  cli->next = g_cli_head;
  g_cli_head = cli;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cli_free(t_cli *cli)
{
  g_nb_cli -= 1;
  if (cli->prev)
    cli->prev->next = cli->next;
  if (cli->next)
    cli->next->prev = cli->prev;
  if (cli == g_cli_head)
    g_cli_head = cli->next;
  close(cli->sock_fd);
  mdl_close(cli->sock_fd);
  if (cli->pty_master_fd != -1)
    {
    close(cli->pty_master_fd);
    mdl_close(cli->pty_master_fd);
    }
  free(cli);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void nonblocking(int fd)
{
  int flags;
  flags = fcntl(fd, F_GETFL);
  if (flags < 0)
    KOUT(" ");
  if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0)
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void bin_bash_pty(t_cli *cli, char *cmd)
{
  char *argv[5];
  char *env[] = {"PATH=/usr/sbin:/usr/bin:/sbin:/bin",UNIX_X11_DISPLAY,NULL};
  cli->pid = forkpty(&(cli->pty_master_fd), NULL, NULL, NULL);
  if (cli->pid == 0)
    {
    if (cmd)
      {
      argv[0] = "/bin/bash";
      argv[1] = "-c";
      argv[2] = (char *) cmd;
      argv[3] = NULL;
      } 
    else
      {
      argv[0] = "/bin/bash";
      argv[1] = NULL;
      } 
    execve("/bin/bash", argv, env);
    }
  else if (cli->pid < 0)
    KOUT("%s", strerror(errno));
  else if (cli->pty_master_fd < 0)
    KOUT("%s", strerror(errno));
  if (mdl_open(cli->pty_master_fd))
    KOUT("%d", cli->pty_master_fd);
  nonblocking(cli->pty_master_fd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int open_listen_usock(char *name)
{
  int s, result = -1;
  struct sockaddr_un sockun;
  unlink(name);
  s = socket(PF_UNIX, SOCK_STREAM, 0);
  if (s >= 0)
    {
    sockun.sun_family = AF_UNIX;
    strcpy(sockun.sun_path, name);
    if (bind(s, (struct sockaddr*)&sockun, sizeof(sockun)) >= 0)
      {
      if (listen(s, 40) == 0)
        {
        nonblocking(s);
        result = s;
        }
      }
    if (result == -1)
      close(s);
    }
  return s;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int open_listen_vsock(int vsock_port)
{
  int s;
  struct sockaddr_vm vsock;
  memset(&vsock, 0, sizeof(vsock));
  s = socket(AF_VSOCK, SOCK_STREAM, 0);
  if (s < 0)
    KOUT("%s", strerror(errno));
  vsock.svm_family = AF_VSOCK;
  vsock.svm_cid = VMADDR_CID_ANY;
  vsock.svm_port = vsock_port;
  if (bind(s, (struct sockaddr*)&vsock, sizeof(vsock)) < 0)
    KOUT("%s", strerror(errno));
  if (listen(s, 40) < 0)
    KOUT("%s", strerror(errno));
  nonblocking(s);
  return s;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int open_listen_isock(int isock_port)
{
  int s, optval=1;
  struct sockaddr_in isock;
  memset(&isock, 0, sizeof(isock));
  s = socket(AF_INET, SOCK_STREAM, 0);
  if (s < 0)
    KOUT("%s", strerror(errno));
  isock.sin_family = AF_INET;
  isock.sin_addr.s_addr = htonl(INADDR_ANY);
  isock.sin_port = htons(isock_port);
  if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    KOUT(" ");
  if (bind(s, (struct sockaddr*)&isock, sizeof(isock)) < 0)
    KOUT("%s", strerror(errno));
  if (listen(s, 40) < 0)
    KOUT("%s", strerror(errno));
  nonblocking(s);
  return s;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cli_pty_master_action(t_cli *cli)
{
  t_msg msg;
  int len;
  len = read(cli->pty_master_fd, msg.buf, sizeof(msg.buf));
  if (len <= 0)
    {
    mdl_close(cli->pty_master_fd);
    close(cli->pty_master_fd);
    cli->pty_master_fd = -1;
    }
  else
    {
    msg.type = msg_type_data_cli;
    msg.len = len;
    if (mdl_queue_write_msg(cli->sock_fd, &msg))
      KERR("%d", len);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void listen_socket_action(int listen_sock_fd)
{
  int sock_fd;
  sock_fd = accept(listen_sock_fd, NULL, NULL);
  if (sock_fd >= 0)
    {
    if (mdl_open(sock_fd))
      {
      KERR("Error too big fd %d\n", sock_fd);
      close(sock_fd);
      }
    else
      {
      nonblocking(sock_fd);
      cli_alloc(sock_fd);
      }
    }
  else
    KOUT(" ");
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_err_cb (void *ptr, char *err)
{
  t_cli *cli = (t_cli *) ptr;
  cli_free(cli);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_end(int s, char status)
{
  t_msg msg;
  memset(&msg, 0, sizeof(t_msg));
  msg.type = msg_type_end_cli;
  msg.len = 1;
  msg.buf[0] = status;
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
static int rx_msg_cb(void *ptr, int sock_fd, t_msg *msg)
{
  int result = 0;
  t_cli *cli = (t_cli *) ptr;
  switch(msg->type)
    {

    case msg_type_data_pty :
      if (cli->pty_master_fd == -1)
        KOUT(" ");
      else if (mdl_queue_write_raw(cli->pty_master_fd, msg->buf, msg->len))
        KERR("%d", msg->len);
    break;

    case msg_type_open_bash :
      cli->cli_type = cli_type_bash;
      bin_bash_pty(cli, NULL);
    break;

    case msg_type_open_cmd :
      cli->cli_type = cli_type_cmd;
      bin_bash_pty(cli, msg->buf);
    break;

    case msg_type_win_size :
      if (cli->pty_master_fd != -1)
        {
        ioctl(cli->pty_master_fd, TIOCSWINSZ, msg->buf);
        ioctl(cli->pty_master_fd, TIOCSIG, SIGWINCH);
        }
    break;

    case msg_type_scp_open_snd :
    case msg_type_scp_open_rcv :
      if (recv_scp_open(msg->type, sock_fd, &(cli->scp_fd), msg->buf)) 
        {
        cli_free(cli);
        result = -1;
        }
    break;

    case msg_type_scp_data:
      recv_scp_data(cli->scp_fd, msg);
      break;
    case msg_type_scp_data_end:
      recv_scp_data_end(cli->scp_fd, sock_fd);
      break;
    case msg_type_scp_data_begin:
      cli->scp_begin = 1;
      break;
    case msg_type_scp_data_end_ack:
      send_msg_type_end(sock_fd, 0);
      break;

    case msg_type_x11_fwd_init:
      if (!x11_listen(&(cli->x11_fwd_listen_fd)))
        send_msg_type_x11_fwd_init(sock_fd, "OK");
      else
        send_msg_type_x11_fwd_init(sock_fd, "KO");
      break;

    case msg_type_x11_fwd_data:
      if (x11_recv_data(cli->x11_fwd_fd, msg->buf, msg->len))
        {
        close(cli->x11_fwd_fd);
        cli->x11_fwd_fd = -1;
        }
      break;

    default :
      KOUT("%d", msg->type);
      }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void sig_evt_action(int sig_read_fd)
{
  t_cli *cur = g_cli_head;
  int len, status, pid;
  char buf[1], exstat;
  len = read(sig_read_fd, buf, 1);
  if ((len != 1) || (buf[0] != 's'))
    KOUT("%d %s", len, buf);
  else
    {
    if ((pid = waitpid(-1, &status, WNOHANG)) > 0)
      {
      while(cur)
        {
        if (cur->pid == pid)
          {
          if (WIFEXITED(status))
            exstat = WEXITSTATUS(status);
          else
            exstat = 1;
          send_msg_type_end(cur->sock_fd, exstat);
          break;
          }
        cur = cur->next;
        }
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void child_exit(int sig)
{
  int len;
  char buf[1];
  buf[0] = 's';
  len = write(g_sig_write_fd, buf, 1);
  if (len != 1)
    KOUT("%d %s", len, strerror(errno));
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_max(int listen_sock_fd, int sig_read_fd)
{
  t_cli *cur = g_cli_head;
  int result = listen_sock_fd;
  if (sig_read_fd > result)
    result = sig_read_fd;
  while(cur)
    {
    if (cur->sock_fd > result)
      result = cur->sock_fd;
    if (cur->pty_master_fd > result)
      result = cur->pty_master_fd;
    if (cur->x11_fwd_listen_fd > result)
      result = cur->x11_fwd_listen_fd;
    if (cur->x11_fwd_fd > result)
      result = cur->x11_fwd_fd;
    cur = cur->next;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void prepare_fd_set(int listen_sock_fd, int sig_read_fd, 
                           fd_set *readfds, fd_set *writefds) 
{
  t_cli *cur = g_cli_head;
  FD_ZERO(readfds);
  FD_ZERO(writefds);
  FD_SET(listen_sock_fd, readfds);
  FD_SET(sig_read_fd, readfds);
  while(cur)
    {
    if (cur->x11_fwd_listen_fd != -1)
      FD_SET(cur->x11_fwd_listen_fd, readfds);
    if (cur->x11_fwd_fd != -1)
      FD_SET(cur->x11_fwd_fd, readfds);
    if (cur->pty_master_fd != -1)
      {
      if (!mdl_queue_write_saturated(cur->pty_master_fd))
        FD_SET(cur->sock_fd, readfds);
      }
    else
      FD_SET(cur->sock_fd, readfds);
    if (cur->pty_master_fd != -1)
      {
      if (mdl_queue_write_not_empty(cur->pty_master_fd))
        FD_SET(cur->pty_master_fd, writefds);
      }
    if (!mdl_queue_write_saturated(cur->sock_fd))
      {
      if (cur->pty_master_fd != -1)
        FD_SET(cur->pty_master_fd, readfds);
      if (mdl_queue_write_not_empty(cur->sock_fd))
        FD_SET(cur->sock_fd, writefds);
      }
    else
      FD_SET(cur->sock_fd, writefds);
    cur = cur->next;
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void server_loop(int listen_sock_fd, int sig_read_fd)
{
  int max_fd;
  t_cli *next, *cur;
  fd_set readfds, writefds;
  cur = g_cli_head;
  while(cur)
    {
    if ((cur->scp_begin) && (cur->scp_fd != -1))
      {
      if (send_scp_to_cli(cur->scp_fd, cur->sock_fd))
        {
        close(cur->scp_fd);
        cur->scp_begin = 0;
        cur->scp_fd = -1;
        }
      }
    cur = next;
    }
  max_fd = get_max(listen_sock_fd, sig_read_fd);
  prepare_fd_set(listen_sock_fd, sig_read_fd, &readfds, &writefds);
  if (select(max_fd + 1, &readfds, &writefds, NULL, NULL) < 0)
    {
    if (errno != EINTR && errno != EAGAIN)
      KOUT("%s", strerror(errno));
    }
  else
    {
    if (FD_ISSET(listen_sock_fd, &readfds))
      listen_socket_action(listen_sock_fd);
    if (FD_ISSET(sig_read_fd, &readfds))
      sig_evt_action(sig_read_fd);
    cur = g_cli_head;
    while(cur)
      {
      next = cur->next;
      if (FD_ISSET(cur->sock_fd, &writefds))
        mdl_write(cur->sock_fd);
      if (cur->pty_master_fd != -1)
        {
        if (FD_ISSET(cur->pty_master_fd, &readfds))
          cli_pty_master_action(cur);
        }
      if (cur->pty_master_fd != -1)
        {
        if (FD_ISSET(cur->pty_master_fd, &writefds))
          mdl_write(cur->pty_master_fd);
        }
      if (FD_ISSET(cur->sock_fd, &readfds))
        mdl_read((void *)cur, cur->sock_fd, rx_msg_cb, rx_err_cb);
      if (FD_ISSET(cur->x11_fwd_listen_fd, &readfds))
        x11_listen_action(cur->x11_fwd_listen_fd, &(cur->x11_fwd_fd));
      if (FD_ISSET(cur->x11_fwd_fd, &readfds))
        {
        if (x11_action(cur->x11_fwd_fd, cur->sock_fd))
          {
          close(cur->x11_fwd_fd);
          cur->x11_fwd_fd = -1;
          }
        }
      cur = next;
      }
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void vsock_srv(int listen_sock_fd)
{
  int pipe_fd[2];
  daemon(0,0);
  if (signal(SIGPIPE, SIG_IGN))
    KERR("%d", errno);
  if (signal(SIGHUP, SIG_IGN))
    KERR("%d", errno);
  if (signal(SIGTTIN, SIG_IGN))
    KERR("%d", errno);
  if (signal(SIGTTOU, SIG_IGN))
    KERR("%d", errno);
  if (signal(SIGCHLD, child_exit))
    KERR("%d", errno);
  if (pipe(pipe_fd) < 0)
    KERR(" ");
  g_sig_write_fd = pipe_fd[1];
  while (1)
    {
    server_loop(listen_sock_fd, pipe_fd[0]);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int parse_port(const char *port_str)
{
  int result = -1;
  char *end = NULL;
  long port = strtol(port_str, &end, 10);
  if (port_str != end && *end == '\0')
    result = (int) port;
  else
    KOUT("%s", port_str);
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void usage(char *name)
{
  printf("\n%s <vsock_port>\n", name);
  printf("\n\t or for tests:\n");
  printf("\n%s -i <inet_port>", name);
  printf("\n%s -u <unix_sock_path>", name);
  printf("\n\n");
  exit(1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char **argv)
{
  char unix_sock_path[MAX_PATH_LEN];
  int listen_sock_fd, port;
  g_nb_cli = 0;
  if (argc == 2)
    {
    port = mdl_parse_val(argv[1]);
    listen_sock_fd = open_listen_vsock(port);
    if (listen_sock_fd < 0)
      KOUT(" %d: %s\n", port, strerror(errno));
    }
  else if(argc != 3)
    usage(argv[0]);
  else
    {
    if (!strcmp(argv[1], "-u"))
      {
      memset(unix_sock_path, 0, MAX_PATH_LEN);
      strncpy(unix_sock_path, argv[2], MAX_PATH_LEN-1);
      listen_sock_fd = open_listen_usock(unix_sock_path);
      if (listen_sock_fd < 0)
        KOUT(" %s: %s\n", unix_sock_path, strerror(errno));
      }
    else if (!strcmp(argv[1], "-i"))
      {
      port = mdl_parse_val(argv[2]);
      listen_sock_fd = open_listen_isock(port);
      if (listen_sock_fd < 0)
        KOUT(" %d: %s\n", port, strerror(errno));
      }
    else
      usage(argv[0]);
    }

  if (listen_sock_fd >= 0)
    vsock_srv(listen_sock_fd);
  return 0;
}
/*--------------------------------------------------------------------------*/
