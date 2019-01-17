#include "flora-agent.h"
#include <curses.h>
#include <stdio.h>
#include <string.h>

using namespace std;
using namespace flora;

#define MONITOR_MSG_NAME "--dfdc9571857b9306326d9bb3ef5fe71299796858--"

static const uint32_t COLUMN_WIDTH[] = {8, 32};
#define COLUMN_PAD 1
static char textBuffer[64];
static const char *COLUMN_HEADER_TEXT[] = {"PID", "NAME"};

typedef struct {
} CmdlineArgs;

static bool parseCmdline(int argc, char **argv, CmdlineArgs &res) {
  return true;
}

static void printTabCell(int32_t idx, const char *content) {
  uint32_t width = COLUMN_WIDTH[idx] + COLUMN_PAD;
  int32_t len;
  memset(textBuffer, ' ', width);
  textBuffer[width] = '\0';
  len = strlen(content);
  if (len > COLUMN_WIDTH[idx])
    len = COLUMN_WIDTH[idx];
  memcpy(textBuffer, content, len);
  printw("%s", textBuffer);
}

static void printTabHeader() {
  uint32_t numCol = sizeof(COLUMN_WIDTH) / sizeof(COLUMN_WIDTH[0]);
  uint32_t i;

  for (i = 0; i < numCol; ++i) {
    printTabCell(i, COLUMN_HEADER_TEXT[i]);
  }
  printw("\n");
}

static void printTabLine(shared_ptr<Caps> &msg) {
  int32_t pid;
  string name;
  if (msg->read(pid) != CAPS_SUCCESS) {
    return;
  }
  if (msg->read(name) != CAPS_SUCCESS) {
    return;
  }
  char buf[16];
  snprintf(buf, sizeof(buf), "%d", pid);
  printTabCell(0, buf);
  printTabCell(1, name.c_str());
  printw("\n");
}

static void updateMonitorScreen(shared_ptr<Caps> &msg) {
  clear();
  printTabHeader();
  shared_ptr<Caps> sub;
  while (true) {
    if (msg->read(sub) != CAPS_SUCCESS)
      break;
    printTabLine(sub);
  }
  refresh();
}

static void doMonitor(CmdlineArgs &args) {
  initscr();
  raw();
  keypad(stdscr, TRUE);
  noecho();

  Agent agent;
  agent.config(FLORA_AGENT_CONFIG_URI,
               "unix:/var/run/flora.sock#flora-monitor");
  agent.subscribe(MONITOR_MSG_NAME, [](const char *, shared_ptr<Caps> &msg,
                                       uint32_t) { updateMonitorScreen(msg); });
  agent.start();

  int c;
  while (true) {
    c = getch();
    if (c < 0) {
      break;
    }
    if (c == 'q' || c == 'Q')
      break;
  }

  agent.close();
  endwin();
}

int main(int argc, char **argv) {
  CmdlineArgs cmdlineArgs;

  if (!parseCmdline(argc, argv, cmdlineArgs)) {
    return 1;
  }

  doMonitor(cmdlineArgs);
  return 0;
}
