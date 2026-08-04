/*
 * inhibit_spacebar.c — Windows GUI Key-agent style TX Inhibit tester
 *
 * Emulates WIMS SSB/CW Key-agent over UDP (same protocol as the gate):
 *   KEY-down  → hold + keepalives
 *   KEY-up    → WIMS adaptive hang (still keepalives), then ttl_ms=0
 *
 * Adaptive hang (inhibit.py adaptive_hang_s):
 *   dit-like (<=200 ms) → hang = 8×dit, clamped 200–1000 ms
 *   long KEY (>=750 ms) or non-dit → hang = 20 ms
 *
 * SPACE / big button = KEY (press-and-hold). Force RELEASE skips hang.
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
  IDT_UI = 2,
  IDT_HANG = 3
};

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 22372
#define DEFAULT_TTL_MS 600
#define DEFAULT_KA_MS 200
#define TTL_MIN 100
#define TTL_MAX 30000

/* WIMS adaptive hang constants (same as TxInhibitLogic.hpp) */
#define LONG_HANG_MS 20
#define ADAPTIVE_DITS 8
#define ADAPTIVE_HANG_MIN_MS 200
#define ADAPTIVE_HANG_MAX_MS 1000
#define LONG_CLOSURE_MS 750
#define DIT_MAX_MS 200
#define CLOSURE_DEBOUNCE_MS 20
#define CLOSURE_WINDOW 8

static HWND g_hwnd;
static HWND g_hold_btn;
static HWND g_status;
static HWND g_last_pkt;
static HWND g_counters;
static SOCKET g_sock = INVALID_SOCKET;
static int g_seq;
static int g_holds, g_releases, g_keepalives, g_errors;
static int g_udp_active;   /* sending holds/keepalives to the gate */
static int g_key_down;     /* physical KEY (space/button) down */
static int g_in_hang;      /* KEY up, still hanging before release */
static ULONGLONG g_deadline_ms;
static ULONGLONG g_key_down_at;
static ULONGLONG g_hang_until;
static int g_last_hang_ms;
static int g_closure_ms[CLOSURE_WINDOW];
static int g_closure_count;

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

static void push_closure_ms(int duration_ms) {
  int i;
  if (duration_ms < CLOSURE_DEBOUNCE_MS)
    return;
  if (g_closure_count < CLOSURE_WINDOW) {
    g_closure_ms[g_closure_count++] = duration_ms;
    return;
  }
  for (i = 1; i < CLOSURE_WINDOW; i++)
    g_closure_ms[i - 1] = g_closure_ms[i];
  g_closure_ms[CLOSURE_WINDOW - 1] = duration_ms;
}

/* Same rules as TxInhibit::adaptive_hang_ms / WIMS adaptive_hang_s */
static int adaptive_hang_ms(void) {
  int i, last, min_dit, hang;
  int have_dit = 0;
  if (g_closure_count <= 0)
    return LONG_HANG_MS;
  last = g_closure_ms[g_closure_count - 1];
  if (last >= LONG_CLOSURE_MS)
    return LONG_HANG_MS;
  min_dit = 0;
  for (i = 0; i < g_closure_count; i++) {
    int c = g_closure_ms[i];
    if (c > 0 && c <= DIT_MAX_MS) {
      if (!have_dit || c < min_dit) {
        min_dit = c;
        have_dit = 1;
      }
    }
  }
  if (!have_dit)
    return LONG_HANG_MS;
  hang = ADAPTIVE_DITS * min_dit;
  if (hang < ADAPTIVE_HANG_MIN_MS) hang = ADAPTIVE_HANG_MIN_MS;
  if (hang > ADAPTIVE_HANG_MAX_MS) hang = ADAPTIVE_HANG_MAX_MS;
  return hang;
}

static int send_datagram(HWND hwnd, int ttl_ms_arg, int is_keepalive) {
  char host[128], station[64], band[32], portstr[32];
  char payload[512];
  char status[320];
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
    snprintf(status, sizeof status, "RELEASE → %s:%d  seq=%d", host, port, g_seq);
  } else if (is_keepalive) {
    g_keepalives++;
    g_deadline_ms = GetTickCount64() + (ULONGLONG)ttl_ms;
    if (g_in_hang) {
      int hang_left = (int)(g_hang_until - GetTickCount64());
      if (hang_left < 0) hang_left = 0;
      snprintf(status, sizeof status,
               "HANG keepalive → %s:%d  hang left %d ms (hang was %d ms)",
               host, port, hang_left, g_last_hang_ms);
    } else {
      snprintf(status, sizeof status, "KEEPALIVE → %s:%d  seq=%d", host, port, g_seq);
    }
  } else {
    g_holds++;
    g_deadline_ms = GetTickCount64() + (ULONGLONG)ttl_ms;
    snprintf(status, sizeof status, "HOLD (KEY down) → %s:%d  ttl=%d",
             host, port, ttl_ms);
  }
  set_text(g_status, status);
  return 1;
}

static void update_counters(void) {
  char b[400];
  ULONGLONG now = GetTickCount64();
  const char *st;
  int hang_left = 0;

  if (g_key_down)
    st = "KEY-DOWN";
  else if (g_in_hang)
    st = "HANG";
  else if (g_udp_active)
    st = "UDP-ACTIVE";
  else
    st = "idle";

  if (g_in_hang && g_hang_until > now)
    hang_left = (int)(g_hang_until - now);

  if (g_in_hang) {
    snprintf(b, sizeof b,
             "state=%s   hang=%d ms left (applied %d)   seq=%d   "
             "holds=%d ka=%d rel=%d err=%d   closures=%d",
             st, hang_left, g_last_hang_ms, g_seq, g_holds, g_keepalives,
             g_releases, g_errors, g_closure_count);
  } else {
    snprintf(b, sizeof b,
             "state=%s   seq=%d   holds=%d ka=%d rel=%d err=%d   closures=%d",
             st, g_seq, g_holds, g_keepalives, g_releases, g_errors,
             g_closure_count);
  }
  set_text(g_counters, b);
}

static void set_key_ui(void) {
  if (g_key_down) {
    set_text(g_hold_btn, "KEY DOWN — release SPACE / mouse for adaptive hang");
  } else if (g_in_hang) {
    set_text(g_hold_btn, "HANG — keepalives until hang ends, then release");
  } else {
    set_text(g_hold_btn, "Hold SPACE or mouse = KEY (adaptive hang on release)");
  }
}

static void stop_timer(HWND hwnd, UINT id) {
  KillTimer(hwnd, id);
}

static void start_keepalive_timer(HWND hwnd) {
  int ka = get_int_field(hwnd, IDC_KEEPALIVE, DEFAULT_KA_MS);
  if (ka < 50) ka = 50;
  if (ka > 2000) ka = 2000;
  stop_timer(hwnd, IDT_KEEPALIVE);
  SetTimer(hwnd, IDT_KEEPALIVE, (UINT)ka, NULL);
}

/* KEY-down: start UDP hold + keepalives (Key-agent assert). */
static void key_down(HWND hwnd) {
  if (g_key_down)
    return;
  /* Cancel any hang-in-progress: KEY is down again */
  stop_timer(hwnd, IDT_HANG);
  g_in_hang = 0;
  g_key_down = 1;
  g_key_down_at = GetTickCount64();
  if (!g_udp_active) {
    if (!send_datagram(hwnd, 1, 0)) {
      g_key_down = 0;
      return;
    }
    g_udp_active = 1;
    start_keepalive_timer(hwnd);
  } else {
    /* already hanging with UDP — refresh hold */
    send_datagram(hwnd, 1, 0);
  }
  set_key_ui();
  update_counters();
}

static void finish_release(HWND hwnd) {
  stop_timer(hwnd, IDT_KEEPALIVE);
  stop_timer(hwnd, IDT_HANG);
  g_udp_active = 0;
  g_in_hang = 0;
  g_key_down = 0;
  send_datagram(hwnd, 0, 0);
  set_key_ui();
  update_counters();
}

/* KEY-up: measure closure, adaptive hang, then release (Key-agent). */
static void key_up(HWND hwnd) {
  int dur, hang;
  ULONGLONG now;
  char msg[160];

  if (!g_key_down)
    return;
  g_key_down = 0;
  now = GetTickCount64();
  dur = (int)(now - g_key_down_at);
  if (dur < 0) dur = 0;
  push_closure_ms(dur);
  hang = adaptive_hang_ms();
  g_last_hang_ms = hang;

  if (!g_udp_active) {
    /* never got a hold out — nothing to hang */
    set_key_ui();
    update_counters();
    return;
  }

  if (hang <= 0) {
    finish_release(hwnd);
    return;
  }

  g_in_hang = 1;
  g_hang_until = now + (ULONGLONG)hang;
  /* keep keepalives during hang so gate deadman does not expire early */
  start_keepalive_timer(hwnd);
  stop_timer(hwnd, IDT_HANG);
  SetTimer(hwnd, IDT_HANG, (UINT)hang, NULL);
  snprintf(msg, sizeof msg,
           "KEY up after %d ms → adaptive hang %d ms (then RELEASE)",
           dur, hang);
  set_text(g_status, msg);
  set_key_ui();
  update_counters();
}

/* Immediate clear — no hang (Escape / Force RELEASE). */
static void force_release(HWND hwnd) {
  g_key_down = 0;
  g_in_hang = 0;
  stop_timer(hwnd, IDT_HANG);
  finish_release(hwnd);
  set_text(g_status, "Force RELEASE (no hang)");
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

static WNDPROC g_old_btn_proc;

static LRESULT CALLBACK HoldBtnProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
  HWND parent = GetParent(hwnd);
  switch (msg) {
  case WM_LBUTTONDOWN:
    SetCapture(hwnd);
    key_down(parent);
    break;
  case WM_LBUTTONUP:
    if (GetCapture() == hwnd)
      ReleaseCapture();
    key_up(parent);
    break;
  case WM_CAPTURECHANGED:
    if ((HWND)lParam != hwnd && g_key_down)
      key_up(parent);
    break;
  }
  return CallWindowProcA(g_old_btn_proc, hwnd, msg, wParam, lParam);
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

  g_hold_btn = CreateWindowA(
      "BUTTON", "Hold SPACE or mouse = KEY (adaptive hang on release)",
      WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
      x, y, 480, 72, hwnd, (HMENU)(intptr_t)IDC_HOLD_BTN,
      GetModuleHandle(NULL), NULL);
  SendMessage(g_hold_btn, WM_SETFONT, (WPARAM)font, TRUE);
  g_old_btn_proc = (WNDPROC)SetWindowLongPtrA(
      g_hold_btn, GWLP_WNDPROC, (LONG_PTR)HoldBtnProc);
  y += 80;

  c = CreateWindowA("BUTTON", "Force RELEASE now (skip hang)",
                    WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                    x, y, 240, 28, hwnd, (HMENU)(intptr_t)IDC_RELEASE_BTN,
                    GetModuleHandle(NULL), NULL);
  SendMessage(c, WM_SETFONT, (WPARAM)font, TRUE);
  y += 36;

  add_label(hwnd, x, y, 480, eh, "Status");
  y += eh;
  g_status = CreateWindowA("STATIC",
                           "IDLE — press SPACE = KEY down; release = adaptive hang then clear",
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
      "Key-agent simulation (WIMS adaptive hang): short press (dit) → hang ~8xdit; "
      "long press (>=750 ms) → hang 20 ms; then UDP release. Force RELEASE skips hang. "
      "Same hang rules as CTS KEY inside wsjtx-inhibit and the SSB/CW agent.",
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
    SetTimer(hwnd, IDT_UI, 50, NULL);
    return 0;

  case WM_COMMAND:
    if (LOWORD(wParam) == IDC_RELEASE_BTN && HIWORD(wParam) == BN_CLICKED) {
      force_release(hwnd);
      return 0;
    }
    break;

  case WM_TIMER:
    if (wParam == IDT_KEEPALIVE) {
      if (g_udp_active && (g_key_down || g_in_hang)) {
        send_datagram(hwnd, 1, 1);
        update_counters();
      } else {
        stop_timer(hwnd, IDT_KEEPALIVE);
      }
    } else if (wParam == IDT_HANG) {
      stop_timer(hwnd, IDT_HANG);
      if (g_in_hang && !g_key_down)
        finish_release(hwnd);
    } else if (wParam == IDT_UI) {
      /* poll hang end in case timer granularity is coarse */
      if (g_in_hang && !g_key_down && GetTickCount64() >= g_hang_until)
        finish_release(hwnd);
      else
        update_counters();
    }
    return 0;

  case WM_DESTROY:
    force_release(hwnd);
    stop_timer(hwnd, IDT_UI);
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProcA(hwnd, msg, wParam, lParam);
}

static int filter_keys(MSG *m) {
  if (!g_hwnd) return 0;

  if (m->message == WM_KEYDOWN && m->wParam == VK_ESCAPE) {
    force_release(g_hwnd);
    return 1;
  }

  if (m->message == WM_KEYDOWN && m->wParam == VK_SPACE) {
    if (is_edit_focus(g_hwnd)) return 0;
    if (!(m->lParam & (1 << 30)))
      key_down(g_hwnd);
    return 1;
  }

  if (m->message == WM_KEYUP && m->wParam == VK_SPACE) {
    if (is_edit_focus(g_hwnd) && !g_key_down) return 0;
    if (g_key_down)
      key_up(g_hwnd);
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
      "wsjtx-inhibit — Key-agent (spacebar) tester",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, 530, 500,
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
