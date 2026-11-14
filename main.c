#define _GNU_SOURCE
#include <assert.h>
#include <crypt.h>
#include <errno.h>
#include <fcntl.h>
#include <newt.h>
#include <shadow.h>
#include <slang.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/select.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/sysinfo.h>
#include <sys/timerfd.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "conf.h"
#include "guess.h"

#if __STDC_VERSION__ < 201710L
#error "C17-compliant compiler is required"
#endif

static_assert(sizeof(off_t) == 8,
              "File offset type must be 64-bit on the platform!");

#define NEWT_COLORSET_BOLD NEWT_COLORSET_CUSTOM(1)
#define NEWT_COLORSET_GREEN NEWT_COLORSET_CUSTOM(2)

const unsigned int MAX_BODY_TEXT_WIDTH = (80 - 3);

extern const bool is_reserved_user(const char *restrict user);

struct InstallConfig {
  char *install_target;
  char *filesystem_type;
  const char *username;
  const char *hostname;
  const char *password;
  const char *timezone;
  char *locale;
};

static char const *get_tar_decompress_option(void) {
  if (__builtin_strstr(TARBALL_PATH, ".tar.gz")) {
    return "-z";
  } else if (__builtin_strstr(TARBALL_PATH, ".tar.xz")) {
    return "-j";
  } else if (__builtin_strstr(TARBALL_PATH, ".tar.zst")) {
    return "--zstd";
  }
  return NULL;
}

static inline unsigned int ssub(unsigned int a, unsigned int b) {
  unsigned int result;
  if (!__builtin_sub_overflow(a, b, &result)) {
    return result;
  }
  return 0;
}

static const char *get_system_type_string(void) {
#if defined(__alpha__)
  return "DEC Alpha (Alpha AXP)";
#elif defined(__ia64__)
  return "Intel Itanium II";
#elif defined(__mips64)
#if __BYTE_ORDER == __LITTLE_ENDIAN
  return "Loongson 2F/3 (MIPS)";
#else __BYTE_ORDER == __BIG_ENDIAN
  return "SGI Fuel/Indigo/Indy/Octane/Onyx/Origin, etc.";
#endif
#elif defined(__powerpc__) && __BYTE_ORDER == __BIG_ENDIAN
  return "Apple Macintosh (32-bit PowerPC), or IBM CHRP/PReP";
#elif defined(__powerpc64__) && __BYTE_ORDER == __BIG_ENDIAN
  return "Apple Macintosh (64-bit PowerPC), or IBM pSeries";
#elif defined(__sparc64__)
  return "Sun UltraSPARC, Phytium FT-1000/1500, or OpenSPARC";
#elif defined(__i386__)
  struct utsname uts = {};
  if (uname(&uts) == 0) {
    if (strncmp("i486", uts.machine, 4) == 0) {
      return "i486 (Intel486, Am486, Cyrix 5x86, etc.)";
    } else if (strncmp("i586", uts.machine, 4) == 0) {
      return "i586 (Intel Pentium/MMX, AMD K6, Transmeta Crusoe, etc.)";
    } else if (strncmp("i686", uts.machine, 4) == 0) {
      return "i686 (Intel Pentium Pro/II/III/4/M, AMD Athlon/XP, etc.)";
    }
  }
  return "80x86 (unknown)";
#else
  return "Unknown or unsupported";
#endif
}

static bool can_install_afterglow(void) {
  struct sysinfo info;
  if (sysinfo(&info) != 0) {
    return false;
  }
  // Require at least 32 MB of RAM
  if (info.totalram < (MIN_RAM / info.mem_unit)) {
    return false;
  }
  return true;
}

static void draw_title(void) {
  newtDrawRootText(1, 1, "Afterglow Setup");
  newtDrawRootText(0, 2, "═════════════════");
}

static void draw_help_line(const char *help_text) {
  newtPopHelpLine();
  newtPushHelpLine(help_text);
}

static inline void draw_normal_page_prologue(struct newtExitStruct *exitStruct,
                                             const newtComponent page,
                                             const bool first_page) {
  draw_title();
  newtFormSetBackground(page, NEWT_COLORSET_BOLD);
  draw_help_line(first_page ? "  ENTER=Continue  F3=Exit"
                            : "  ENTER=Continue  F3=Exit  F4=Back");
  newtFormAddHotKey(page, NEWT_KEY_F3);
  newtFormAddHotKey(page, NEWT_KEY_ENTER);
  if (!first_page) {
    newtFormAddHotKey(page, NEWT_KEY_F4);
    newtFormAddHotKey(page, NEWT_KEY_ESCAPE);
  }
}

static inline void
draw_installing_page_prologue(struct newtExitStruct *exitStruct,
                              const newtComponent page) {
  draw_title();
  newtFormSetBackground(page, NEWT_COLORSET_BOLD);
  draw_help_line(" ");
}

static inline int check_page_exit(struct newtExitStruct *exitStruct) {
  return exitStruct->reason == NEWT_EXIT_HOTKEY ? exitStruct->u.key : -1;
}

static int draw_welcome_page(struct InstallConfig *install_config) {
  struct newtExitStruct exitStruct = {};

  const newtComponent welcomePage = newtForm(NULL, NULL, 0);
  draw_normal_page_prologue(&exitStruct, welcomePage, true);

  const newtComponent welcomeLabel =
      newtLabel(3, 4, "Welcome to Afterglow setup.");
  newtLabelSetColors(welcomeLabel, NEWT_COLORSET_BOLD);
  newtFormAddComponent(welcomePage, welcomeLabel);

  const newtComponent welcomeText = newtTextboxReflowed(
      3, 6,
      "The Setup program for Afterglow operating system version 2026.0 "
      "prepares Afterglow to run on your computer.\n\n"
      "   • To setup Afterglow, press ENTER.\n\n"
      "   • To quit Setup without installing Afterglow, press F3.",
      MAX_BODY_TEXT_WIDTH, 0, 0, 0);
  newtTextboxSetColors(welcomeText, NEWT_COLORSET_ROOTTEXT,
                       NEWT_COLORSET_ROOTTEXT);
  newtFormAddComponent(welcomePage, welcomeText);

  newtFormRun(welcomePage, &exitStruct);
  newtFormDestroy(welcomePage);
  return check_page_exit(&exitStruct);
}

static int draw_instructions_page(struct InstallConfig *install_config) {
  struct newtExitStruct exitStruct = {};
  const newtComponent instPage = newtForm(NULL, NULL, 0);

  draw_normal_page_prologue(&exitStruct, instPage, false);

  const newtComponent instText = newtTextboxReflowed(
      3, 4,
      "Setup will now guide you through the installation process.\n\n"
      "It is now a good time to partition your storage device. If you haven't "
      "done so yet, press F3 to exit setup, and run the `cfdisk' command.\n"
      "When done, run the `setup' command to restart the installation "
      "process.\n\n"
      "Afterglow expects the following partition scheme:\n\n"
      "   • MBR partition scheme for BIOS-based systems, GPT for (U)EFI,\n"
      "     and APM for PowerPC-based Macintosh systems.\n\n"
      "   • A single partition for Afterglow.\n\n"
      "Please refer to the Afterglow User Manual, chapter X, for details on "
      "partitioning your storage device.\n\n"
      "   • If you have already partitioned your storage device, please press\n"
      "     ENTER to continue.",
      MAX_BODY_TEXT_WIDTH, 0, 0, 0);
  newtTextboxSetColors(instText, NEWT_COLORSET_ROOTTEXT,
                       NEWT_COLORSET_ROOTTEXT);
  newtFormAddComponent(instPage, instText);

  newtFormRun(instPage, &exitStruct);
  newtFormDestroy(instPage);
  return check_page_exit(&exitStruct);
}

static int draw_partition_page(struct InstallConfig *install_config) {
  struct newtExitStruct exitStruct = {};
  const newtComponent partPage = newtForm(NULL, NULL, 0);

  draw_normal_page_prologue(&exitStruct, partPage, false);

  const newtComponent partText = newtTextboxReflowed(
      3, 4,
      "Setup will now gather essential information about your system.\n\n"
      "Listed below are devices available for installation. Afterglow may only "
      "be installed on devices formatted with ext4 or XFS journaling "
      "filesystems.\n\n"
      "Use the UP and DOWN ARROW keys to select an item in the list.",
      MAX_BODY_TEXT_WIDTH, 0, 0, 0);
  newtTextboxSetColors(partText, NEWT_COLORSET_ROOTTEXT,
                       NEWT_COLORSET_ROOTTEXT);
  newtFormAddComponent(partPage, partText);

  const newtComponent deviceList = newtListbox(3, 12, 10, NEWT_FLAG_SCROLL);
  newtListboxSetWidth(deviceList, 70);

  unsigned int device_count = 0;
  unsigned int descriptor_capacity = 10;
  char *descriptor_strings = calloc(descriptor_capacity, 64);
  char *device_paths = calloc(descriptor_capacity, 32);
  char parse_buffer[128] = {0};
  int pipes[2] = {0};
  int r = pipe2(pipes, O_CLOEXEC);
  if (r == -1) {
    perror("pipe2");
    exit(EXIT_FAILURE);
  }
  pid_t lsblk_pid = 0;
  posix_spawn_file_actions_t file_actions;
  posix_spawn_file_actions_init(&file_actions);
  posix_spawn_file_actions_adddup2(&file_actions, pipes[1], STDOUT_FILENO);
  posix_spawnp(
      &lsblk_pid, "lsblk", &file_actions, NULL,
      (char *[]){"lsblk", "-nro", "TYPE,NAME,FSTYPE,SIZE,FSAVAIL", NULL}, NULL);
  close(pipes[1]);
  posix_spawn_file_actions_destroy(&file_actions);
  // read from the pipe
  close(pipes[1]);
  FILE *pipe_stream = fdopen(pipes[0], "r");
  if (!pipe_stream) {
    perror("fdopen");
    exit(EXIT_FAILURE);
  }
  while (fgets(parse_buffer, sizeof(parse_buffer), pipe_stream)) {
    // filter out all entries that are not partitions
    if (memcmp(parse_buffer, "part ", 5) != 0) {
      continue;
    }
    ++device_count;
    if (device_count > descriptor_capacity) {
      descriptor_capacity *= 2;
      descriptor_strings =
          reallocarray(descriptor_strings, descriptor_capacity, 64);
      device_paths = reallocarray(device_paths, descriptor_capacity, 32);
      if (!descriptor_strings || !device_paths) {
        // unrecoverable error, unable to allocate memory
        perror("reallocarray");
        exit(EXIT_FAILURE);
      }
    }
    char *dev_path_start = parse_buffer + 5;
    char *dev_path_end = memchr(dev_path_start, ' ', 128);
    if (!dev_path_end) {
      continue;
    }
    const unsigned long dev_path_len = dev_path_end - dev_path_start;
    const unsigned int leading_space_count = ssub(10, dev_path_len);
    char *block_start = descriptor_strings + (device_count - 1) * 64;
    block_start[63] = '\0';
    memset(block_start, ' ', 62);
    memcpy(block_start + leading_space_count, dev_path_start, dev_path_len);
    memcpy(device_paths + (device_count - 1) * 32, dev_path_start,
           dev_path_len);
    device_paths[(device_count - 1) * 32 + dev_path_len] = '\0';
    char *fs_type_start = dev_path_end + 1;
    char *fs_type_end = memchr(fs_type_start, ' ', 128);
    if (!fs_type_end) {
      fs_type_end = fs_type_start;
    }
    const unsigned long fs_type_len = fs_type_end - fs_type_start;
    if (fs_type_len > 0) {
      block_start[10] = ' ';
      block_start[11] = '[';
      memcpy(block_start + 12, fs_type_start, fs_type_len);
      block_start[12 + fs_type_len] = ']';
    }
    char *size_string_start = fs_type_end + 1;
    char *size_string_end = memchr(size_string_start, ' ', 128);
    if (!size_string_end) {
      continue;
    }
    const unsigned long size_string_len = size_string_end - size_string_start;
    if (size_string_len < 1) {
      continue;
    }
    const unsigned int size_string_space_count = ssub(7, size_string_len);
    memcpy(block_start + 40 + size_string_space_count, size_string_start,
           size_string_len);
    const unsigned int formatting_size_string_end =
        40 + size_string_space_count + size_string_len;
    block_start[formatting_size_string_end] = 'B';

    char *free_size_start = size_string_end + 1;
    char *free_size_end = memchr(free_size_start, 0, 8);
    if (!free_size_end) {
      continue;
    }
    const unsigned long free_size_len = free_size_end - free_size_start;
    if (free_size_len > 1) {
      const unsigned int free_size_space_count = ssub(7, free_size_len);
      block_start[formatting_size_string_end + 1] = ' ';
      block_start[formatting_size_string_end + 2] = '(';
      memcpy(block_start + formatting_size_string_end + 3 +
                 free_size_space_count,
             free_size_start, free_size_len);
      memcpy(&block_start[56], "B free)", 7);
    }
  }
  fclose(pipe_stream);

  for (int i = 0; i < device_count; i++) {
    // 62 characters per line (including whitespaces)
    // device path = 10 characters; 1 space; fs type = 10 characters; size = 7
    // characters; 1 space; free space = 14 characters total
    newtListboxAppendEntry(deviceList, descriptor_strings + i * 64,
                           device_paths + i * 32);
  }
  newtFormAddComponent(partPage, deviceList);

  newtFormRun(partPage, &exitStruct);

  const char *current_device = newtListboxGetCurrent(deviceList);
  const unsigned long device_path_len = strnlen(current_device, 32);
  install_config->install_target = calloc(1, device_path_len + 1);
  memcpy(install_config->install_target, current_device, device_path_len);
  install_config->install_target[device_path_len] = '\0';

  newtFormDestroy(partPage);

  if (descriptor_strings) {
    free(descriptor_strings);
  }
  if (device_paths) {
    free(device_paths);
  }

  return check_page_exit(&exitStruct);
}

static bool validate_hostname(const char *hostname, char **error_message) {
  if (!hostname || !*hostname) {
    *error_message = "You have not yet specified a hostname.";
    return false;
  }
  const char first_char = hostname[0];
  if (!((first_char >= 'a' && first_char <= 'z') ||
        (first_char >= '0' && first_char <= '9'))) {
    *error_message = "The specified hostname is invalid - starts "
                     "with a non-alphanumeric character, which is "
                     "not allowed.";
    return false;
  }
  const unsigned int hostname_len = strnlen(hostname, 64);
  for (const char *p = hostname; *p; p++) {
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' ||
          *p == '.')) {
      *error_message = "The specified hostname is invalid - contains "
                       "invalid character(s). Only lower-cased "
                       "alphanumeric characters, non-consecutive "
                       "dots (.), and dashes (-) are allowed.";
      return false;
    }
  }

  if (memmem(hostname, hostname_len, "..", 2)) {
    *error_message = "The specified hostname is invalid - contains "
                     "two or more consecutive dots, which is not "
                     "allowed.";
    return false;
  }

  const char last_char = hostname[hostname_len - 1];
  if (last_char == '.' || last_char == '-') {
    *error_message = "The specified hostname is invalid - ends "
                     "with a dash (-) or a dot (.), which is not "
                     "allowed.";
    return false;
  }
  return true;
}

static bool validate_username(const char *username, char **error_message) {
  if (!username || !*username) {
    *error_message = "You have not yet specified a UNIX username.";
    return false;
  }
  for (const char *p = username; *p; p++) {
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9'))) {
      *error_message = "The specified UNIX username is invalid - "
                       "only lower-cased alphanumeric characters are "
                       "allowed.";
      return false;
    }
  }

  const char first_char = username[0];
  if (first_char < 'a' || first_char > 'z') {
    *error_message = "The specified UNIX username is invalid - "
                     "must starts with a lower-cased letter.";
    return false;
  }

  if (is_reserved_user(username)) {
    *error_message = "The specified UNIX username is invalid - it is a system "
                     "reserved username.";
    return false;
  }
  return true;
}

static int draw_user_config_page(struct InstallConfig *install_config) {
  struct newtExitStruct exitStruct = {.u.key = NEWT_KEY_ENTER};
  bool validated = false;
  const newtComponent configPage = newtForm(NULL, NULL, 0);

  draw_normal_page_prologue(&exitStruct, configPage, false);

  const newtComponent instText = newtTextboxReflowed(
      3, 4,
      "Setup will now gather essential information about your system.\n\n"
      "Each Afterglow system must have a basic set of credentials - a UNIX "
      "username, password, and system hostname. \n",
      MAX_BODY_TEXT_WIDTH, 0, 0, 0);
  newtTextboxSetColors(instText, NEWT_COLORSET_ROOTTEXT,
                       NEWT_COLORSET_ROOTTEXT);
  newtFormAddComponent(configPage, instText);

  const newtGrid configGrid = newtCreateGrid(2, 4);
  const newtComponent loginText = newtLabel(0, 0, "Your login username:");
  newtGridSetField(configGrid, 0, 0, NEWT_GRID_COMPONENT, loginText, 0, 0, 5, 0,
                   NEWT_ANCHOR_LEFT, 0);
  const newtComponent loginInput = newtEntry(0, 0, install_config->username, 32,
                                             &install_config->username, 0);
  newtGridSetField(configGrid, 1, 0, NEWT_GRID_COMPONENT, loginInput, 0, 0, 0,
                   0, NEWT_ANCHOR_LEFT, 0);
  const newtComponent pwdText = newtLabel(0, 0, "Your password:");
  newtGridSetField(configGrid, 0, 1, NEWT_GRID_COMPONENT, pwdText, 0, 0, 0, 0,
                   NEWT_ANCHOR_LEFT, 0);
  const newtComponent pwdInput =
      newtEntry(0, 0, install_config->password, 32, &install_config->password,
                NEWT_FLAG_PASSWORD);
  newtGridSetField(configGrid, 1, 1, NEWT_GRID_COMPONENT, pwdInput, 0, 0, 0, 0,
                   NEWT_ANCHOR_LEFT, 0);

  const newtComponent hostnameText =
      newtLabel(0, 0, "Hostname for your system:");
  newtGridSetField(configGrid, 0, 2, NEWT_GRID_COMPONENT, hostnameText, 0, 0, 5,
                   0, NEWT_ANCHOR_LEFT, 0);
  const newtComponent hostnameInput = newtEntry(
      0, 0, install_config->hostname, 32, &install_config->hostname, 0);
  newtGridSetField(configGrid, 1, 2, NEWT_GRID_COMPONENT, hostnameInput, 0, 0,
                   0, 0, NEWT_ANCHOR_LEFT, 0);

  newtGridPlace(configGrid, 3, 10);
  newtGridAddComponentsToForm(configGrid, configPage, true);

  while (!validated && exitStruct.u.key == NEWT_KEY_ENTER) {
    newtFormRun(configPage, &exitStruct);
    const int result = check_page_exit(&exitStruct);
    if (result != NEWT_KEY_ENTER)
      break;
    // validate the input
    char error_buffer[2048] = {};
    char *error_buffer_cursor = error_buffer;
    char *error_message = NULL;
    validated = true;
    validated &= validate_username(install_config->username, &error_message);
    if (error_message) {
      error_buffer_cursor +=
          snprintf(error_buffer_cursor,
                   sizeof(error_buffer) - (error_buffer_cursor - error_buffer),
                   "• %s\n", error_message);
    }
    // password check
    error_message = NULL;
    {
      if (!install_config->password || !*install_config->password) {
        error_message = "You have not yet specified a password.";
        validated = false;
      }
      if (error_message) {
        error_buffer_cursor += snprintf(
            error_buffer_cursor,
            sizeof(error_buffer) - (error_buffer_cursor - error_buffer),
            "• %s\n", error_message);
      }
    }
    error_message = NULL;
    validated &= validate_hostname(install_config->hostname, &error_message);
    if (error_message) {
      error_buffer_cursor +=
          snprintf(error_buffer_cursor,
                   sizeof(error_buffer) - (error_buffer_cursor - error_buffer),
                   "• %s\n", error_message);
    }
    if (!validated) {
      newtWinMessage("Validation Error", "OK", error_buffer, NULL, NULL);
    }
  }

  install_config->username = strdup(install_config->username);
  install_config->password = strdup(install_config->password);
  install_config->hostname = strdup(install_config->hostname);
  newtFormDestroy(configPage);
  if (configGrid)
    newtGridFree(configGrid, false);
  return check_page_exit(&exitStruct);
}

static inline int isupperalpha(char c) { return (c >= 'A' && c <= 'Z'); }

static int draw_timezone_page(struct InstallConfig *install_config) {
  struct newtExitStruct exitStruct = {.u.key = NEWT_KEY_ENTER};
  bool validated = false;
  const newtComponent configPage = newtForm(NULL, NULL, 0);

  draw_normal_page_prologue(&exitStruct, configPage, false);

  const newtComponent instText = newtTextboxReflowed(
      3, 4,
      "Setup will now gather essential information about your system.\n\n"
      "Now, Setup will now ask you for your current timezone. The timezone "
      "is used to setup your system's clock and time information, which is "
      "essential for secure networking.\n\n"
      "Timezones are set in \"Area/Location\" pairs, Setup will validate "
      "your input and, if incorrect, please consult \"Appendix A\" of the "
      "Afterglow Handbook for a list of available timezones.\n\n",
      MAX_BODY_TEXT_WIDTH, 0, 0, 0);
  newtTextboxSetColors(instText, NEWT_COLORSET_ROOTTEXT,
                       NEWT_COLORSET_ROOTTEXT);
  newtFormAddComponent(configPage, instText);

  const newtGrid configGrid = newtCreateGrid(2, 1);
  const newtComponent loginText = newtLabel(0, 0, "Your current timezone:");
  newtGridSetField(configGrid, 0, 0, NEWT_GRID_COMPONENT, loginText, 0, 0, 5, 0,
                   NEWT_ANCHOR_LEFT, 0);
  const newtComponent loginInput = newtEntry(0, 0, install_config->timezone, 32,
                                             &install_config->timezone, 0);
  newtGridSetField(configGrid, 1, 0, NEWT_GRID_COMPONENT, loginInput, 0, 0, 0,
                   0, NEWT_ANCHOR_LEFT, 0);

  newtGridPlace(configGrid, 3, 15);
  newtGridAddComponentsToForm(configGrid, configPage, true);

  while (!validated && exitStruct.u.key == NEWT_KEY_ENTER) {
    newtFormRun(configPage, &exitStruct);
    if (exitStruct.reason != NEWT_EXIT_HOTKEY ||
        exitStruct.u.key != NEWT_KEY_ENTER) {
      break;
    }
    char buffer[1024] = {0};
    if (!install_config->timezone || !*install_config->timezone) {
      newtWinMessage("Validation Error", "OK",
                     "You have not yet specified a timezone.", NULL, NULL);
      continue;
    }
    const char prefix[] = "/usr/share/zoneinfo/";
    const size_t prefix_len = strlen(prefix);
    snprintf(buffer, 1024, "/usr/share/zoneinfo/%s", install_config->timezone);
    buffer[1023] = '\0';
    char *real_path = realpath(buffer, NULL);
    validated = isupperalpha(install_config->timezone[0]) && real_path &&
                strncmp(real_path, prefix, prefix_len) == 0;
    validated = validated && strchr(install_config->timezone, '.') == NULL;
    if (!validated) {
      newtWinMessage("Validation Error", "OK",
                     "The specified timezone is invalid.\n\n"
                     "Please consult \"Appendix A\" of the Afterglow "
                     "Handbook for a list of available timezones.",
                     NULL, NULL);
    }
    if (real_path) {
      free(real_path);
    }
  }

  install_config->timezone = strdup(install_config->timezone);
  newtFormDestroy(configPage);
  if (configGrid)
    newtGridFree(configGrid, false);
  return check_page_exit(&exitStruct);
}

static int draw_locale_page(struct InstallConfig *install_config) {
  struct newtExitStruct exitStruct = {};
  bool validated = false;
  const newtComponent configPage = newtForm(NULL, NULL, 0);

  draw_normal_page_prologue(&exitStruct, configPage, false);

  const newtComponent instText = newtTextboxReflowed(
      3, 4,
      "Setup will now gather essential information about your system.\n\n"
      "To finalize configuration, Setup will now ask you for your preferred "
      "system locale. The system locale dictates the display language, "
      "time-and-date "
      "format, and unit system for Afterglow applications and user "
      "environments.\n\n"
      "Please select your preferred system locale:",
      MAX_BODY_TEXT_WIDTH, 0, 0, 0);
  newtTextboxSetColors(instText, NEWT_COLORSET_ROOTTEXT,
                       NEWT_COLORSET_ROOTTEXT);
  newtFormAddComponent(configPage, instText);

  const newtComponent localeList = newtListbox(3, 12, 10, 0);
  newtListboxSetWidth(localeList, 70);
  newtListboxAppendEntry(localeList, " Chinese (Simplified)", "zh_CN");
  newtListboxAppendEntry(localeList, " Chinese (Traditional)", "zh_TW");
  newtListboxAppendEntry(localeList, " English (United States)", "en_US");

  if (install_config->locale) {
    newtListboxSetCurrentByKey(localeList, install_config->locale);
  }

  newtFormAddComponent(configPage, localeList);
  newtFormRun(configPage, &exitStruct);

  install_config->locale = newtListboxGetCurrent(localeList);

  newtFormDestroy(configPage);
  return check_page_exit(&exitStruct);
}

static int draw_confirm_page(struct InstallConfig *install_config) {
  char buffer[1024] = {0};
  struct newtExitStruct exitStruct = {};
  bool validated = false;
  const newtComponent configPage = newtForm(NULL, NULL, 0);

  draw_normal_page_prologue(&exitStruct, configPage, false);
#ifndef NDEBUG
  install_config->install_target = "loop0";
#endif

  snprintf(
      buffer, 1024,
      "Setup is now ready to install Afterglow on your system:\n\n"
      "      System Type: %s\n"
      "      Destination: %s\n"
      "    UNIX username: %s\n"
      "         Hostname: %s\n"
      "         Timezone: %s\n"
      "           Locale: (%s, in UTF-8)\n\n"
      "If all of the information above seem correct, press ENTER to begin "
      "installation. Otherwise, press F3 to restart configuration.\n\n"
      "Please note that Afterglow makes use of compressed RAM block for "
      "virtual memory (swap area), which compresses memory pages to conserve "
      "RAM available for applications. This may have implications on system "
      "performance. For more details on this topic, please consult the "
      "\"System Performance\" chapter in the Afterglow Handbook.",
      get_system_type_string(), install_config->install_target,
      install_config->username, install_config->hostname,
      install_config->timezone, install_config->locale);
  const newtComponent instText =
      newtTextboxReflowed(3, 4, buffer, MAX_BODY_TEXT_WIDTH, 0, 0, 0);
  newtTextboxSetColors(instText, NEWT_COLORSET_ROOTTEXT,
                       NEWT_COLORSET_ROOTTEXT);
  newtFormAddComponent(configPage, instText);
  newtFormRun(configPage, &exitStruct);
  newtFormDestroy(configPage);
  return check_page_exit(&exitStruct);
}

/**
 * Runs a program in a chroot environment.
 *
 * @param new_root The new root directory for the chroot.
 * @param program The program to execute inside the chroot.
 * @param argv The arguments to pass to the program (argv[0] needs to be the
 * program name).
 * @param log_fd The file descriptor for logging the output (stdout and stderr),
 * or -1 to ignore.
 * @param wait Whether to wait for the program to finish.
 * @return The exit status of the program if wait is true, or the PID if wait is
 * false.
 */
static int chroot_run(const char *new_root, const char *program,
                      const char *const argv[], int log_fd, bool wait) {
  pid_t pid = fork();
  if (pid == 0) {
    if (chroot(new_root) != 0) {
      exit(127);
    }
    if (log_fd != -1) {
      dup2(log_fd, STDOUT_FILENO);
      dup2(log_fd, STDERR_FILENO);
    }
    setenv("DEBIAN_FRONTEND", "noninteractive", 1);
    setenv("HOME", "/root", 1);
    chdir("/");
    close(STDIN_FILENO); // disable standard input for the chrooted process
    execvp(program, (char *const *)argv);
    exit(127);
  }
  int status = 0;
  if (!wait) {
    return pid;
  }
  if (waitpid(pid, &status, 0) == -1) {
    return -1;
  }
  return WEXITSTATUS(status);
}

static int obscure_memory_data(void *addr, size_t len) {
  if (addr == NULL || len == 0) {
    return -1;
  }
  // try to zero out the memory securely
  volatile unsigned char *p = (volatile unsigned char *)addr;
  while (len--) {
    *p++ = 0;
  }
  return 0;
}

static int setup_password(int dirfd, struct InstallConfig *install_config) {
  int shadow_fd = openat(dirfd, "etc/shadow", O_RDWR);
  if (shadow_fd == -1) {
    return -1;
  }
  FILE *shadow_file = fdopen(shadow_fd, "r+");
  if (shadow_file == NULL) {
    close(shadow_fd);
    return -1;
  }
  struct spwd *shadow_entry = NULL;
  size_t user_entry_start = 0;
  for (shadow_entry = fgetspent(shadow_file); shadow_entry != NULL;
       shadow_entry = fgetspent(shadow_file)) {
    if (strcmp(shadow_entry->sp_namp, install_config->username) == 0) {
      break;
    }
    user_entry_start = ftell(shadow_file);
  }
  int r = 0;
  if (shadow_entry == NULL) {
    r = -1;
    goto end;
  }

#if CRYPT_GENSALT_IMPLEMENTS_AUTO_ENTROPY &&                                   \
    CRYPT_GENSALT_IMPLEMENTS_DEFAULT_PREFIX
  char *setting_string = crypt_gensalt_ra(NULL, 0, NULL, 0);
#else
#error "afterglow-setup requires a new enough libxcrypt"
#endif
  if (setting_string == NULL) {
    r = -1;
    goto end;
  }
  struct crypt_data crypt_data_instance = {0};
  char *hashed = crypt_rn(install_config->password, setting_string,
                          &crypt_data_instance, sizeof(crypt_data_instance));
  if (hashed == NULL) {
    r = -2;
    goto end;
  }
  const size_t hashed_len = strnlen(hashed, CRYPT_MAX_PASSPHRASE_SIZE);
  if (hashed_len == 0) {
    r = -3;
    goto end;
  }
  // write hashed password to shadow file
  shadow_entry->sp_pwdp = hashed;
  if (fseek(shadow_file, user_entry_start, SEEK_SET) != 0) {
    r = -4;
    goto end;
  }
  if (putspent(shadow_entry, shadow_file) != 0) {
    r = -4;
    goto end;
  }

end:
  // delete sensitive data from memory
  if (setting_string) {
    obscure_memory_data(setting_string,
                        strnlen(setting_string, CRYPT_MAX_PASSPHRASE_SIZE));
    free(setting_string);
  }
  if (crypt_data_instance.initialized)
    obscure_memory_data(&crypt_data_instance, sizeof(crypt_data_instance));
  obscure_memory_data(
      (void *)install_config->password,
      strnlen(install_config->password, CRYPT_MAX_PASSPHRASE_SIZE));

  if (shadow_file) {
    fclose(shadow_file);
  }
  close(shadow_fd);
  return r;
}

static int setup_hostname(int dirfd, struct InstallConfig *install_config) {
  int hostname_fd =
      openat(dirfd, "etc/hostname", O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (hostname_fd == -1) {
    return -1;
  }
  const size_t hostname_len = strnlen(install_config->hostname, 64);
  if (write(hostname_fd, install_config->hostname, hostname_len) !=
      (ssize_t)hostname_len) {
    close(hostname_fd);
    return -1;
  }
  close(hostname_fd);
  return 0;
}

static int setup_locale(int dirfd, struct InstallConfig *install_config) {
  int locale_fd =
      openat(dirfd, "etc/locale.conf", O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (locale_fd == -1) {
    return -1;
  }
  const size_t locale_len = strnlen(install_config->locale, 64);
  if (write(locale_fd, "LANG=", 5) != 5) {
    close(locale_fd);
    return -1;
  }
  if (write(locale_fd, install_config->locale, locale_len) !=
      (ssize_t)locale_len) {
    close(locale_fd);
    return -1;
  }
  close(locale_fd);
  return 0;
}

static pid_t setup_ssh_hostkeys(int dirfd, int logfd, const char *target) {
  const pid_t pid =
      chroot_run(target, "/usr/bin/ssh-keygen",
                 (const char *const[]){"ssh-keygen", "-A", NULL}, logfd, false);
  return pid;
}

static int format_target_device(int logfd, char *device) {
  pid_t pid = 0;
  int status = 0;
  int r = 0;

  // TODO: redirect to a real log file
  logfd = open("/dev/null", O_WRONLY);
  posix_spawn_file_actions_t file_actions;
  posix_spawn_file_actions_init(&file_actions);
  posix_spawn_file_actions_adddup2(&file_actions, logfd, STDOUT_FILENO);
  posix_spawn_file_actions_adddup2(&file_actions, logfd, STDERR_FILENO);

  if (posix_spawnp(&pid, "mkfs.ext4", &file_actions, NULL,
                   (char *const[]){"mkfs.ext4", "-FF", device, NULL},
                   NULL) != 0) {
    r = -1;
  }

  if (waitpid(pid, &status, 0) == -1) {
    r = WEXITSTATUS(status);
  }

end:
  close(logfd);
  posix_spawn_file_actions_destroy(&file_actions);
  return r;
}

#if defined(__i386__)
static int activate_target_device(const char *device) {
  pid_t pid = 0;
  int status = 0;

  if (posix_spawnp(&pid, "sfdisk", NULL, NULL,
                   (char *const[]){"sfdisk", "-A", device, NULL},
                   NULL) != 0) {
    return -1;
  }

  if (waitpid(pid, &status, 0) == -1) {
    return -1;
  }

  return WEXITSTATUS(status);
}
#endif

static int mount_target_device(const char *device, const char *target) {
  return mount(device, target, "ext4", MS_RELATIME | MS_LAZYTIME, NULL);
}

static int mount_bind_target_system(int dirfd, const char *target) {
  int r = 0;
  const char *const mount_dirs[] = {"/dev", "/proc", "/sys"};

  for (size_t i = 0; i < sizeof(mount_dirs) / sizeof(mount_dirs[0]); i++) {
    const int tree_fd =
        open_tree(AT_FDCWD, mount_dirs[i], OPEN_TREE_CLONE | OPEN_TREE_CLOEXEC);
    if (tree_fd == -1) {
      r |= -1;
      continue;
    }
    r |= move_mount(tree_fd, "", dirfd, mount_dirs[i] + 1,
                    MOVE_MOUNT_F_EMPTY_PATH);
    close(tree_fd);
  }

  return r;
}

static int setup_grub_bootloader(int dirfd, int logfd, const char *target) {
  int r = 0;
  // we need a stupid hack to trick the `install-grub` script into thinking the
  // dialog box was "humanly" selected as yes
  const char fake_debconf_path[] = "/tmp/fake_db_script.sh";
  const char fake_debconf_data[] = "db_get() { export RET=true; return 0; }\n"
                                   "db_input() { return 0; }\n "
                                   "db_go() { return 0; }\n";
  int dialog_fd =
      openat(dirfd, fake_debconf_path, O_RDWR | O_CREAT | O_TRUNC, 0644);
  if (dialog_fd < 0) {
    r = -1;
    goto end;
  }
  if (write(dialog_fd, fake_debconf_data, sizeof(fake_debconf_data) - 1) !=
      (ssize_t)(sizeof(fake_debconf_data) - 1)) {
    r = -1;
    goto end;
  }
  close(dialog_fd);
  const int fake_debconf_fd =
      open_tree(dirfd, fake_debconf_path, OPEN_TREE_CLONE | OPEN_TREE_CLOEXEC);
  if (move_mount(fake_debconf_fd, "", dirfd, "usr/share/debconf/confmodule",
                 MOVE_MOUNT_F_EMPTY_PATH) < 0) {
    r = -1;
    goto end;
  }
  r = chroot_run(
      target, "/usr/bin/bash",
      (const char *const[]){"bash", "-ec",
                            "GRUB_SKIP_CONTAINER_CHECK=1 "
                            "/var/lib/dpkg/info/grub.postinst install",
                            NULL},
      logfd, true);
  if (r != 0)
    goto end;
  r = chroot_run(
      target, "/usr/bin/grub-mkconfig",
      (const char *const[]){"grub-mkconfig", "-o", "/boot/grub/grub.cfg", NULL},
      logfd, true);

end:
  close(fake_debconf_fd);
  unlinkat(dirfd, fake_debconf_path, 0);
  return r;
}

static int setup_user(int dirfd, int logfd, const char *target,
                      struct InstallConfig *install_config) {
  int r = chroot_run(target, "/usr/bin/useradd",
                     (const char *const[]){"useradd", "-m", "-s", "/bin/bash",
                                           install_config->username, "-G",
                                           USERGROUPS, NULL},
                     logfd, true);
  return r;
}

static int setup_timezone(int dirfd, struct InstallConfig *install_config) {
  char buffer[1024] = {0};
  snprintf(buffer, sizeof(buffer), "../usr/share/zoneinfo/%s",
           install_config->timezone);
  buffer[1023] = '\0';
  unlinkat(dirfd, "etc/localtime", 0);
  return symlinkat(buffer, dirfd, "etc/localtime");
}

static int umount_target_system(const char *target) {
  char buffer[4096] = {0};
  const int fd = open("/proc/self/mountinfo", O_RDONLY);
  int r = 0;
  if (fd < 0) {
    return -1;
  }
  char search_prefix[1024] = {0};
  const int search_prefix_len =
      snprintf(search_prefix, sizeof(search_prefix), " %s", target);
  if (search_prefix_len <= 0) {
    close(fd);
    return -1;
  }
  size_t cursor = 0;
  while (read(fd, buffer + cursor, sizeof(buffer) - cursor) > 0) {
    char *line_start = buffer + cursor;
    char *line_end;
    while ((line_end = memchr(line_start, '\n', sizeof(buffer) - cursor))) {
      char *path_start = memmem(line_start, line_end - line_start,
                                search_prefix, search_prefix_len);
      if (path_start) {
        char *path_end = memchr(path_start + 1, ' ', line_end - line_start);
        if (path_end) {
          path_end[0] = '\0'; // Null-terminate the path for further
          if (umount2(path_start + 1, UMOUNT_NOFOLLOW) < 0) {
            ++r;
          }
        }
      }
      line_start = line_end + 1;
      cursor = line_start - buffer;
    }
    // Move remaining data to the beginning of the buffer
    if (cursor < sizeof(buffer)) {
      memmove(buffer, buffer + cursor, sizeof(buffer) - cursor);
      memset(buffer + sizeof(buffer) - cursor, 0, cursor);
      cursor = sizeof(buffer) - cursor;
    } else {
      cursor = 0;
      memset(buffer, 0, sizeof(buffer));
    }
  }
  close(fd);
  return r;
}

static void draw_chicago_progress_box(int top, int left, int inner_width) {
  SLsmg_draw_box(top, left, 3, inner_width + 2);
}

static int extract_system_tar_with_progress(const char *tar_path,
                                            const char *target,
                                            const int pb_top, const int pb_left,
                                            const int pb_width) {
  int fd = open(tar_path, O_RDONLY);
  int r = 0;
  struct stat st = {0};

  if (fd == -1) {
    return -1;
  }
  if (fstat(fd, &st) == -1) {
    r = -1;
    goto end;
  }
  const off_t file_size = st.st_size;
  const off_t file_size_h = file_size / 100;
  pid_t pid = 0;
  int pipes[2];
  if (pipe2(pipes, O_CLOEXEC) != 0) {
    return -errno;
  }
  posix_spawn_file_actions_t file_actions = {};
  posix_spawn_file_actions_init(&file_actions);
  posix_spawn_file_actions_adddup2(&file_actions, pipes[0], STDIN_FILENO);
  posix_spawnp(&pid, "tar", &file_actions, NULL,
               (char *const[]){"tar", "-C", (char *)target, "--numeric-owner",
                               (char *)get_tar_decompress_option(), "-xf", "-",
                               NULL},
               NULL);
  close(pipes[0]);
  int prev_progress = 0;
  SLsmg_gotorc(pb_top, pb_left);
  SLsmg_set_color(NEWT_COLORSET_FULLSCALE);
  for (off_t offset = 0; offset < file_size;) {
    const ssize_t written = sendfile(pipes[1], fd, &offset, 8192);
    if (written <= 0) {
      return -errno;
    }
    const off_t progress = offset / file_size_h;
    int progress_int = (int)progress;
    const int blocks_to_draw =
        ((progress_int - prev_progress) * pb_width / 100);
    if (blocks_to_draw > 0) {
      for (int i = 0; i < blocks_to_draw; i++) {
        SLsmg_write_char(0x2588);
      }
      SLsmg_refresh();
      prev_progress = progress_int;
    }
  }
  close(pipes[1]);
  int status = 0;
  waitpid(pid, &status, 0);
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    r = -1;
  }
  posix_spawn_file_actions_destroy(&file_actions);

end:
  close(fd);
  return r;
}

static int save_and_reboot() {
  sync();
  reboot(RB_AUTOBOOT);
  return 0;
}

static void draw_fatal_error_page(const char *error_message) {
  newtCls();
  struct newtExitStruct exitStruct = {};
  const newtComponent errorPage = newtForm(NULL, NULL, 0);
  draw_installing_page_prologue(&exitStruct, errorPage);
  const newtComponent errorLabel =
      newtLabel(3, 4, "Setup encountered a fatal error and cannot continue.");
  const newtComponent errorMessage = newtLabel(3, 6, error_message);
  const newtComponent errorHelp = newtLabel(3, 8, "Press F3 to reboot.");
  newtFormAddComponent(errorPage, errorLabel);
  newtFormAddComponent(errorPage, errorMessage);
  newtFormAddComponent(errorPage, errorHelp);
  draw_help_line("  F3=Reboot");
  newtFormAddHotKey(errorPage, NEWT_KEY_F3);
  newtFormRun(errorPage, &exitStruct);
  newtFormDestroy(errorPage);
#ifdef NDEBUG
  save_and_reboot();
#else
  newtCls();
  newtFinished();
  printf("Debug: %s: %s\n", error_message, strerror(errno));
#endif
  exit(1);
}

static int draw_installing_page(struct InstallConfig *install_config) {
  const char step1_text[] = "Unpacking system archive ";
  const char step2_text[] = "Generating SSH host keys ";
  const char step3_text[] = "Installing bootloader    ";
  const char step4_text[] = "Finalizing installation  ";
  const char dot_text[] = "...";
  const char tic_text[] = "√  ";
  const unsigned int steps_text_length = sizeof(step1_text) - 1;

#ifndef NDEBUG
  install_config->install_target = "loop0p1";
#endif

  struct newtExitStruct exitStruct = {};
  const newtComponent installingPage = newtForm(NULL, NULL, 0);
  draw_installing_page_prologue(&exitStruct, installingPage);
  const newtComponent instText = newtLabel(
      3, 4, "Please wait while Setup installs Afterglow to your system.");
  newtFormAddComponent(installingPage, instText);
  const newtComponent s1Text = newtLabel(7, 6, step1_text);
  const newtComponent s1Status = newtLabel(7 + steps_text_length, 6, dot_text);
  newtFormAddComponent(installingPage, s1Text);
  newtFormAddComponent(installingPage, s1Status);

  char target_path[4096] = "/dev/";
  strncat(target_path, install_config->install_target,
          sizeof(target_path) - strlen(target_path) - 1);
  newtFormAddHotKey(installingPage, NEWT_KEY_ENTER);
  newtDrawForm(installingPage);
  draw_chicago_progress_box(7, 7, 60);
  newtRefresh();
  // NOTE: any error inside installing is considered fatal and should trigger
  // the fatal error page.
#if defined(__i386__)
  if (activate_target_device(target_path) != 0)
    draw_fatal_error_page("Failed to mark target device as active.");
#endif
  if (format_target_device(-1, target_path) != 0)
    draw_fatal_error_page("Failed to format target device.");
  if (mount_target_device(target_path, MOUNT_POINT) < 0)
    draw_fatal_error_page("Failed to mount target device.");
  const int target_dir_fd = open(MOUNT_POINT, O_DIRECTORY | O_RDONLY);
  if (target_dir_fd < 0)
    draw_fatal_error_page("Failed to open target directory.");
  if (mkdirat(target_dir_fd, "var", 0755) == -1 && errno != EEXIST) {
    draw_fatal_error_page("Failed to create var directory.");
  }
  if (mkdirat(target_dir_fd, "var/log", 0755) == -1 && errno != EEXIST) {
    draw_fatal_error_page("Failed to create log directory.");
  }
  const int log_fd = openat(target_dir_fd, "var/log/afterglow-setup.log",
                            O_CREAT | O_WRONLY | O_TRUNC, 0644);
  if (log_fd < 0)
    draw_fatal_error_page("Failed to create a log file for Afterglow setup.");
  if (extract_system_tar_with_progress(TARBALL_PATH, MOUNT_POINT, 8, 8, 60) < 0)
    draw_fatal_error_page("Failed to copy system files.");
  newtLabelSetText(s1Status, tic_text);
  newtLabelSetColors(s1Status, NEWT_COLORSET_GREEN);
  SLsmg_fill_region(7, 7, 3, 62, ' ');
  newtRefresh();

  const newtComponent s2Text = newtLabel(7, 7, step2_text);
  const newtComponent s2Status = newtLabel(7 + steps_text_length, 7, dot_text);
  newtFormAddComponent(installingPage, s2Text);
  newtFormAddComponent(installingPage, s2Status);
  newtDrawForm(installingPage);
  newtRefresh();

  if (mount_bind_target_system(target_dir_fd, MOUNT_POINT) < 0)
    draw_fatal_error_page("Failed to mount target system.");
  const unsigned long guessed_ssh_keygen_time = guess_ssh_keygen_time();
  // how long each progress step should take in seconds
  const unsigned long time_interval = guessed_ssh_keygen_time / 60;
  struct itimerspec timer = {.it_interval.tv_sec = time_interval,
                             .it_interval.tv_nsec = 0,
                             .it_value.tv_sec = time_interval,
                             .it_value.tv_nsec = 0};
  const pid_t ssh_pid = setup_ssh_hostkeys(target_dir_fd, log_fd, MOUNT_POINT);
  draw_chicago_progress_box(8, 7, 60);
  SLsmg_gotorc(9, 8);
  SLsmg_set_color(NEWT_COLORSET_FULLSCALE);
  newtRefresh();

  const int timer_fd = timerfd_create(CLOCK_MONOTONIC, 0);
  const int pid_fd = syscall(SYS_pidfd_open, ssh_pid, 0);
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(timer_fd, &fds);
  FD_SET(pid_fd, &fds);
  do {
    if (select(FD_SETSIZE, &fds, NULL, NULL, NULL) < 0)
      draw_fatal_error_page("select() system call failed.");
    if (FD_ISSET(timer_fd, &fds)) {
      SLsmg_write_char(0x2588);
      newtRefresh();
      FD_SET(pid_fd, &fds); // Re-arm the pid_fd for the next select call
    } else if (FD_ISSET(pid_fd, &fds)) {
      // SSH process status changed
      int status = 0;
      waitpid(ssh_pid, &status, 0);
      if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        break;
      } else {
        draw_fatal_error_page("SSH key generation failed.");
        return -1;
      }
    }
    timerfd_settime(timer_fd, 0, &timer, NULL);
  } while (1);

  newtLabelSetText(s2Status, tic_text);
  newtLabelSetColors(s2Status, NEWT_COLORSET_GREEN);
  SLsmg_fill_region(8, 7, 3, 62, ' ');

  const newtComponent s3Text = newtLabel(7, 8, step3_text);
  const newtComponent s3Status = newtLabel(7 + steps_text_length, 8, dot_text);
  newtFormAddComponent(installingPage, s3Text);
  newtFormAddComponent(installingPage, s3Status);
  newtDrawForm(installingPage);
  newtRefresh();
  if (setup_grub_bootloader(target_dir_fd, log_fd, MOUNT_POINT) != 0)
    draw_fatal_error_page("Failed to install GRUB bootloader.");
  newtLabelSetText(s3Status, tic_text);
  newtLabelSetColors(s3Status, NEWT_COLORSET_GREEN);
  SLsmg_fill_region(9, 7, 3, 62, ' ');

  const newtComponent s4Text = newtLabel(7, 9, step4_text);
  const newtComponent s4Status = newtLabel(7 + steps_text_length, 9, dot_text);
  newtFormAddComponent(installingPage, s4Text);
  newtFormAddComponent(installingPage, s4Status);
  newtDrawForm(installingPage);
  newtRefresh();

  setup_user(target_dir_fd, log_fd, MOUNT_POINT, install_config);
  setup_password(target_dir_fd, install_config);
  setup_hostname(target_dir_fd, install_config);
  setup_timezone(target_dir_fd, install_config);
  setup_locale(target_dir_fd, install_config);

  syncfs(log_fd);
  close(log_fd);

  for (int i = 0; i < 5; i++) {
    if (umount_target_system(MOUNT_POINT) == 0)
      break;
  }
  // we do not check if umount was successful because the user will be
  // instructed to reboot anyways

  newtLabelSetText(s4Status, tic_text);
  newtLabelSetColors(s4Status, NEWT_COLORSET_GREEN);
  SLsmg_fill_region(10, 7, 3, 62, ' ');
  newtRefresh();

  newtFormDestroy(installingPage);

  close(target_dir_fd);
  return NEWT_KEY_ENTER;
}

static int draw_success_page(struct InstallConfig *install_config) {
  struct newtExitStruct exitStruct = {};
  const newtComponent successPage = newtForm(NULL, NULL, 0);
  draw_normal_page_prologue(&exitStruct, successPage, true);
  draw_help_line("  Enter=Reboot  F3=Exit");
  const newtComponent successText = newtTextboxReflowed(
      7, 7,
      "Afterglow Setup has complete successfully.\n\nRemove any floppy "
      "disk(s), diskette(s), or optical media (CD/DVD) from the system's "
      "drive(s).\n\nPress ENTER to to restart your system, or F3 to return to "
      "the installation environment for further configuration if needed, "
      "consult \"Configuring an Installed System Externally\" chapter in the "
      "Afterglow Handbook for tips on configuring an installed system from an "
      "external environment.\n\nWhen your system restarts, it should "
      "automatically start Afterglow.",
      50, 0, 0, 0);
  newtFormAddComponent(successPage, successText);
  newtTextboxSetColors(successText, NEWT_COLORSET_ROOTTEXT,
                       NEWT_COLORSET_ROOTTEXT);
  newtFormRun(successPage, &exitStruct);
  newtFormDestroy(successPage);

  if (exitStruct.u.key == NEWT_KEY_ENTER) {
    save_and_reboot();
    return NEWT_KEY_ENTER;
  }
  return NEWT_KEY_F3;
}

typedef int (*DrawPageFunction)(struct InstallConfig *install_config);
static const DrawPageFunction draw_pages[] = {
    draw_welcome_page,     draw_instructions_page, draw_partition_page,
    draw_user_config_page, draw_timezone_page,     draw_locale_page,
    draw_confirm_page,     draw_installing_page,   draw_success_page};

int main() {
  struct InstallConfig install_config = {0};

  newtInit();
  SLutf8_enable(true);
  SLtt_utf8_enable(true);
  SLsmg_utf8_enable(true);
  newtCls();

  // setup color palette
  newtSetColor(NEWT_COLORSET_HELPLINE, "black", "lightgray");
  newtSetColor(NEWT_COLORSET_LISTBOX, "white", "blue");
  newtSetColor(NEWT_COLORSET_ACTSELLISTBOX, "blue", "lightgray");
  newtSetColor(NEWT_COLORSET_BOLD, "white", "blue");
  newtSetColor(NEWT_COLORSET_GREEN, "green", "blue");
  newtSetColor(NEWT_COLORSET_LABEL, "white", "blue");
  newtSetColor(NEWT_COLORSET_FULLSCALE, "yellow", "yellow");
  SLtt_add_color_attribute(NEWT_COLORSET_BOLD, SLTT_BOLD_MASK);

  // check memory
  if (!can_install_afterglow()) {
    draw_fatal_error_page("Insufficient memory to install Afterglow.");
    goto end;
  }

  const unsigned int num_pages = sizeof(draw_pages) / sizeof(draw_pages[0]);
  unsigned int page_no = 0;

  while (page_no < num_pages) {
    newtCls();
    newtCursorOff();

    const int page_result = draw_pages[page_no](&install_config);
    newtClearKeyBuffer();
    switch (page_result) {
    case -1:
    case NEWT_KEY_F3:
      goto end;
    case 0:
      // stay on the same page
      break;
    case NEWT_KEY_F4:
    case NEWT_KEY_ESCAPE:
      page_no = ssub(page_no, 1);
      break;
    case NEWT_KEY_ENTER:
    default:
      ++page_no;
      break;
    }
  }

end:
  newtCls();
  newtFinished();
  return 0;
}
