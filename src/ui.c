#include "ui.h"

#include <commctrl.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define APP_TITLE "WinSoftphone"

#define ID_EDIT_DIAL 100
#define ID_STATUS_TEXT 101
#define ID_LINE_TEXT 102

#define ID_BUTTON_CALL 200
#define ID_BUTTON_HANGUP 201
#define ID_BUTTON_HOLD 202
#define ID_BUTTON_WARM_TRANSFER 203
#define ID_BUTTON_BLIND_TRANSFER 204
#define ID_BUTTON_RECORD 205
#define ID_BUTTON_MUTE 206
#define ID_BUTTON_CALL_SELECTED 207

#define ID_MENU_SETTINGS 300
#define ID_MENU_EXIT 301

#define ID_DTMF_BASE 400

#define ID_SETTINGS_CAPTURE 500
#define ID_SETTINGS_PLAYBACK 501
#define ID_SETTINGS_RX 502
#define ID_SETTINGS_TX 503
#define ID_SETTINGS_APPLY 504
#define ID_SETTINGS_CLOSE 505
#define ID_SETTINGS_DOMAIN 510
#define ID_SETTINGS_USER 511
#define ID_SETTINGS_PASSWORD 512
#define ID_SETTINGS_PROXY 513
#define ID_SETTINGS_TRANSPORT 514
#define ID_SETTINGS_REGISTER 515

#define ID_HISTORY_LIST 700

#define ID_TRAY_ICON 800
#define ID_TRAY_OPEN 801
#define ID_TRAY_EXIT 802

#define WM_APP_STATUS (WM_APP + 1)
#define WM_APP_LINE (WM_APP + 2)
#define WM_APP_OPEN_URL (WM_APP + 3)
#define WM_APP_TRAY (WM_APP + 4)

typedef struct {
  HWND hwnd;
  HWND list_capture;
  HWND list_playback;
  HWND slider_rx;
  HWND slider_tx;
  HWND edit_domain;
  HWND edit_user;
  HWND edit_password;
  HWND edit_proxy;
  HWND edit_transport;
  HWND button_register;
} settings_state_t;

static app_state_t *g_app = NULL;
static HWND g_hwnd = NULL;
static HWND g_status_label = NULL;
static HWND g_line_label = NULL;
static HWND g_dial_edit = NULL;
static HWND g_button_hold = NULL;
static HWND g_button_record = NULL;
static HWND g_button_mute = NULL;
static HWND g_button_warm = NULL;
static HWND g_history_list = NULL;
static HWND g_button_call_selected = NULL;
static NOTIFYICONDATAA g_tray_icon;
static int g_tray_added = 0;
static int g_allow_close = 0;

static LRESULT CALLBACK ui_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static LRESULT CALLBACK settings_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
static void ui_handle_call(void);

static void ui_append_dial_text(const char *text) {
  char buffer[256];
  GetWindowTextA(g_dial_edit, buffer, sizeof(buffer));
  if (strlen(buffer) + strlen(text) + 1 >= sizeof(buffer)) {
    return;
  }
  strcat(buffer, text);
  SetWindowTextA(g_dial_edit, buffer);
}

static void ui_get_edit_text(HWND edit, char *out, size_t out_size) {
  if (!out || out_size == 0) {
    return;
  }
  if (!edit) {
    out[0] = '\0';
    return;
  }
  GetWindowTextA(edit, out, (int)out_size);
  out[out_size - 1] = '\0';
}

static void ui_show_main_window(void) {
  if (!g_hwnd) {
    return;
  }
  ShowWindow(g_hwnd, SW_SHOW);
  SetForegroundWindow(g_hwnd);
}

static void ui_hide_main_window(void) {
  if (!g_hwnd) {
    return;
  }
  ShowWindow(g_hwnd, SW_HIDE);
}

static void ui_remove_tray_icon(void) {
  if (!g_tray_added) {
    return;
  }
  Shell_NotifyIconA(NIM_DELETE, &g_tray_icon);
  g_tray_added = 0;
}

static void ui_init_tray_icon(void) {
  if (!g_hwnd) {
    return;
  }
  memset(&g_tray_icon, 0, sizeof(g_tray_icon));
  g_tray_icon.cbSize = sizeof(g_tray_icon);
  g_tray_icon.hWnd = g_hwnd;
  g_tray_icon.uID = ID_TRAY_ICON;
  g_tray_icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
  g_tray_icon.uCallbackMessage = WM_APP_TRAY;
  g_tray_icon.hIcon = LoadIcon(NULL, IDI_APPLICATION);
  strcpy(g_tray_icon.szTip, APP_TITLE);
  if (Shell_NotifyIconA(NIM_ADD, &g_tray_icon)) {
    g_tray_added = 1;
  }
}

static void ui_show_tray_menu(void) {
  HMENU menu;
  POINT pos;
  if (!g_hwnd) {
    return;
  }
  menu = CreatePopupMenu();
  if (!menu) {
    return;
  }
  AppendMenuA(menu, MF_STRING, ID_TRAY_OPEN, "Open WinSoftphone");
  AppendMenuA(menu, MF_STRING, ID_TRAY_EXIT, "Exit");
  GetCursorPos(&pos);
  SetForegroundWindow(g_hwnd);
  TrackPopupMenu(menu, TPM_RIGHTBUTTON, pos.x, pos.y, 0, g_hwnd, NULL);
  DestroyMenu(menu);
}

static void ui_exit_application(void) {
  if (!g_hwnd) {
    return;
  }
  if (g_app) {
    app_shutdown(g_app);
  }
  g_allow_close = 1;
  DestroyWindow(g_hwnd);
}

static void ui_format_duration(int duration_sec, char *out, size_t out_size) {
  int minutes;
  int seconds;
  if (!out || out_size == 0) {
    return;
  }
  minutes = duration_sec / 60;
  seconds = duration_sec % 60;
  snprintf(out, out_size, "%02d:%02d", minutes, seconds);
}

static void ui_refresh_history_list(void) {
  if (!g_history_list || !g_app) {
    return;
  }
  ListView_DeleteAllItems(g_history_list);
  for (size_t i = 0; i < g_app->history.count; i++) {
    char duration[32];
    const history_entry_t *entry = &g_app->history.items[i];
    LVITEMA item;
    memset(&item, 0, sizeof(item));
    item.mask = LVIF_TEXT;
    item.iItem = (int)i;
    item.pszText = (LPSTR)entry->number;
    ListView_InsertItem(g_history_list, &item);
    ListView_SetItemText(g_history_list, (int)i, 1, (LPSTR)entry->cname);
    ListView_SetItemText(g_history_list, (int)i, 2, (LPSTR)entry->direction);
    ListView_SetItemText(g_history_list, (int)i, 3, (LPSTR)entry->timestamp);
    ui_format_duration(entry->duration_sec, duration, sizeof(duration));
    ListView_SetItemText(g_history_list, (int)i, 4, duration);
  }
}

static void ui_init_history_list(HWND parent) {
  LVCOLUMNA col;
  if (!parent) {
    return;
  }
  g_history_list = CreateWindowExA(
      WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
      WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
      520, 60, 340, 320, parent, (HMENU)(UINT_PTR)ID_HISTORY_LIST,
      g_app->instance, NULL);
  if (!g_history_list) {
    return;
  }
  ListView_SetExtendedListViewStyle(g_history_list,
                                    LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
  memset(&col, 0, sizeof(col));
  col.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
  col.pszText = "Number";
  col.cx = 110;
  ListView_InsertColumn(g_history_list, 0, &col);
  col.pszText = "Name";
  col.cx = 120;
  col.iSubItem = 1;
  ListView_InsertColumn(g_history_list, 1, &col);
  col.pszText = "Direction";
  col.cx = 80;
  col.iSubItem = 2;
  ListView_InsertColumn(g_history_list, 2, &col);
  col.pszText = "Time";
  col.cx = 140;
  col.iSubItem = 3;
  ListView_InsertColumn(g_history_list, 3, &col);
  col.pszText = "Duration";
  col.cx = 70;
  col.iSubItem = 4;
  ListView_InsertColumn(g_history_list, 4, &col);
}

static void ui_call_selected_history(void) {
  int selected;
  char number[128];
  if (!g_history_list) {
    return;
  }
  selected = ListView_GetNextItem(g_history_list, -1, LVNI_SELECTED);
  if (selected == -1) {
    ui_set_status_text("Select a history entry");
    return;
  }
  ListView_GetItemText(g_history_list, selected, 0, number, (int)sizeof(number));
  number[sizeof(number) - 1] = '\0';
  if (number[0] == '\0') {
    ui_set_status_text("Selected entry has no number");
    return;
  }
  SetWindowTextA(g_dial_edit, number);
  ui_handle_call();
}

static void ui_handle_dtmf(const char *digit) {
  if (!digit) {
    return;
  }
  ui_append_dial_text(digit);
  if (g_app && g_app->sip.active_call_id != PJSUA_INVALID_ID) {
    sip_send_dtmf(&g_app->sip, digit);
  }
}

static void ui_handle_call(void) {
  char dial[256];
  GetWindowTextA(g_dial_edit, dial, sizeof(dial));
  if (dial[0] == '\0') {
    ui_set_status_text("Enter destination number");
    return;
  }
  sip_make_call(&g_app->sip, dial);
}

static void ui_handle_hangup(void) {
  sip_hangup(&g_app->sip);
}

static void ui_handle_hold(void) {
  if (!g_app) {
    return;
  }
  if (g_app->call_on_hold) {
    sip_unhold(&g_app->sip);
    g_app->call_on_hold = 0;
    SetWindowTextA(g_button_hold, "Hold");
  } else {
    sip_hold(&g_app->sip);
    g_app->call_on_hold = 1;
    SetWindowTextA(g_button_hold, "Resume");
  }
}

static void ui_handle_warm_transfer(void) {
  char dial[256];
  if (!g_app) {
    return;
  }
  GetWindowTextA(g_dial_edit, dial, sizeof(dial));
  if (dial[0] == '\0') {
    ui_set_status_text("Enter transfer target");
    return;
  }
  if (g_app->sip.warm_transfer_call_id == PJSUA_INVALID_ID) {
    sip_start_warm_transfer(&g_app->sip, dial);
    SetWindowTextA(g_button_warm, "Complete Xfer");
    ui_set_status_text("Warm transfer started");
  } else {
    sip_complete_warm_transfer(&g_app->sip);
    g_app->sip.warm_transfer_call_id = PJSUA_INVALID_ID;
    SetWindowTextA(g_button_warm, "Warm Transfer");
    ui_set_status_text("Warm transfer completed");
  }
}

static void ui_handle_blind_transfer(void) {
  char dial[256];
  GetWindowTextA(g_dial_edit, dial, sizeof(dial));
  if (dial[0] == '\0') {
    ui_set_status_text("Enter transfer target");
    return;
  }
  sip_transfer_blind(&g_app->sip, dial);
}

static void ui_handle_record(void) {
  char recording_path[MAX_PATH];
  if (!g_app) {
    return;
  }
  if (!g_app->recording) {
    if (app_build_recording_path(g_app, recording_path, sizeof(recording_path)) != 0) {
      ui_set_status_text("Failed to create recording file");
      return;
    }
    if (sip_start_recording(&g_app->sip, recording_path) == 0) {
      g_app->recording = 1;
      SetWindowTextA(g_button_record, "Stop Rec");
      ui_set_status_text("Recording started");
    }
  } else {
    sip_stop_recording(&g_app->sip);
    g_app->recording = 0;
    SetWindowTextA(g_button_record, "Record");
    ui_set_status_text("Recording stopped");
  }
}

static void ui_handle_mute(void) {
  if (!g_app) {
    return;
  }
  g_app->muted = !g_app->muted;
  sip_set_mute(&g_app->sip, g_app->muted);
  SetWindowTextA(g_button_mute, g_app->muted ? "Unmute" : "Mute");
}

static void ui_fill_device_list(HWND list, int require_input, int require_output) {
  sip_audio_device_t devices[64];
  unsigned count = (unsigned)(sizeof(devices) / sizeof(devices[0]));
  if (sip_list_audio_devices(devices, &count) != 0) {
    return;
  }
  SendMessage(list, LB_RESETCONTENT, 0, 0);
  for (unsigned i = 0; i < count; i++) {
    if (require_input && devices[i].input_count == 0) {
      continue;
    }
    if (require_output && devices[i].output_count == 0) {
      continue;
    }
    int idx = (int)SendMessageA(list, LB_ADDSTRING, 0, (LPARAM)devices[i].name);
    SendMessage(list, LB_SETITEMDATA, idx, (LPARAM)devices[i].id);
  }
}

static void ui_select_device(HWND list, int device_id) {
  int count = (int)SendMessage(list, LB_GETCOUNT, 0, 0);
  for (int i = 0; i < count; i++) {
    int id = (int)SendMessage(list, LB_GETITEMDATA, i, 0);
    if (id == device_id) {
      SendMessage(list, LB_SETCURSEL, i, 0);
      return;
    }
  }
}

static void ui_open_settings_window(void) {
  const char *class_name = "WinSoftphoneSettings";
  HWND hwnd = CreateWindowExA(
      0, class_name, "Settings",
      WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
      CW_USEDEFAULT, CW_USEDEFAULT, 560, 520,
      g_hwnd, NULL, g_app->instance, NULL);
  ShowWindow(hwnd, SW_SHOW);
}

static void settings_apply(settings_state_t *state) {
  int capture_sel = (int)SendMessage(state->list_capture, LB_GETCURSEL, 0, 0);
  int playback_sel = (int)SendMessage(state->list_playback, LB_GETCURSEL, 0, 0);
  int capture_id = g_app->config.capture_device;
  int playback_id = g_app->config.playback_device;
  int rx_pos = (int)SendMessage(state->slider_rx, TBM_GETPOS, 0, 0);
  int tx_pos = (int)SendMessage(state->slider_tx, TBM_GETPOS, 0, 0);

  if (capture_sel != LB_ERR) {
    capture_id = (int)SendMessage(state->list_capture, LB_GETITEMDATA, capture_sel, 0);
  }
  if (playback_sel != LB_ERR) {
    playback_id = (int)SendMessage(state->list_playback, LB_GETITEMDATA, playback_sel, 0);
  }

  g_app->config.capture_device = capture_id;
  g_app->config.playback_device = playback_id;
  g_app->config.rx_level = rx_pos / 100.0f;
  g_app->config.tx_level = tx_pos / 100.0f;
  g_app->sip.last_tx_level = g_app->config.tx_level;
  ui_get_edit_text(state->edit_domain, g_app->config.sip_domain,
                   sizeof(g_app->config.sip_domain));
  ui_get_edit_text(state->edit_user, g_app->config.sip_user,
                   sizeof(g_app->config.sip_user));
  ui_get_edit_text(state->edit_password, g_app->config.sip_password,
                   sizeof(g_app->config.sip_password));
  ui_get_edit_text(state->edit_proxy, g_app->config.sip_proxy,
                   sizeof(g_app->config.sip_proxy));
  ui_get_edit_text(state->edit_transport, g_app->config.sip_transport,
                   sizeof(g_app->config.sip_transport));
  sip_set_audio_devices(capture_id, playback_id);
  sip_set_volume_levels(g_app->config.rx_level, g_app->config.tx_level);
  config_save(&g_app->config);
  ui_set_status_text("Settings updated");
}

static void settings_register(settings_state_t *state) {
  if (!g_app || !state) {
    return;
  }
  ui_get_edit_text(state->edit_domain, g_app->config.sip_domain,
                   sizeof(g_app->config.sip_domain));
  ui_get_edit_text(state->edit_user, g_app->config.sip_user,
                   sizeof(g_app->config.sip_user));
  ui_get_edit_text(state->edit_password, g_app->config.sip_password,
                   sizeof(g_app->config.sip_password));
  ui_get_edit_text(state->edit_proxy, g_app->config.sip_proxy,
                   sizeof(g_app->config.sip_proxy));
  ui_get_edit_text(state->edit_transport, g_app->config.sip_transport,
                   sizeof(g_app->config.sip_transport));
  config_save(&g_app->config);
  if (sip_register_account(&g_app->sip,
                           g_app->config.sip_domain,
                           g_app->config.sip_user,
                           g_app->config.sip_password,
                           g_app->config.sip_proxy,
                           g_app->config.sip_transport) == 0) {
    ui_set_status_text("Registration sent");
  } else {
    ui_set_status_text("Registration failed");
  }
}

static LRESULT CALLBACK settings_wnd_proc(HWND hwnd, UINT msg,
                                          WPARAM wparam, LPARAM lparam) {
  settings_state_t *state =
      (settings_state_t *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  switch (msg) {
    case WM_CREATE: {
      state = (settings_state_t *)calloc(1, sizeof(settings_state_t));
      SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)state);
      state->hwnd = hwnd;

      CreateWindowExA(0, "STATIC", "Microphone",
                      WS_CHILD | WS_VISIBLE,
                      20, 20, 200, 20, hwnd, NULL, g_app->instance, NULL);
      state->list_capture = CreateWindowExA(
          WS_EX_CLIENTEDGE, "LISTBOX", "",
          WS_CHILD | WS_VISIBLE | LBS_NOTIFY,
          20, 45, 200, 120, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_CAPTURE,
          g_app->instance, NULL);

      CreateWindowExA(0, "STATIC", "Speakers",
                      WS_CHILD | WS_VISIBLE,
                      240, 20, 200, 20, hwnd, NULL, g_app->instance, NULL);
      state->list_playback = CreateWindowExA(
          WS_EX_CLIENTEDGE, "LISTBOX", "",
          WS_CHILD | WS_VISIBLE | LBS_NOTIFY,
          240, 45, 200, 120, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_PLAYBACK,
          g_app->instance, NULL);

      CreateWindowExA(0, "STATIC", "Playback Volume",
                      WS_CHILD | WS_VISIBLE,
                      20, 175, 200, 20, hwnd, NULL, g_app->instance, NULL);
      state->slider_rx = CreateWindowExA(
          0, TRACKBAR_CLASS, "",
          WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
          20, 195, 200, 30, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_RX,
          g_app->instance, NULL);
      SendMessage(state->slider_rx, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
      SendMessage(state->slider_rx, TBM_SETPOS, TRUE,
                  (LPARAM)(g_app->config.rx_level * 100));

      CreateWindowExA(0, "STATIC", "Mic Volume",
                      WS_CHILD | WS_VISIBLE,
                      240, 175, 200, 20, hwnd, NULL, g_app->instance, NULL);
      state->slider_tx = CreateWindowExA(
          0, TRACKBAR_CLASS, "",
          WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
          240, 195, 200, 30, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_TX,
          g_app->instance, NULL);
      SendMessage(state->slider_tx, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
      SendMessage(state->slider_tx, TBM_SETPOS, TRUE,
                  (LPARAM)(g_app->config.tx_level * 100));

      CreateWindowExA(0, "BUTTON", "Account",
                      WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                      20, 235, 420, 190, hwnd, NULL, g_app->instance, NULL);
      CreateWindowExA(0, "STATIC", "Domain",
                      WS_CHILD | WS_VISIBLE,
                      30, 260, 100, 18, hwnd, NULL, g_app->instance, NULL);
      state->edit_domain = CreateWindowExA(
          WS_EX_CLIENTEDGE, "EDIT", "",
          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
          140, 258, 280, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_DOMAIN,
          g_app->instance, NULL);
      CreateWindowExA(0, "STATIC", "User",
                      WS_CHILD | WS_VISIBLE,
                      30, 288, 100, 18, hwnd, NULL, g_app->instance, NULL);
      state->edit_user = CreateWindowExA(
          WS_EX_CLIENTEDGE, "EDIT", "",
          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
          140, 286, 280, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_USER,
          g_app->instance, NULL);
      CreateWindowExA(0, "STATIC", "Password",
                      WS_CHILD | WS_VISIBLE,
                      30, 316, 100, 18, hwnd, NULL, g_app->instance, NULL);
      state->edit_password = CreateWindowExA(
          WS_EX_CLIENTEDGE, "EDIT", "",
          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
          140, 314, 280, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_PASSWORD,
          g_app->instance, NULL);
      CreateWindowExA(0, "STATIC", "Proxy",
                      WS_CHILD | WS_VISIBLE,
                      30, 344, 100, 18, hwnd, NULL, g_app->instance, NULL);
      state->edit_proxy = CreateWindowExA(
          WS_EX_CLIENTEDGE, "EDIT", "",
          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
          140, 342, 280, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_PROXY,
          g_app->instance, NULL);
      CreateWindowExA(0, "STATIC", "Transport",
                      WS_CHILD | WS_VISIBLE,
                      30, 372, 100, 18, hwnd, NULL, g_app->instance, NULL);
      state->edit_transport = CreateWindowExA(
          WS_EX_CLIENTEDGE, "EDIT", "",
          WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
          140, 370, 120, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_TRANSPORT,
          g_app->instance, NULL);
      state->button_register = CreateWindowExA(
          0, "BUTTON", "Register",
          WS_CHILD | WS_VISIBLE,
          280, 368, 140, 26, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_REGISTER,
          g_app->instance, NULL);

      CreateWindowExA(0, "BUTTON", "Apply",
                      WS_CHILD | WS_VISIBLE,
                      240, 440, 90, 30, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_APPLY,
                      g_app->instance, NULL);
      CreateWindowExA(0, "BUTTON", "Close",
                      WS_CHILD | WS_VISIBLE,
                      350, 440, 90, 30, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_CLOSE,
                      g_app->instance, NULL);

      ui_fill_device_list(state->list_capture, 1, 0);
      ui_fill_device_list(state->list_playback, 0, 1);
      ui_select_device(state->list_capture, g_app->config.capture_device);
      ui_select_device(state->list_playback, g_app->config.playback_device);
      SetWindowTextA(state->edit_domain, g_app->config.sip_domain);
      SetWindowTextA(state->edit_user, g_app->config.sip_user);
      SetWindowTextA(state->edit_password, g_app->config.sip_password);
      SetWindowTextA(state->edit_proxy, g_app->config.sip_proxy);
      SetWindowTextA(state->edit_transport, g_app->config.sip_transport);
      break;
    }
    case WM_COMMAND: {
      switch (LOWORD(wparam)) {
        case ID_SETTINGS_APPLY:
          settings_apply(state);
          break;
        case ID_SETTINGS_REGISTER:
          settings_register(state);
          break;
        case ID_SETTINGS_CLOSE:
          DestroyWindow(hwnd);
          break;
        default:
          break;
      }
      break;
    }
    case WM_DESTROY:
      free(state);
      break;
    default:
      break;
  }
  return DefWindowProc(hwnd, msg, wparam, lparam);
}

static int ui_register_classes(void) {
  WNDCLASSA wc;
  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = ui_wnd_proc;
  wc.hInstance = g_app->instance;
  wc.lpszClassName = APP_TITLE;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  if (!RegisterClassA(&wc)) {
    return -1;
  }

  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = settings_wnd_proc;
  wc.hInstance = g_app->instance;
  wc.lpszClassName = "WinSoftphoneSettings";
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
  if (!RegisterClassA(&wc)) {
    return -1;
  }
  return 0;
}

int ui_init(app_state_t *app) {
  HMENU menu;
  const char *dtmf_labels[] = {"1", "2", "3", "4", "5", "6",
                               "7", "8", "9", "*", "0", "#"};
  int start_x = 20;
  int start_y = 100;
  int btn_w = 50;
  int btn_h = 40;
  INITCOMMONCONTROLSEX icex;

  if (!app) {
    return -1;
  }
  g_app = app;
  icex.dwSize = sizeof(icex);
  icex.dwICC = ICC_LISTVIEW_CLASSES | ICC_BAR_CLASSES;
  InitCommonControlsEx(&icex);
  if (ui_register_classes() != 0) {
    return -1;
  }

  g_hwnd = CreateWindowExA(
      0, APP_TITLE, APP_TITLE,
      WS_OVERLAPPED | WS_SYSMENU | WS_MINIMIZEBOX,
      CW_USEDEFAULT, CW_USEDEFAULT, 900, 520,
      NULL, NULL, g_app->instance, NULL);
  if (!g_hwnd) {
    return -1;
  }
  g_app->main_hwnd = g_hwnd;

  menu = CreateMenu();
  AppendMenuA(menu, MF_STRING, ID_MENU_SETTINGS, "Settings");
  AppendMenuA(menu, MF_STRING, ID_MENU_EXIT, "Exit");
  SetMenu(g_hwnd, menu);

  g_status_label = CreateWindowExA(
      0, "STATIC", "Idle",
      WS_CHILD | WS_VISIBLE,
      20, 10, 480, 20, g_hwnd, (HMENU)(UINT_PTR)ID_STATUS_TEXT,
      g_app->instance, NULL);
  g_line_label = CreateWindowExA(
      0, "STATIC", "Line: Ready",
      WS_CHILD | WS_VISIBLE,
      20, 30, 480, 20, g_hwnd, (HMENU)(UINT_PTR)ID_LINE_TEXT,
      g_app->instance, NULL);
  g_dial_edit = CreateWindowExA(
      WS_EX_CLIENTEDGE, "EDIT", "",
      WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
      20, 60, 240, 24, g_hwnd, (HMENU)(UINT_PTR)ID_EDIT_DIAL,
      g_app->instance, NULL);

  for (int i = 0; i < 12; i++) {
    int row = i / 3;
    int col = i % 3;
    CreateWindowExA(
        0, "BUTTON", dtmf_labels[i],
        WS_CHILD | WS_VISIBLE,
        start_x + col * (btn_w + 10),
        start_y + row * (btn_h + 10),
        btn_w, btn_h, g_hwnd, (HMENU)(UINT_PTR)(ID_DTMF_BASE + i),
        g_app->instance, NULL);
  }

  CreateWindowExA(
      0, "BUTTON", "Call",
      WS_CHILD | WS_VISIBLE,
      300, 60, 180, 30, g_hwnd, (HMENU)(UINT_PTR)ID_BUTTON_CALL,
      g_app->instance, NULL);
  CreateWindowExA(
      0, "BUTTON", "Hangup",
      WS_CHILD | WS_VISIBLE,
      300, 95, 180, 30, g_hwnd, (HMENU)(UINT_PTR)ID_BUTTON_HANGUP,
      g_app->instance, NULL);
  g_button_hold = CreateWindowExA(
      0, "BUTTON", "Hold",
      WS_CHILD | WS_VISIBLE,
      300, 130, 180, 30, g_hwnd, (HMENU)(UINT_PTR)ID_BUTTON_HOLD,
      g_app->instance, NULL);
  g_button_warm = CreateWindowExA(
      0, "BUTTON", "Warm Transfer",
      WS_CHILD | WS_VISIBLE,
      300, 165, 180, 30, g_hwnd, (HMENU)(UINT_PTR)ID_BUTTON_WARM_TRANSFER,
      g_app->instance, NULL);
  CreateWindowExA(
      0, "BUTTON", "Blind Transfer",
      WS_CHILD | WS_VISIBLE,
      300, 200, 180, 30, g_hwnd, (HMENU)(UINT_PTR)ID_BUTTON_BLIND_TRANSFER,
      g_app->instance, NULL);
  g_button_record = CreateWindowExA(
      0, "BUTTON", "Record",
      WS_CHILD | WS_VISIBLE,
      300, 235, 180, 30, g_hwnd, (HMENU)(UINT_PTR)ID_BUTTON_RECORD,
      g_app->instance, NULL);
  g_button_mute = CreateWindowExA(
      0, "BUTTON", "Mute",
      WS_CHILD | WS_VISIBLE,
      300, 270, 180, 30, g_hwnd, (HMENU)(UINT_PTR)ID_BUTTON_MUTE,
      g_app->instance, NULL);

  ui_init_history_list(g_hwnd);
  g_button_call_selected = CreateWindowExA(
      0, "BUTTON", "Call Selected",
      WS_CHILD | WS_VISIBLE,
      520, 390, 340, 30, g_hwnd, (HMENU)(UINT_PTR)ID_BUTTON_CALL_SELECTED,
      g_app->instance, NULL);
  ui_refresh_history_list();

  ui_init_tray_icon();
  ShowWindow(g_hwnd, SW_SHOW);
  return 0;
}

int ui_run(void) {
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0) > 0) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
  return (int)msg.wParam;
}

void ui_set_status_text(const char *text) {
  if (!g_hwnd || !text) {
    return;
  }
  PostMessage(g_hwnd, WM_APP_STATUS, 0, (LPARAM)_strdup(text));
}

void ui_set_line_text(const char *text) {
  if (!g_hwnd || !text) {
    return;
  }
  PostMessage(g_hwnd, WM_APP_LINE, 0, (LPARAM)_strdup(text));
}

void ui_open_url(const char *url) {
  if (!g_hwnd || !url) {
    return;
  }
  PostMessage(g_hwnd, WM_APP_OPEN_URL, 0, (LPARAM)_strdup(url));
}

void ui_refresh_history(void) {
  ui_refresh_history_list();
}

static LRESULT CALLBACK ui_wnd_proc(HWND hwnd, UINT msg,
                                    WPARAM wparam, LPARAM lparam) {
  switch (msg) {
    case WM_COMMAND: {
      int id = LOWORD(wparam);
      if (id >= ID_DTMF_BASE && id < ID_DTMF_BASE + 12) {
        static const char *digits[] = {"1", "2", "3", "4", "5", "6",
                                       "7", "8", "9", "*", "0", "#"};
        ui_handle_dtmf(digits[id - ID_DTMF_BASE]);
      } else {
        switch (id) {
          case ID_BUTTON_CALL:
            ui_handle_call();
            break;
          case ID_BUTTON_HANGUP:
            ui_handle_hangup();
            break;
          case ID_BUTTON_HOLD:
            ui_handle_hold();
            break;
          case ID_BUTTON_WARM_TRANSFER:
            ui_handle_warm_transfer();
            break;
          case ID_BUTTON_BLIND_TRANSFER:
            ui_handle_blind_transfer();
            break;
          case ID_BUTTON_RECORD:
            ui_handle_record();
            break;
          case ID_BUTTON_MUTE:
            ui_handle_mute();
            break;
          case ID_BUTTON_CALL_SELECTED:
            ui_call_selected_history();
            break;
          case ID_MENU_SETTINGS:
            ui_open_settings_window();
            break;
          case ID_MENU_EXIT:
            ui_exit_application();
            break;
          case ID_TRAY_OPEN:
            ui_show_main_window();
            break;
          case ID_TRAY_EXIT:
            ui_exit_application();
            break;
          default:
            break;
        }
      }
      break;
    }
    case WM_NOTIFY: {
      LPNMHDR header = (LPNMHDR)lparam;
      if (header && header->idFrom == ID_HISTORY_LIST &&
          header->code == NM_DBLCLK) {
        ui_call_selected_history();
      }
      break;
    }
    case WM_APP_STATUS: {
      char *text = (char *)lparam;
      if (text) {
        SetWindowTextA(g_status_label, text);
        free(text);
      }
      break;
    }
    case WM_APP_LINE: {
      char *text = (char *)lparam;
      if (text) {
        SetWindowTextA(g_line_label, text);
        free(text);
      }
      break;
    }
    case WM_APP_OPEN_URL: {
      char *url = (char *)lparam;
      if (url) {
        ShellExecuteA(hwnd, "open", url, NULL, NULL, SW_SHOWNORMAL);
        free(url);
      }
      break;
    }
    case WM_APP_TRAY: {
      if (lparam == WM_LBUTTONDBLCLK) {
        ui_show_main_window();
      } else if (lparam == WM_RBUTTONUP || lparam == WM_CONTEXTMENU) {
        ui_show_tray_menu();
      }
      break;
    }
    case WM_CLOSE:
      if (g_allow_close) {
        DestroyWindow(hwnd);
      } else {
        ui_hide_main_window();
        ui_set_status_text("WinSoftphone minimized to tray");
      }
      return 0;
    case WM_DESTROY:
      ui_remove_tray_icon();
      PostQuitMessage(0);
      break;
    default:
      break;
  }
  return DefWindowProc(hwnd, msg, wparam, lparam);
}
