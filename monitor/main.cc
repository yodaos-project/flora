#include "flora-agent.h"
#include <curses.h>
#include <list>
#include <stdio.h>
#include <string.h>

using namespace std;
using namespace flora;

static const uint32_t COLUMN_WIDTH[] = {8, 32};
#define COLUMN_PAD 1
static char textBuffer[64];
static const char *COLUMN_HEADER_TEXT[] = {"PID", "NAME"};

typedef struct {
} CmdlineArgs;

static bool parseCmdline(int argc, char **argv, CmdlineArgs &res) {
  return true;
}

class FloraClientInfo {
public:
  uint32_t id = 0;
  int32_t pid = 0;
  string name;
  uint32_t flags = 0;
};

class MonitorView : public flora::MonitorCallback {
public:
  void list_all(std::vector<MonitorListItem> &items) {
    auto it = items.begin();
    while (it != items.end()) {
      auto fcit = floraClients.emplace(floraClients.end());
      fcit->id = it->id;
      fcit->pid = it->pid;
      fcit->name = it->name;
      fcit->flags = it->flags;
      ++it;
    }
    updateMonitorScreen();
  }

  void list_add(MonitorListItem &item) {
    auto fcit = floraClients.emplace(floraClients.end());
    fcit->id = item.id;
    fcit->pid = item.pid;
    fcit->name = item.name;
    fcit->flags = item.flags;
    updateMonitorScreen();
  }

  void list_remove(uint32_t id) {
    auto it = floraClients.begin();
    while (it != floraClients.end()) {
      if (it->id == id) {
        floraClients.erase(it);
        updateMonitorScreen();
        break;
      }
      ++it;
    }
  }

private:
  void updateMonitorScreen() {
    clear();
    printTabHeader();
    auto it = floraClients.begin();
    while (it != floraClients.end()) {
      printTabLine(*it);
      ++it;
    }
    refresh();
  }

  void printTabHeader() {
    uint32_t numCol = sizeof(COLUMN_WIDTH) / sizeof(COLUMN_WIDTH[0]);
    uint32_t i;

    for (i = 0; i < numCol; ++i) {
      printTabCell(i, COLUMN_HEADER_TEXT[i]);
    }
    printw("\n");
  }

  void printTabLine(FloraClientInfo &info) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d", info.pid);
    printTabCell(0, buf);
    printTabCell(1, info.name.c_str());
    printw("\n");
  }

  void printTabCell(int32_t idx, const char *content) {
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

private:
  list<FloraClientInfo> floraClients;
};

static void doMonitor(CmdlineArgs &args) {
  initscr();
  raw();
  keypad(stdscr, TRUE);
  noecho();

  Agent agent;
  MonitorView monitorView;
  agent.config(FLORA_AGENT_CONFIG_URI,
               "unix:/var/run/flora.sock#flora-monitor");
  agent.config(FLORA_AGENT_CONFIG_MONITOR, FLORA_CLI_FLAG_MONITOR,
               &monitorView);
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
