/*
 * Copyright (C) 2007-2011 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/reboot.h>
#include <unistd.h>
#include <cutils/log.h>

#include "common.h"
#include "extendedcommands.h"
#include "minui/minui.h"
#include "bootmenu_ui.h"

#define MODES_COUNT 5
const char* modes[] = {
  "bootmenu",
  "2nd-boot",
  "2nd-boot-uart",
  "2nd-system",
  "recovery",
};

// user friendly menu labels
#define LABEL_2NDBOOT    "2nd-boot"
#define LABEL_2NDBOOT_UART    "2nd-boot-uart"
#define LABEL_2NDSYSTEM  "2nd-system"

static int adbd_ready = 0;

/**
 * int_mode()
 *
 */
int int_mode(char * mode) {
  int m;
  for (m=0; m < MODES_COUNT; m++) {
    if (0 == strcmp(modes[m], mode)) {
      return m;
    }
  }
  return 0;
}

/**
 * snd_boot()
 */
int boot_mode(int ui, const char mode[]) {
  int status;
  int i;

  bypass_sign("yes");

  if (ui)
    ui_print("Start %s boot....\n", mode);
  else
    LOGI("Start %s boot....\n", mode);

  ui_stop_redraw();
      status = exec_script(mode, ui);
  ui_resume_redraw();

  if (status) {
    bypass_sign("no");
    return -1;
  }

  if (ui)
   ui_print("Wait 2 seconds....\n");
  else
    LOGI("Wait 2 seconds....\n");

  for(i = 2; i > 0; --i) {
    if (ui)
      ui_print("%d.\n", i);
    else
      LOGI("%d..\n", i);
    usleep(1000000);
  }

  bypass_sign("no");
  return 0;
}


/**
 * str_mode()
 *
 */
const char* str_mode(int mode) {
  if (mode >= 0 && mode < MODES_COUNT) {
    return modes[mode];
  }
  return "bootmenu";
}

/**
 * show_menu_boot()
 *
 */
int show_menu_boot(void) {

  #define BOOT_2NDBOOT    1
  #define BOOT_2NDBOOT_UART    2
  #define BOOT_2NDSYSTEM  3

  int status, res = 0;

  const char* headers[] = {
        " # Boot -->",
        "",
        NULL
  };
  char** title_headers = prepend_title(headers);

  struct UiMenuItem items[(6)] = {
    {MENUITEM_SMALL, "Set Default: [" LABEL_2NDBOOT "]", NULL},
    {MENUITEM_SMALL, LABEL_2NDBOOT, NULL},
    {MENUITEM_SMALL, LABEL_2NDBOOT_UART, NULL},
    {MENUITEM_SMALL, LABEL_2NDSYSTEM, NULL},

    {MENUITEM_SMALL, "<--Go Back", NULL},
    {MENUITEM_NULL, NULL, NULL},
  };

  char opt_def[64];
  char opt_adb[32];
  struct UiMenuResult ret;
  int bootmode;

  for (;;) {
    bootmode = get_default_bootmode();

    sprintf(opt_def, "Set Default: [%s]", str_mode(bootmode) );
    items[0].title = opt_def;

    //Hide unavailables modes
    if (!file_exists((char*) FILE_2NDSYSTEM)) {
        items[BOOT_2NDSYSTEM].title = "";
    }
    if (!file_exists((char*) FILE_2NDBOOT)) {
        items[BOOT_2NDBOOT].title = "";
    }
    if (!file_exists((char*) FILE_2NDBOOT_UART)) {
        items[BOOT_2NDBOOT_UART].title = "";
    }

    ret = get_menu_selection(title_headers, TABS, items, 1, 0);

    if (ret.result == GO_BACK) {
        goto exit_loop;
    }

    //Submenu: select default mode
    if (ret.result == 0) {
        show_config_bootmode();
        continue;
    }

    //Direct boot modes
    else if (ret.result == BOOT_2NDBOOT) {
        status = boot_mode(ENABLE, FILE_2NDBOOT);
        res = (status == 0);
        goto exit_loop;
    }
    else if (ret.result == BOOT_2NDBOOT_UART) {
        status = boot_mode(ENABLE, FILE_2NDBOOT_UART);
        res = (status == 0);
        goto exit_loop;
    }
    else if (ret.result == BOOT_2NDSYSTEM) {
        if (!file_exists((char*) FILE_2NDSYSTEM)) {
            LOGE("Script not found :\n%s\n", FILE_2NDSYSTEM);
            continue;
        }
    }
    else
    switch (ret.result) {
      default:
        goto exit_loop;
    }

  } //for

exit_loop:

  // free alloc by prepend_title()
  free_menu_headers(title_headers);

  return res;
}

/**
 * show_config_bootmode()
 *
 */
int show_config_bootmode(void) {

  //last mode enabled for default modes
  #define LAST_MODE 4

  int res = 0;
  const char* headers[3] = {
          " # Boot --> Set Default -->",
          "",
          NULL
  };
  char** title_headers = prepend_title(headers);

  static char options[MODES_COUNT][64];
  struct UiMenuItem menu_opts[MODES_COUNT];
  int i, mode;
  struct UiMenuResult ret;

  for (;;) {

    mode = get_default_bootmode();

    for(i = 0; i < LAST_MODE; ++i) {
      sprintf(options[i], " [%s]", str_mode(i));
      if(mode == i)
        options[i][0] = '*';
      menu_opts[i] = buildMenuItem(MENUITEM_SMALL, options[i], NULL);
    }

    menu_opts[LAST_MODE] = buildMenuItem(MENUITEM_SMALL, "<--Go Back", NULL);
    menu_opts[LAST_MODE+1] = buildMenuItem(MENUITEM_NULL, NULL, NULL);

    ret = get_menu_selection(title_headers, TABS, menu_opts, 1, mode);
    if (ret.result >= LAST_MODE || strlen(menu_opts[ret.result].title) == 0) {
      //back
      res=1;
      break;
    }
    if (ret.result == BOOT_2NDSYSTEM) {
      if (!file_exists((char*) FILE_2NDSYSTEM)) {
        LOGE("Script not found :\n%s\n", FILE_2NDSYSTEM);
        continue;
      }
    }
    if (ret.result == BOOT_2NDBOOT_UART) {
      if (!file_exists((char*) FILE_2NDBOOT_UART)) {
        LOGE("Script not found :\n%s\n", FILE_2NDBOOT_UART);
        continue;
      }
    }
    if (set_default_bootmode(ret.result) == 0) {
      ui_print("Done..");
      continue;
    }
    else {
      ui_print("Failed to setup default boot mode.");
      break;
    }
  }

  free_menu_headers(title_headers);
  return res;
}


/**
 * show_menu_fs_tools()
 *
 * ADB shell and usb shares
 */
int show_menu_fs_tools(void) {

#define TOOL_FORMAT_EXT4 1
#define TOOL_FORMAT_EXT3 2

  int status;

  const char* headers[] = {
        "",
        " # File System Tools -->",
        "",
        NULL
  };
  char** title_headers = prepend_title(headers);

  struct UiMenuItem items[] = {
    {MENUITEM_SMALL, "!!!This will wipe your data!!!", NULL},
    {MENUITEM_SMALL, "Format DATA and CACHE to ext4", NULL},
    {MENUITEM_SMALL, "Format DATA and CACHE to ext3", NULL},
    {MENUITEM_SMALL, "!!!This will wipe your data!!!", NULL},
    {MENUITEM_SMALL, "<--Go Back", NULL},
    {MENUITEM_NULL, NULL, NULL},
  };

  struct UiMenuResult ret = get_menu_selection(title_headers, TABS, items, 1, 0);

  switch (ret.result) {
     case TOOL_FORMAT_EXT4:
      ui_print("Format DATA and CACHE in ext4....");
      status = exec_script(FILE_FORMAT_EXT4, ENABLE);
      ui_print("Done..\n");
      break;

    case TOOL_FORMAT_EXT3:
      ui_print("Format DATA and CACHE in ext3....");
      status = exec_script(FILE_FORMAT_EXT3, ENABLE);
      ui_print("Done..\n");
      break;

    default:
      break;
  }

  free_menu_headers(title_headers);
  return 0;
}

/**
 * show_menu_tools()
 *
 * ADB shell and usb shares
 */
int show_menu_usb_mount_tools(void) {

#define TOOL_USB     0
#define TOOL_CDROM   1
#define TOOL_SYSTEM  2
#define TOOL_DATA    3
#define TOOL_NATIVE  4
#define TOOL_UMOUNT  6

#ifndef BOARD_MMC_DEVICE
#define BOARD_MMC_DEVICE "/dev/block/mmcblk1"
#endif

  int status;

  const char* headers[] = {
        "",
        " # File System Tools -->",
        "",
        NULL
  };
  char** title_headers = prepend_title(headers);

  struct UiMenuItem items[] = {
    {MENUITEM_SMALL, "Share SD Card", NULL},
    {MENUITEM_SMALL, "Share Drivers", NULL},
    {MENUITEM_SMALL, "Share system", NULL},
    {MENUITEM_SMALL, "Share data", NULL},
    {MENUITEM_SMALL, "Share MMC - Dangerous!", NULL},
    {MENUITEM_SMALL, "", NULL},
    {MENUITEM_SMALL, "Stop USB Share", NULL},
    {MENUITEM_SMALL, "<--Go Back", NULL},
    {MENUITEM_NULL, NULL, NULL},
  };

  struct UiMenuResult ret = get_menu_selection(title_headers, TABS, items, 1, 0);

  switch (ret.result) {

    case TOOL_UMOUNT:
      ui_print("Stopping USB share...");
      sync();
      mount_usb_storage("");
      status = set_usb_device_mode("acm");
      ui_print("Done..\n");
      break;

    case TOOL_USB:
      ui_print("USB Mass Storage....");
      status = exec_script(FILE_SDCARD, ENABLE);
      ui_print("Done..\n");
      break;

    case TOOL_CDROM:
      ui_print("USB Drivers....");
      status = exec_script(FILE_CDROM, ENABLE);
      ui_print("Done..\n");
      break;

    case TOOL_SYSTEM:
      ui_print("Sharing System Partition....");
      status = exec_script(FILE_SYSTEM, ENABLE);
      ui_print("Done..\n");
      break;

    case TOOL_DATA:
      ui_print("Sharing Data Partition....");
      status = exec_script(FILE_DATA, ENABLE);
      ui_print("Done..\n");
      break;

    case TOOL_NATIVE:
      ui_print("Set USB device mode...");
      sync();
      mount_usb_storage(BOARD_MMC_DEVICE);
      usleep(500*1000);
      status = set_usb_device_mode("msc_adb");
      usleep(500*1000);
      mount_usb_storage(BOARD_MMC_DEVICE);
      ui_print("Done..\n");
      break;

    default:
      break;
  }

  free_menu_headers(title_headers);
  return 0;
}


/**
 * show_menu_tools()
 *
 * ADB shell and usb shares
 */
int show_menu_tools(void) {

#define TOOL_ADB     0
#define USB_TOOLS    1
#define FS_TOOLS     2

  int status;

  const char* headers[] = {
        "",
        " # USB Tools -->",
        "",
        NULL
  };
  char** title_headers = prepend_title(headers);

  struct UiMenuItem items[] = {
    {MENUITEM_SMALL, "ADB Daemon", NULL},
    {MENUITEM_SMALL, "USB Mount tools", NULL},
    {MENUITEM_SMALL, "File System Tools", NULL},
    {MENUITEM_SMALL, "", NULL},
    {MENUITEM_SMALL, "<--Go Back", NULL},
    {MENUITEM_NULL, NULL, NULL},
  };

  struct UiMenuResult ret = get_menu_selection(title_headers, TABS, items, 1, 0);

  switch (ret.result) {
    case TOOL_ADB:
      ui_print("ADB Deamon....");
      status = exec_script(FILE_ADBD, ENABLE);
      ui_print("Done..\n");
      break;

      case USB_TOOLS:
        show_menu_usb_mount_tools();
        break;

      case FS_TOOLS:
        show_menu_fs_tools();
        break;

    default:
      break;
  }

  free_menu_headers(title_headers);
  return 0;
}

/**
 * show_menu_recovery()
 *
 */
int show_menu_recovery(void) {
  #define RECOVERY_CUSTOM     0
  #define RECOVERY_STOCK      1

  int status, res=0;
  char** args;
  FILE* f;

  const char* headers[] = {
        " # Recovery -->",
        "",
        NULL
  };
  char** title_headers = prepend_title(headers);

  struct UiMenuItem items[] = {
    {MENUITEM_SMALL, "Team Win Recovery", NULL},
    {MENUITEM_SMALL, "Motorola Stock Recovery", NULL},
    {MENUITEM_SMALL, "<--Go Back", NULL},
    {MENUITEM_NULL, NULL, NULL},
  };

  struct UiMenuResult ret = get_menu_selection(title_headers, TABS, items, 1, 0);

  switch (ret.result) {
    case RECOVERY_CUSTOM:
      ui_print("Starting Recovery..\n");
      ui_print("This can take a couple of seconds.\n");
      ui_show_text(DISABLE);
      ui_stop_redraw();
      status = exec_script(FILE_RECOVERY, ENABLE);
      ui_resume_redraw();
      ui_show_text(ENABLE);
      if (!status) res = 1;
      break;

    case RECOVERY_STOCK:
      ui_print("Rebooting to Stock Recovery..\n");
      sync();
      reboot(LINUX_REBOOT_CMD_RESTART2);

    default:
      break;
  }

  free_menu_headers(title_headers);
  return res;
}

// --------------------------------------------------------

/**
 * get_default_bootmode()
 *
 */
int get_default_bootmode() {
  char mode[32];
  int m;
  FILE* f = fopen(FILE_DEFAULTBOOTMODE, "r");
  if (f != NULL) {
      fscanf(f, "%s", mode);
      fclose(f);

      m = int_mode(mode);
      LOGI("default_bootmode=%d\n", m);

      if (m >=0) return m;
      else return int_mode("bootmenu");

  }
  return -1;
}

/**
 * get_bootmode()
 *
 */
int get_bootmode(int clean,int log) {
  char mode[32];
  int m;
  FILE* f = fopen(FILE_BOOTMODE, "r");
  if (f != NULL) {

      // One-shot bootmode, bootmode.conf is deleted after
      fscanf(f, "%s", mode);
      fclose(f);

      if (clean) {
          exec_script(FILE_BOOTMODE_CLEAN,DISABLE);
      }

      m = int_mode(mode);
      if (log) LOGI("bootmode=%d\n", m);
      if (m >= 0) return m;
  }

  return get_default_bootmode();
}

/**
 * set_default_bootmode()
 *
 */
int set_default_bootmode(int mode) {

  char log[64];
  char* str = (char*) str_mode(mode);

  if (mode < MODES_COUNT) {

      ui_print("Set %s...\n", str);
      return bootmode_write(str);
  }

  ui_print("ERROR: bad mode %d\n", mode);
  return 1;
}

/**
 * bootmode_write()
 *
 * write default boot mode in config file
 */
int bootmode_write(const char* str) {
  FILE* f = fopen(FILE_DEFAULTBOOTMODE, "w");

  if (f != NULL) {
    fprintf(f, "%s", str);
    fflush(f);
    fclose(f);
    sync();
    //double check
    if (get_bootmode(0,0) == int_mode( (char*)str) ) {
      return 0;
    }
  }

  ui_print("ERROR: unable to write mode %s\n", str);
  return 1;
}

/**
 * next_bootmode_write()
 *
 * write next boot mode in config file
 */
int next_bootmode_write(const char* str) {
  FILE* f = fopen(FILE_BOOTMODE, "w");

  if (f != NULL) {
    fprintf(f, "%s", str);
    fflush(f);
    fclose(f);
    sync();
    ui_print("Next boot mode set to %s\n\nRebooting...\n", str);
    return 0;
  }

  return 1;
}

// --------------------------------------------------------

/**
 * led_alert()
 *
 */
int led_alert(const char* color, int value) {
  char led_path[PATH_MAX];
  sprintf(led_path, "/sys/class/leds/%s/brightness", color);
  FILE* f = fopen(led_path, "w");

  if (f != NULL) {
    fprintf(f, "%d", value);
    fclose(f);
    return 0;
  }
  return 1;
}

/**
 * bypass_sign()
 *
 */
int bypass_sign(const char* mode) {
  FILE* f = fopen(FILE_BYPASS, "w");

  if (f != NULL) {
    fprintf(f, "%s",  mode);
    fclose(f);
    return 0;
  }
  return 1;
}

/**
 * bypass_check()
 *
 */
int bypass_check(void) {
   FILE* f = fopen(FILE_BYPASS, "r");
   char bypass[30];

  if (f != NULL) {
    fscanf(f, "%s", bypass);
    fclose(f);
    if (0 == strcmp(bypass, "yes")) {
      return 0;
    }
  }
  return 1;
}

/**
 * exec_and_wait()
 *
 */
int exec_and_wait(char** argp) {
  pid_t pid;
  sig_t intsave, quitsave;
  sigset_t mask, omask;
  int pstat;

  sigemptyset(&mask);
  sigaddset(&mask, SIGCHLD);
  sigprocmask(SIG_BLOCK, &mask, &omask);
  switch (pid = vfork()) {
  case -1:            /* error */
    sigprocmask(SIG_SETMASK, &omask, NULL);
    return(-1);
  case 0:                /* child */
    sigprocmask(SIG_SETMASK, &omask, NULL);
    execve(argp[0], argp, environ);

    // execve require the full path of binary in argp[0]
    if (errno == 2 && strncmp(argp[0], "/", 1)) {
        char bin[PATH_MAX] = "/system/bin/";
        argp[0] = strcat(bin, argp[0]);
        execve(argp[0], argp, environ);
    }

    fprintf(stdout, "E:Can't run %s (%s)\n", argp[0], strerror(errno));
    _exit(127);
  }

  intsave = (sig_t)  signal(SIGINT, SIG_IGN);
  quitsave = (sig_t) signal(SIGQUIT, SIG_IGN);
  pid = waitpid(pid, (int *)&pstat, 0);
  sigprocmask(SIG_SETMASK, &omask, NULL);
  (void)signal(SIGINT, intsave);
  (void)signal(SIGQUIT, quitsave);
  return (pid == -1 ? -1 : pstat);
}

/**
 * exec_script()
 *
 */
int exec_script(const char* filename, int ui) {
  int status;
  char** args;

  if (!file_exists((char*) filename)) {
    LOGE("Script not found :\n%s\n", filename);
    return -1;
  }

  LOGI("exec %s\n", filename);

  chmod(filename, 0755);

  args = (char **)malloc(sizeof(char*) * 2);
  args[0] = (char *) filename;
  args[1] = NULL;

  status = exec_and_wait(args);

  free(args);

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    if (ui) {
      LOGE("Error in %s\n(Result: %s)\n", filename, strerror(errno));
    }
    else {
      LOGI("E:Error in %s\n(Result: %s)\n", filename, strerror(errno));
    }
    return -1;
  }

  return 0;
}

inline int snd_reboot() {
  sync();
  return reboot(RB_AUTOBOOT);
}

/**
 * real_execute()
 *
 * when bootmenu is substitued to a system binary (like logwrapper)
 * we need also to execute the original binary, renamed logwrapper.bin
 *
 */
int real_execute(int r_argc, char** r_argv) {
  char* hijacked_executable = r_argv[0];
  int result = 0;
  int i;

  char real_executable[PATH_MAX];
  char ** argp = (char **)malloc(sizeof(char *) * (r_argc + 1));

  sprintf(real_executable, "%s.bin", hijacked_executable);

  argp[0] = real_executable;
  for (i = 1; i < r_argc; i++) {
      argp[i] = r_argv[i];
  }
  argp[r_argc] = NULL;

  result = exec_and_wait(argp);

  free(argp);

  if (!WIFEXITED(result) || WEXITSTATUS(result) != 0)
    return -1;
  else
    return 0;
}

/**
 * file_exists()
 *
 */
int file_exists(char * file)
{
  struct stat file_info;
  memset(&file_info,0,sizeof(file_info));
  return (int) (0 == stat(file, &file_info));
}

int log_dumpfile(char * file)
{
  char buffer[MAX_COLS];
  int lines = 0;
  FILE* f = fopen(file, "r");
  if (f == NULL) return 0;

  while (fgets(buffer, MAX_COLS, f) != NULL) {
    ui_print("%s", buffer);
    lines++;

    // limit max read size...
    if (lines > MAX_ROWS*100) break;
  }
  fclose(f);

  return lines;
}

/**
 * usb_connected()
 *
 */
int usb_connected() {
  int state;
  FILE* f;

  //usb should be 1 and ac 0
  f = fopen(SYS_USB_CONNECTED, "r");
  if (f != NULL) {
    fscanf(f, "%d", &state);
    fclose(f);
    if (state) {
      f = fopen(SYS_POWER_CONNECTED, "r");
      if (f != NULL) {
        fscanf(f, "%d", &state);
        fclose(f);
        return (state == 0);
      }
    }
  }
  return 0;
}

int adb_started() {
  int res = 0;

  #ifndef FILE_ADB_STATE
  #define FILE_ADB_STATE "/tmp/usbd_current_state"
  #endif

  FILE* f = fopen(FILE_ADB_STATE, "r");
  if (f != NULL) {
    char mode[32] = "";
    fscanf(f, "%s", mode);
    res = (0 == strcmp("usb_mode_charge_adb", mode));
    fclose(f);
  }

  bool con = usb_connected();
  if (con && res) {
    adbd_ready = true;
  } else {
    // must be restarted, if usb was disconnected
    adbd_ready = false;
    f = fopen(FILE_ADB_STATE, "w");
    if (f != NULL) {
      fprintf(f, "\n");
      fflush(f);
      fclose(f);
    }
  }

  return adbd_ready;
}

/**
 * bettery_level()
 *
 */
int battery_level() {
  int state = 0;
  FILE* f = fopen(SYS_BATTERY_LEVEL, "r");
  if (f != NULL) {
    fscanf(f, "%d", &state);
    fclose(f);
  }
  return state;
}


/**
 * Native USB ADB Mode Switch
 *
 */
int set_usb_device_mode(const char* mode) {

  #ifndef BOARD_USB_MODESWITCH
  #define BOARD_USB_MODESWITCH  "/dev/usb_device_mode"
  #endif

  FILE* f = fopen(BOARD_USB_MODESWITCH, "w");
  if (f != NULL) {

    fprintf(f, "%s", mode);
    fclose(f);

    LOGI("set usb mode=%s\n", mode);
    return 0;

  } else {
    fprintf(stdout, "E:Can't open " BOARD_USB_MODESWITCH " (%s)\n", strerror(errno));
    return errno;
  }
}

int mount_usb_storage(const char* part) {

  #ifndef BOARD_UMS_LUNFILE
  #define BOARD_UMS_LUNFILE  "/sys/devices/platform/usb_mass_storage/lun0/file"
  #endif

  FILE* f = fopen(BOARD_UMS_LUNFILE, "w");
  if (f != NULL) {

    fprintf(f, "%s", part);
    fclose(f);
    return 0;

  } else {
    ui_print("E:Unable to write to lun file (%s)", strerror(errno));
    return errno;
  }
}

