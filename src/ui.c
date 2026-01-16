#include "ui.h"

#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
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
#define ID_SETTINGS_TAB 506
#define ID_SETTINGS_DOMAIN 510
#define ID_SETTINGS_USER 511
#define ID_SETTINGS_PASSWORD 512
#define ID_SETTINGS_PROXY 513
#define ID_SETTINGS_TRANSPORT 514
#define ID_SETTINGS_REGISTER 515
#define ID_SETTINGS_URL_TEMPLATE 516
#define ID_SETTINGS_AUTO_RECORD 517
#define ID_SETTINGS_RECORD_PATH 518
#define ID_SETTINGS_RECORD_BROWSE 519

#define ID_HISTORY_LIST 700

#define ID_TRAY_ICON 800
#define ID_TRAY_OPEN 801
#define ID_TRAY_EXIT 802
#define ID_REG_INDICATOR 803

#define WM_APP_STATUS (WM_APP + 1)
#define WM_APP_LINE (WM_APP + 2)
#define WM_APP_OPEN_URL (WM_APP + 3)
#define WM_APP_TRAY (WM_APP + 4)

typedef struct {
  HWND hwnd;
  HWND tab;
  HWND audio_controls[48];
  int audio_count;
  HWND automation_controls[48];
  int automation_count;
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
  HWND edit_url_template;
  HWND check_auto_record;
  HWND edit_record_path;
  HWND button_record_browse;
} settings_state_t;

static app_state_t *g_app = NULL;
static HWND g_hwnd = NULL;
static HWND g_status_label = NULL;
static HWND g_line_label = NULL;
static HWND g_reg_indicator = NULL;
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
static int g_reg_status = 0;
static HBRUSH g_bg_brush = NULL;
static HBRUSH g_edit_brush = NULL;
static COLORREF g_bg_color = RGB(30, 30, 30);
static COLORREF g_edit_color = RGB(45, 45, 48);
static COLORREF g_text_color = RGB(229, 229, 229);
static COLORREF g_muted_text = RGB(153, 153, 153);

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

static void settings_register_control(HWND *list, int *count, HWND control) {
  if (!control || !list || !count) {
    return;
  }
  if (*count >= 48) {
    return;
  }
  list[(*count)++] = control;
}

static void settings_show_tab(settings_state_t *state, int index) {
  int show_audio = (index == 0);
  int show_auto = (index == 1);
  for (int i = 0; i < state->audio_count; i++) {
    ShowWindow(state->audio_controls[i], show_audio ? SW_SHOW : SW_HIDE);
  }
  for (int i = 0; i < state->automation_count; i++) {
    ShowWindow(state->automation_controls[i], show_auto ? SW_SHOW : SW_HIDE);
  }
}

static int ui_browse_for_folder(char *out_path, size_t out_size) {
  BROWSEINFOA bi;
  LPITEMIDLIST pidl;
  if (!out_path || out_size == 0) {
    return -1;
  }
  memset(&bi, 0, sizeof(bi));
  bi.lpszTitle = "Select recording folder";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_USENEWUI;
  pidl = SHBrowseForFolderA(&bi);
  if (!pidl) {
    return -1;
  }
  if (!SHGetPathFromIDListA(pidl, out_path)) {
    CoTaskMemFree(pidl);
    return -1;
  }
  CoTaskMemFree(pidl);
  out_path[out_size - 1] = '\0';
  return 0;
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
      CW_USEDEFAULT, CW_USEDEFAULT, 640, 600,
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
  ui_get_edit_text(state->edit_url_template, g_app->config.incoming_url_template,
                   sizeof(g_app->config.incoming_url_template));
  ui_get_edit_text(state->edit_record_path, g_app->config.recording_base_dir,
                   sizeof(g_app->config.recording_base_dir));
  g_app->config.auto_record = (SendMessage(state->check_auto_record,
                                           BM_GETCHECK, 0, 0) == BST_CHECKED);
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

      state->tab = CreateWindowExA(
          0, WC_TABCONTROLA, "",
          WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
          10, 10, 600, 460, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_TAB,
          g_app->instance, NULL);
      if (state->tab) {
        TCITEMA item;
        memset(&item, 0, sizeof(item));
        item.mask = TCIF_TEXT;
        item.pszText = "Audio & Account";
        TabCtrl_InsertItem(state->tab, 0, &item);
        item.pszText = "Automation";
        TabCtrl_InsertItem(state->tab, 1, &item);
      }

      {
        int left = 30;
        int right = 320;
        int top = 50;
        HWND label;
        label = CreateWindowExA(0, "STATIC", "Microphone",
                                WS_CHILD | WS_VISIBLE,
                                left, top, 200, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, label);
        state->list_capture = CreateWindowExA(
            WS_EX_CLIENTEDGE, "LISTBOX", "",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY,
            left, top + 20, 240, 120, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_CAPTURE,
            g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, state->list_capture);

        label = CreateWindowExA(0, "STATIC", "Speakers",
                                WS_CHILD | WS_VISIBLE,
                                right, top, 200, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, label);
        state->list_playback = CreateWindowExA(
            WS_EX_CLIENTEDGE, "LISTBOX", "",
            WS_CHILD | WS_VISIBLE | LBS_NOTIFY,
            right, top + 20, 240, 120, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_PLAYBACK,
            g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, state->list_playback);

        label = CreateWindowExA(0, "STATIC", "Playback Volume",
                                WS_CHILD | WS_VISIBLE,
                                left, top + 150, 200, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, label);
        state->slider_rx = CreateWindowExA(
            0, TRACKBAR_CLASS, "",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            left, top + 170, 240, 30, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_RX,
            g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, state->slider_rx);
        SendMessage(state->slider_rx, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
        SendMessage(state->slider_rx, TBM_SETPOS, TRUE,
                    (LPARAM)(g_app->config.rx_level * 100));

        label = CreateWindowExA(0, "STATIC", "Mic Volume",
                                WS_CHILD | WS_VISIBLE,
                                right, top + 150, 200, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, label);
        state->slider_tx = CreateWindowExA(
            0, TRACKBAR_CLASS, "",
            WS_CHILD | WS_VISIBLE | TBS_AUTOTICKS,
            right, top + 170, 240, 30, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_TX,
            g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, state->slider_tx);
        SendMessage(state->slider_tx, TBM_SETRANGE, TRUE, MAKELPARAM(0, 200));
        SendMessage(state->slider_tx, TBM_SETPOS, TRUE,
                    (LPARAM)(g_app->config.tx_level * 100));

        label = CreateWindowExA(0, "BUTTON", "Account",
                                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                left, top + 210, 530, 170, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, label);
        label = CreateWindowExA(0, "STATIC", "Domain",
                                WS_CHILD | WS_VISIBLE,
                                left + 10, top + 235, 100, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, label);
        state->edit_domain = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            left + 120, top + 233, 380, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_DOMAIN,
            g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, state->edit_domain);
        label = CreateWindowExA(0, "STATIC", "User",
                                WS_CHILD | WS_VISIBLE,
                                left + 10, top + 263, 100, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, label);
        state->edit_user = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            left + 120, top + 261, 380, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_USER,
            g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, state->edit_user);
        label = CreateWindowExA(0, "STATIC", "Password",
                                WS_CHILD | WS_VISIBLE,
                                left + 10, top + 291, 100, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, label);
        state->edit_password = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_PASSWORD,
            left + 120, top + 289, 380, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_PASSWORD,
            g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, state->edit_password);
        label = CreateWindowExA(0, "STATIC", "Proxy",
                                WS_CHILD | WS_VISIBLE,
                                left + 10, top + 319, 100, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, label);
        state->edit_proxy = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            left + 120, top + 317, 380, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_PROXY,
            g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, state->edit_proxy);
        label = CreateWindowExA(0, "STATIC", "Transport",
                                WS_CHILD | WS_VISIBLE,
                                left + 10, top + 347, 100, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, label);
        state->edit_transport = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            left + 120, top + 345, 140, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_TRANSPORT,
            g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, state->edit_transport);
        state->button_register = CreateWindowExA(
            0, "BUTTON", "Register",
            WS_CHILD | WS_VISIBLE,
            left + 280, top + 343, 120, 26, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_REGISTER,
            g_app->instance, NULL);
        settings_register_control(state->audio_controls, &state->audio_count, state->button_register);
      }

      {
        int left = 30;
        int top = 50;
        HWND label;
        label = CreateWindowExA(0, "BUTTON", "Automation",
                                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                left, top, 530, 140, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->automation_controls, &state->automation_count, label);
        label = CreateWindowExA(0, "STATIC", "Incoming URL Template",
                                WS_CHILD | WS_VISIBLE,
                                left + 10, top + 25, 200, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->automation_controls, &state->automation_count, label);
        state->edit_url_template = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            left + 10, top + 45, 500, 22, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_URL_TEMPLATE,
            g_app->instance, NULL);
        settings_register_control(state->automation_controls, &state->automation_count, state->edit_url_template);

        label = CreateWindowExA(0, "BUTTON", "Recording",
                                WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
                                left, top + 160, 530, 160, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->automation_controls, &state->automation_count, label);
        state->check_auto_record = CreateWindowExA(
            0, "BUTTON", "Record all calls automatically",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            left + 10, top + 185, 300, 20, hwnd,
            (HMENU)(UINT_PTR)ID_SETTINGS_AUTO_RECORD,
            g_app->instance, NULL);
        settings_register_control(state->automation_controls, &state->automation_count, state->check_auto_record);
        label = CreateWindowExA(0, "STATIC", "Recording Folder",
                                WS_CHILD | WS_VISIBLE,
                                left + 10, top + 215, 200, 18, hwnd, NULL,
                                g_app->instance, NULL);
        settings_register_control(state->automation_controls, &state->automation_count, label);
        state->edit_record_path = CreateWindowExA(
            WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            left + 10, top + 235, 380, 22, hwnd,
            (HMENU)(UINT_PTR)ID_SETTINGS_RECORD_PATH,
            g_app->instance, NULL);
        settings_register_control(state->automation_controls, &state->automation_count, state->edit_record_path);
        state->button_record_browse = CreateWindowExA(
            0, "BUTTON", "Browse",
            WS_CHILD | WS_VISIBLE,
            left + 400, top + 233, 110, 26, hwnd,
            (HMENU)(UINT_PTR)ID_SETTINGS_RECORD_BROWSE,
            g_app->instance, NULL);
        settings_register_control(state->automation_controls, &state->automation_count, state->button_record_browse);
      }

      CreateWindowExA(0, "BUTTON", "Apply",
                      WS_CHILD | WS_VISIBLE,
                      360, 490, 90, 30, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_APPLY,
                      g_app->instance, NULL);
      CreateWindowExA(0, "BUTTON", "Close",
                      WS_CHILD | WS_VISIBLE,
                      470, 490, 90, 30, hwnd, (HMENU)(UINT_PTR)ID_SETTINGS_CLOSE,
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
      SetWindowTextA(state->edit_url_template, g_app->config.incoming_url_template);
      SetWindowTextA(state->edit_record_path, g_app->config.recording_base_dir);
      SendMessage(state->check_auto_record, BM_SETCHECK,
                  g_app->config.auto_record ? BST_CHECKED : BST_UNCHECKED, 0);
      settings_show_tab(state, 0);
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
        case ID_SETTINGS_RECORD_BROWSE: {
          char folder[MAX_PATH];
          if (ui_browse_for_folder(folder, sizeof(folder)) == 0) {
            SetWindowTextA(state->edit_record_path, folder);
          }
          break;
        }
        case ID_SETTINGS_CLOSE:
          DestroyWindow(hwnd);
          break;
        default:
          break;
      }
      break;
    }
    case WM_NOTIFY: {
      LPNMHDR header = (LPNMHDR)lparam;
      if (header && header->idFrom == ID_SETTINGS_TAB &&
          header->code == TCN_SELCHANGE) {
        int index = TabCtrl_GetCurSel(state->tab);
        settings_show_tab(state, index);
      }
      break;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
      HDC hdc = (HDC)wparam;
      COLORREF bg = g_bg_color;
      HBRUSH brush = g_bg_brush;
      if (msg == WM_CTLCOLOREDIT) {
        bg = g_edit_color;
        brush = g_edit_brush;
      }
      SetBkColor(hdc, bg);
      SetTextColor(hdc, g_text_color);
      return (LRESULT)brush;
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
  wc.hbrBackground = g_bg_brush ? g_bg_brush : (HBRUSH)(COLOR_WINDOW + 1);
  if (!RegisterClassA(&wc)) {
    return -1;
  }

  memset(&wc, 0, sizeof(wc));
  wc.lpfnWndProc = settings_wnd_proc;
  wc.hInstance = g_app->instance;
  wc.lpszClassName = "WinSoftphoneSettings";
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = g_bg_brush ? g_bg_brush : (HBRUSH)(COLOR_WINDOW + 1);
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
  if (!g_bg_brush) {
    g_bg_brush = CreateSolidBrush(g_bg_color);
  }
  if (!g_edit_brush) {
    g_edit_brush = CreateSolidBrush(g_edit_color);
  }
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
  g_reg_indicator = CreateWindowExA(
      0, "STATIC", "",
      WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
      20, 33, 12, 12, g_hwnd, (HMENU)(UINT_PTR)ID_REG_INDICATOR,
      g_app->instance, NULL);
  g_line_label = CreateWindowExA(
      0, "STATIC", "Line: Offline",
      WS_CHILD | WS_VISIBLE,
      38, 30, 480, 20, g_hwnd, (HMENU)(UINT_PTR)ID_LINE_TEXT,
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
  if (g_history_list) {
    ListView_SetBkColor(g_history_list, g_bg_color);
    ListView_SetTextBkColor(g_history_list, g_bg_color);
    ListView_SetTextColor(g_history_list, g_text_color);
  }
  g_button_call_selected = CreateWindowExA(
      0, "BUTTON", "Call Selected",
      WS_CHILD | WS_VISIBLE,
      520, 390, 340, 30, g_hwnd, (HMENU)(UINT_PTR)ID_BUTTON_CALL_SELECTED,
      g_app->instance, NULL);
  ui_refresh_history_list();

  ui_set_registration_status(0);
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

void ui_set_registration_status(int is_registered) {
  g_reg_status = is_registered ? 1 : 0;
  if (g_reg_indicator) {
    InvalidateRect(g_reg_indicator, NULL, TRUE);
  }
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
    case WM_DRAWITEM: {
      DRAWITEMSTRUCT *dis = (DRAWITEMSTRUCT *)lparam;
      if (dis && dis->CtlID == ID_REG_INDICATOR) {
        HBRUSH dot_brush = CreateSolidBrush(g_reg_status ? RGB(0, 200, 83)
                                                        : RGB(220, 53, 69));
        FillRect(dis->hDC, &dis->rcItem, g_bg_brush);
        HBRUSH old_brush = (HBRUSH)SelectObject(dis->hDC, dot_brush);
        HPEN pen = CreatePen(PS_SOLID, 1, g_bg_color);
        HPEN old_pen = (HPEN)SelectObject(dis->hDC, pen);
        Ellipse(dis->hDC,
                dis->rcItem.left + 1,
                dis->rcItem.top + 1,
                dis->rcItem.right - 1,
                dis->rcItem.bottom - 1);
        SelectObject(dis->hDC, old_pen);
        SelectObject(dis->hDC, old_brush);
        DeleteObject(pen);
        DeleteObject(dot_brush);
        return TRUE;
      }
      break;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT: {
      HDC hdc = (HDC)wparam;
      HWND ctrl = (HWND)lparam;
      COLORREF bg = g_bg_color;
      HBRUSH brush = g_bg_brush;
      if (msg == WM_CTLCOLOREDIT) {
        bg = g_edit_color;
        brush = g_edit_brush;
      }
      SetBkColor(hdc, bg);
      SetTextColor(hdc, g_text_color);
      if (ctrl == g_line_label) {
        SetTextColor(hdc, g_muted_text);
      }
      return (LRESULT)brush;
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
      if (g_bg_brush) {
        DeleteObject(g_bg_brush);
        g_bg_brush = NULL;
      }
      if (g_edit_brush) {
        DeleteObject(g_edit_brush);
        g_edit_brush = NULL;
      }
      PostQuitMessage(0);
      break;
    default:
      break;
  }
  return DefWindowProc(hwnd, msg, wparam, lparam);
}
