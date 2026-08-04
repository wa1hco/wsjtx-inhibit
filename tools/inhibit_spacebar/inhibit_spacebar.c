/*
 * inhibit_spacebar.c — Windows GUI TX Inhibit tester for wsjtx-inhibit
 *
 * Native .exe (system DLLs only). Protocol matches TxInhibitLogic.hpp.
 *
 * SPACE  = hold while down (keepalive), release on key-up
 * Button = TOGGLE click (on / off) — reliable for mouse users
 * "Send release" / Escape = force clear
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#define WIN32_LEAN_AND_MEAN
#define _WIN32_WINNT 0x0601
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <commctrl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
  IDC_HOST = 1001,
  IDC_PORT,
  IDC_STATION,
  IDC_BAND,
  IDC_TTL,
  IDC_KEEPALIVE,
  IDC_HOLD_BTN,
  IDC_RELEASE_BTN,
  IDC_STATUS,
  IDC_LAST_PKT,
  IDC_COUNTERS,
  IDC_HINT,
  IDT_KEEPALIVE = 1,
  IDT_UI = 2
};

#ifndef BN_PUSHED
#define BN_PUSHED 2
#define BN_UNPUSHED 3
#endif

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 22372
#define DEFAULT_TTL_MS 600
#define DEFAULT_KA_MS 200
#define TTL_MIN 100
#define TTL_MAX 30000

static HWND g_hwnd;
static HWND g_hold_btn;
static HWND g_status;
static HWND g_last_pkt;
static HWND g_counters;
static SOCKET g_sock = INVALID_SOCKET;
static int g_seq;
static int g_holds, g_releases, g_keepalives, g_errors;
static int g_holding;     /* UDP hold active (keepalives running) */
static int g_space_down;  /* physical space currently down */
static ULONGLONG g_deadline_ms;

static void set_text(HWND h, const char *s) {
  if (h) SetWindowTextA(h, s ? s : "");
}

static void get_text(HWND parent, int id, char *buf, int n) {
  GetDlgItemTextA(parent, id, buf, n);
  buf[n - 1] = 0;
}

static int get_int_field(HWND parent, int id, int defv) {
  char b[64];
  get_text(parent, id, b, sizeof b);
  if (!b[0]) return defv;
  return atoi(b);
}

static int encode_datagram(char *out, int outn,
                           const char *station, const char *band,
                           int seq, int ttl_ms) {
  return snprintf(out, outn,
                  "{\"tx_inhibit\":1,\"ttl_ms\":%d,\"station\":\"%s\","
                  "\"band\":\"%s\",\"seq\":%d}",
                  ttl_ms, station, band, seq);
}

static void sanitize_token(char *s) {
  char *r = s, *w = s;
  for (; *r; ++r) {
    if (*r == '"' || *r == '\\' || (unsigned char)*r < 0x20) continue;
    *w++ = *r;
  }
  *w = 0;
}

/* ttl_ms_arg: 0 = release; nonzero = use UI TTL field (hold/keepalive) */
static int send_datagram(HWND hwnd, int ttl_ms_arg, int is_keepalive) {
  char host[128], station[64], band[32], portstr[32];
  char payload[512];
  char status[288];
  struct sockaddr_in addr;
  int port, ttl_ms, n, sent;

  get_text(hwnd, IDC_HOST, host, sizeof host);
  get_text(hwnd, IDC_STATION, station, sizeof station);
  get_text(hwnd, IDC_BAND, band, sizeof band);
  get_text(hwnd, IDC_PORT, portstr, sizeof portstr);

  if (!host[0]) strcpy(host, DEFAULT_HOST);
  if (!station[0]) strcpy(station, "TEST-SSB");
  if (!band[0]) strcpy(band, "144");
  sanitize_token(station);
  sanitize_token(band);
  port = portstr[0] ? atoi(portstr) : DEFAULT_PORT;
  if (port < 1 || port > 65535) {
    set_text(g_status, "ERROR: bad port");
    g_errors++;
    return 0;
  }

  if (ttl_ms_arg == 0) {
    ttl_ms = 0;
  } else {
    ttl_ms = get_int_field(hwnd, IDC_TTL, DEFAULT_TTL_MS);
    if (ttl_ms < TTL_MIN || ttl_ms > TTL_MAX) {
      set_text(g_status, "ERROR: TTL must be 100..30000");
      g_errors++;
      return 0;
    }
  }

  g_seq++;
  n = encode_datagram(payload, sizeof payload, station, band, g_seq, ttl_ms);
  if (n <= 0 || n >= (int)sizeof payload) {
    set_text(g_status, "ERROR: encode failed");
    g_errors++;
    return 0;
  }

  memset(&addr, 0, sizeof addr);
  addr.sin_family = AF_INET;
  addr.sin_port = htons((u_short)port);
  addr.sin_addr.s_addr = inet_addr(host);
  if (addr.sin_addr.s_addr == INADDR_NONE) {
    struct hostent *he = gethostbyname(host);
    if (!he || !he->h_addr_list || !he->h_addr_list[0]) {
      set_text(g_status, "ERROR: bad host");
      g_errors++;
      return 0;
    }
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);
  }

  sent = sendto(g_sock, payload, n, 0, (struct sockaddr *)&addr, sizeof addr);
  if (sent != n) {
    snprintf(status, sizeof status, "ERROR: sendto failed (%d)", WSAGetLastError());
    set_text(g_status, status);
    g_errors++;
    return 0;
  }

  set_text(g_last_pkt, payload);

  if (ttl_ms == 0) {
    g_releases++;
    g_deadline_ms = 0;
    snprintf(status, sizeof status, "RELEASE sent → %s:%d  seq=%d", host, port, g_seq);
  } else if (is_keepalive) {
    g_keepalives++;
    g_deadline_ms = GetTickCount64() + (ULONGLONG)ttl_ms;
    snprintf(status, sizeof status, "KEEPALIVE → %s:%d  seq=%d", host, port, g_seq);
  } else {
    g_holds++;
    g_deadline_ms = GetTickCount64() + (ULONGLONG)ttl_ms;
    snprintf(status, sizeof status, "HOLD → %s:%d  seq=%d  ttl=%d",
             host, port, g_seq, ttl_ms);
  }
  set_text(g_status, status);
  return 1;
}

static void update_counters(void) {
  char b[320];
  ULONGLONG now = GetTickCount64();
  int rem = 0;
  if (g_deadline_ms > now)
    rem = (int)(g_deadline_ms - now);

  if (g_deadline_ms && rem > 0) {
    snprintf(b, sizeof b,
             "state=%s   seq=%d   holds=%d   ka=%d   releases=%d   err=%d   "
             "deadman in %d ms",
             g_holding ? "HOLDING" : "idle", g_seq, g_holds, g_keepalives,
             g_releases, g_errors, rem);
  } else {
    snprintf(b, sizeof b,
             "state=%s   seq=%d   holds=%d   ka=%d   releases=%d   err=%d",
             g_holding ? "HOLDING" : "idle", g_seq, g_holds, g_keepalives,
             g_releases, g_errors);
  }
  set_text(g_counters, b);
}

static void set_holding_ui(int holding) {
  if (holding) {
    set_text(g_hold_btn, "INHIBITING — click again or release SPACE to clear");
  } else {
    set_text(g_hold_btn, "Click to INHIBIT (toggle)  |  hold SPACE");
  }
}

static void stop_keepalive_timer(HWND hwnd) {
  KillTimer(hwnd, IDT_KEEPALIVE);
}

static void start_keepalive_timer(HWND hwnd) {
  int ka = get_int_field(hwnd, IDC_KEEPALIVE, DEFAULT_KA_MS);
  if (ka < 50) ka = 50;
  if (ka > 2000) ka = 2000;
  stop_keepalive_timer(hwnd);
  SetTimer(hwnd, IDT_KEEPALIVE, (UINT)ka, NULL);
}

static void start_hold(HWND hwnd) {
  if (g_holding) return;
  if (!send_datagram(hwnd, 1, 0))
    return;
  g_holding = 1;
  set_holding_ui(1);
  start_keepalive_timer(hwnd);
  update_counters();
}

/* Always stop keepalives and send release (idempotent). */
static void end_hold(HWND hwnd) {
  int was = g_holding;
  g_holding = 0;
  stop_keepalive_timer(hwnd);
  /* Send release even if we thought we were idle (clears stuck gate). */
  send_datagram(hwnd, 0, 0);
  set_holding_ui(0);
  if (!was)
    set_text(g_status, "RELEASE sent (was already idle in tester)");
  update_counters();
}

static void toggle_hold(HWND hwnd) {
  if (g_holding)
    end_hold(hwnd);
  else
    start_hold(hwnd);
}

static int is_edit_focus(HWND hwnd) {
  HWND f = GetFocus();
  int id;
  (void)hwnd;
  if (!f) return 0;
  id = GetDlgCtrlID(f);
  return id == IDC_HOST || id == IDC_PORT || id == IDC_STATION ||
         id == IDC_BAND || id == IDC_TTL || id == IDC_KEEPALIVE;
}

static HWND add_label(HWND p, int x, int y, int w, int h, const char *t) {
  return CreateWindowA("STATIC", t, WS_CHILD | WS_VISIBLE,
                       x, y, w, h, p, NULL, GetModuleHandle(NULL), NULL);
}

static HWND add_edit(HWND p, int id, int x, int y, int w, int h, const char *t) {
  return CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", t,
                         WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                         x, y, w, h, p, (HMENU)(intptr_t)id,
                         GetModuleHandle(NULL), NULL);
}

static void layout(HWND hwnd) {
  int y = 12, x = 12, lw = 90, eh = 22, gap = 8;
  HFONT font = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
  HWND c;

  add_label(hwnd, x, y + 3, lw, eh, "Host");
  c = add_edit(hwnd, IDC_HOST, x + lw, y, 180, eh, DEFAULT_HOST);
  SendMessage(c, WM_SETFONT, (WPARAM)font, TRUE);
  add_label(hwnd, x + 290, y + 3, 40, eh, "Port");
  c = add_edit(hwnd, IDC_PORT, x + 330, y, 70, eh, "22372");
  SendMessage(c, WM_SETFONT, (WPARAM)font, TRUE);
  y += eh + gap;

  add_label(hwnd, x, y + 3, lw, eh, "Station");
  c = add_edit(hwnd, IDC_STATION, x + lw, y, 180, eh, "TEST-SSB");
  SendMessage(c, WM_SETFONT, (WPARAM)font, TRUE);
  add_label(hwnd, x + 290, y + 3, 40, eh, "Band");
  c = add_edit(hwnd, IDC_BAND, x + 330, y, 70, eh, "144");
  SendMessage(c, WM_SETFONT, (WPARAM)font, TRUE);
  y += eh + gap;

  add_label(hwnd, x, y + 3, lw, eh, "TTL ms");
  c = add_edit(hwnd, IDC_TTL, x + lw, y, 80, eh, "600");
  SendMessage(c, WM_SETFONT, (WPARAM)font, TRUE);
  add_label(hwnd, x + 200, y + 3, 90, eh, "Keepalive ms");
  c = add_edit(hwnd, IDC_KEEPALIVE, x + 300, y, 70, eh, "200");
  SendMessage(c, WM_SETFONT, (WPARAM)font, TRUE);
  y += eh + gap + 4;

  /* BS_NOTIFY: we only use BN_CLICKED for toggle (mouse). Space is separate. */
  g_hold_btn = CreateWindowA(
      "BUTTON", "Click to INHIBIT (toggle)  |  hold SPACE",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON | BS_NOTIFY,
      x, y, 480, 72, hwnd, (HMENU)(intptr_t)IDC_HOLD_BTN,
      GetModuleHandle(NULL), NULL);
  SendMessage(g_hold_btn, WM_SETFONT, (WPARAM)font, TRUE);
  y += 80;

  c = CreateWindowA("BUTTON", "Force RELEASE now",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    x, y, 200, 28, hwnd, (HMENU)(intptr_t)IDC_RELEASE_BTN,
                    GetModuleHandle(NULL), NULL);
  SendMessage(c, WM_SETFONT, (WPARAM)font, TRUE);
  y += 36;

  add_label(hwnd, x, y, 480, eh, "Status");
  y += eh;
  g_status = CreateWindowA("STATIC", "IDLE — click button to toggle, or hold SPACE",
                           WS_CHILD | WS_VISIBLE | SS_LEFT,
                           x, y, 480, eh + 4, hwnd, (HMENU)(intptr_t)IDC_STATUS,
                           GetModuleHandle(NULL), NULL);
  SendMessage(g_status, WM_SETFONT, (WPARAM)font, TRUE);
  y += eh + gap;

  add_label(hwnd, x, y, 480, eh, "Last packet");
  y += eh;
  g_last_pkt = CreateWindowA("STATIC", "—",
                             WS_CHILD | WS_VISIBLE | SS_LEFT | SS_ENDELLIPSIS,
                             x, y, 480, eh * 2, hwnd, (HMENU)(intptr_t)IDC_LAST_PKT,
                             GetModuleHandle(NULL), NULL);
  SendMessage(g_last_pkt, WM_SETFONT, (WPARAM)font, TRUE);
  y += eh * 2 + gap;

  g_counters = CreateWindowA("STATIC", "",
                             WS_CHILD | WS_VISIBLE | SS_LEFT,
                             x, y, 480, eh * 2, hwnd, (HMENU)(intptr_t)IDC_COUNTERS,
                             GetModuleHandle(NULL), NULL);
  SendMessage(g_counters, WM_SETFONT, (WPARAM)font, TRUE);
  y += eh * 2 + gap;

  c = CreateWindowA(
      "STATIC",
      "wsjtx-inhibit: PTT=RTS/DTR. Mouse = click to arm, click again to clear. "
      "SPACE = hold only while pressed. Force RELEASE if stuck. Escape = release. "
      "If badge says local KEY line, CTS on that COM is asserted (floating pin).",
      WS_CHILD | WS_VISIBLE | SS_LEFT,
      x, y, 480, eh * 4, hwnd, (HMENU)(intptr_t)IDC_HINT,
      GetModuleHandle(NULL), NULL);
  SendMessage(c, WM_SETFONT, (WPARAM)font, TRUE);

  update_counters();
}

static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  switch (msg) {
  case WM_CREATE:
    layout(hwnd);
    SetTimer(hwnd, IDT_UI, 100, NULL);
    return 0;

  case WM_COMMAND: {
    int id = LOWORD(wParam);
    int code = HIWORD(wParam);
    if (id == IDC_HOLD_BTN && code == BN_CLICKED) {
      /* Ignore BN_CLICKED synthesized from SPACE when we handle space ourselves:
         if space is down, space path owns hold state. */
      if (!g_space_down)
        toggle_hold(hwnd);
      return 0;
    }
    if (id == IDC_RELEASE_BTN && code == BN_CLICKED) {
      g_space_down = 0;
      end_hold(hwnd);
      return 0;
    }
    break;
  }

  case WM_TIMER:
    if (wParam == IDT_KEEPALIVE) {
      if (g_holding) {
        if (!send_datagram(hwnd, 1, 1)) {
          /* keep trying; user can Force RELEASE */
        }
        update_counters();
      } else {
        stop_keepalive_timer(hwnd);
      }
    } else if (wParam == IDT_UI) {
      update_counters();
    }
    return 0;

  case WM_DESTROY:
    g_holding = 0;
    stop_keepalive_timer(hwnd);
    /* best-effort release so we don't leave WSJT stuck */
    send_datagram(hwnd, 0, 0);
    KillTimer(hwnd, IDT_UI);
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

/* SPACE: press-and-hold (not toggle). Filtered before DispatchMessage. */
static int filter_keys(MSG *m) {
  if (!g_hwnd) return 0;

  if (m->message == WM_KEYDOWN && m->wParam == VK_ESCAPE) {
    g_space_down = 0;
    end_hold(g_hwnd);
    return 1;
  }

  if (m->message == WM_KEYDOWN && m->wParam == VK_SPACE) {
    if (is_edit_focus(g_hwnd)) return 0;
    if (!(m->lParam & (1 << 30))) { /* ignore autorepeat */
      g_space_down = 1;
      start_hold(g_hwnd);
    }
    return 1; /* swallow so focused button does not also "click" */
  }

  if (m->message == WM_KEYUP && m->wParam == VK_SPACE) {
    if (is_edit_focus(g_hwnd) && !g_space_down) return 0;
    if (g_space_down) {
      g_space_down = 0;
      end_hold(g_hwnd);
    }
    return 1;
  }
  return 0;
}

int WINAPI WinMain(HINSTANCE hi, HINSTANCE hp, LPSTR cmd, int show) {
  WNDCLASSA wc;
  MSG msg;
  WSADATA wsa;
  (void)hp;
  (void)cmd;

  InitCommonControls();
  if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
    MessageBoxA(NULL, "WSAStartup failed", "inhibit_spacebar", MB_ICONERROR);
    return 1;
  }
  g_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (g_sock == INVALID_SOCKET) {
    MessageBoxA(NULL, "UDP socket failed", "inhibit_spacebar", MB_ICONERROR);
    WSACleanup();
    return 1;
  }

  memset(&wc, 0, sizeof wc);
  wc.lpfnWndProc = WndProc;
  wc.hInstance = hi;
  wc.lpszClassName = "WsjtxInhibitSpacebar";
  wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  if (!RegisterClassA(&wc)) {
    MessageBoxA(NULL, "RegisterClass failed", "inhibit_spacebar", MB_ICONERROR);
    return 1;
  }

  g_hwnd = CreateWindowExA(
      0, wc.lpszClassName,
      "wsjtx-inhibit — Spacebar TX Inhibit Tester",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, 530, 480,
      NULL, NULL, hi, NULL);
  if (!g_hwnd) {
    MessageBoxA(NULL, "CreateWindow failed", "inhibit_spacebar", MB_ICONERROR);
    return 1;
  }

  ShowWindow(g_hwnd, show);
  UpdateWindow(g_hwnd);

  while (GetMessageA(&msg, NULL, 0, 0) > 0) {
    if (filter_keys(&msg))
      continue;
    TranslateMessage(&msg);
    DispatchMessageA(&msg);
  }

  closesocket(g_sock);
  WSACleanup();
  return 0;
}
