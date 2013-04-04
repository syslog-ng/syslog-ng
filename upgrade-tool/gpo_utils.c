#include <stdio.h>
#include <stdlib.h>
#include <shlwapi.h>
#include "config_store.h"
#include "upgrade_config.h"
#include "gpo_utils.h"
#include "messages.h"
#include <windows.h>


#define GPO_TMP "SOFTWARE\\BalaBit\\syslog-ng Agent\\Domain GPO Information TMP\\"
#define MIGRATE_TEST_KEY "SOFTWARE\\BalaBit\\syslog-ng Agent\\Migrate Test\\"
#define MIGRATE_TEST_BACKUP "SOFTWARE\\BalaBit\\syslog-ng Agent\\Migrate Test backup\\"

#define SYSLOG_NG_AGENT_REGISTRY_ROOT "SOFTWARE\\BalaBit\\syslog-ng Agent\\"
#define SYSLOG_NG_AGENT_REGISTRY_BACKUP "SOFTWARE\\BalaBit\\syslog_ng_Agent_backup\\"

#define SYSLOG_NG_AGENT_GPO_MIGRATION "SOFTWARE\\BalaBit\\Domain_GPO_Migration\\"

#define BIN_FILE_BACKUP "_backup"



GpoStruct *
gpo_struct_new(gchar *display_name, gchar *full_file_name, gchar *registry_root_key)
{
  GpoStruct *self = g_new0(GpoStruct, 1);
  self->display_name = g_strdup(display_name);
  self->full_file_name = g_strdup(full_file_name);
  self->registry_root_key = g_strdup(registry_root_key);
  return self;
}

void
gpo_struct_free(GpoStruct *self)
{
  g_free(self->display_name);
  g_free(self->full_file_name);
  g_free(self->registry_root_key);
  g_free(self);
}


gchar *
get_user_domain()
{
  return getenv("USERDNSDOMAIN");
}

typedef LONG (WINAPI*  REGSAVEKEYEX)(HKEY hKey, LPCTSTR lpFile, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD Flags);

REGSAVEKEYEX RegSaveKeyExFunc=NULL;

REGSAVEKEYEX GetRegSaveKeyEx()
{
  if (RegSaveKeyExFunc != NULL)
    {
      return RegSaveKeyExFunc;
    }
  HMODULE hModule = LoadLibrary("Advapi32.dll");
  if (hModule == NULL)
    {
      return NULL;
    }
  RegSaveKeyExFunc = (REGSAVEKEYEX)GetProcAddress(hModule,"RegSaveKeyExA");
  return RegSaveKeyExFunc;
}

/*BOOL
PathFileExists(gchar *path)
{
  return g_file_test(path, G_FILE_TEST_EXISTS);
}*/

BOOL
set_privilege(HANDLE hToken, LPCSTR lpszPrivilege, BOOL bEnablePrivilege)
{
  TOKEN_PRIVILEGES tp;
  LUID luid;
  if (!LookupPrivilegeValue(NULL, lpszPrivilege, &luid))
    {

      msg_error("LookupPrivilegeValue error", evt_tag_errno_win("error", GetLastError()), NULL);
      return FALSE;
    }

  tp.PrivilegeCount = 1;
  tp.Privileges[0].Luid = luid;
  if (bEnablePrivilege)
    {
      tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    }
  else
    {
      tp.Privileges[0].Attributes = 0;
    }

  // Enable the privilege or disable all privileges.

  if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(TOKEN_PRIVILEGES), (PTOKEN_PRIVILEGES) NULL, (PDWORD) NULL))
    {
      msg_error("AdjustTokenPrivileges failed", evt_tag_errno_win("error", GetLastError()), NULL);
      return FALSE;
    }

  if (GetLastError() == ERROR_NOT_ALL_ASSIGNED)
    {
      msg_error("The token does not have the specified privilege", NULL);
      return FALSE;
    }

  return TRUE;
}

BOOL
export_to_bin_file(const gchar *src_key, const gchar *path)
{
  HANDLE hToken;
  RegSaveKeyExFunc = GetRegSaveKeyEx();
  if (!RegSaveKeyExFunc)
    {
      msg_error("Can't load find RegSaveKeyEx function. On Windows2000 agent can't export registry settings", NULL);
      return FALSE;
    }
  BOOL backup_privilege = FALSE;
  if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
    {
      backup_privilege = set_privilege(hToken, "SeBackupPrivilege", TRUE);
      CloseHandle(hToken);
    }
  else
    {
      return FALSE;
    }

  if (!backup_privilege)
    {
      return FALSE;
    }

  HKEY key_handle;

  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, src_key, REG_OPTION_BACKUP_RESTORE, KEY_ALL_ACCESS, &key_handle) == 0)
    {
      // delete the file othervise the RegSaveKey will fail
      if (PathFileExists(path))
        {
          DeleteFile(path);
        }

      int error = RegSaveKeyExFunc(key_handle, path, NULL, REG_STANDARD_FORMAT);
      CloseHandle(key_handle);

      if (error == ERROR_SUCCESS)
        {
          return TRUE;
        }
    }
  return FALSE;
}

BOOL
import_to_bin_file(const gchar *src_key, const gchar *path)
{
    BOOL restore_privilege = FALSE;
    BOOL restore_sucessfull = FALSE;
    BOOL file_exist = PathFileExists(path);
    HANDLE hToken;

    if (!file_exist)
      {
        msg_error("File doesn't exist", evt_tag_str("file", path), NULL);
        return FALSE;
      }

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))
       {
         restore_privilege = set_privilege(hToken, "SeRestorePrivilege", TRUE);
         CloseHandle(hToken);
       }
     else
       {
         msg_error("OpenProcessToken() failed", evt_tag_errno_win("error", GetLastError()), NULL);
         return FALSE;
       }

    if (!restore_privilege)
      {
        return FALSE;
      }

    // Import bin file settings to registry.
    HKEY phkResult;
    DWORD dwDisposition;
    LONG error_code = RegCreateKeyEx(HKEY_LOCAL_MACHINE, src_key,
                                     0, NULL, REG_OPTION_NON_VOLATILE,
                                     KEY_ALL_ACCESS,NULL, &phkResult,
                                     &dwDisposition);
    if (error_code != ERROR_SUCCESS)
      {
        msg_error("Failed to open registry key", evt_tag_str("key", src_key), evt_tag_errno_win("error", GetLastError()), NULL);
        return FALSE;
      }

    error_code = RegRestoreKey(phkResult, path, REG_FORCE_RESTORE);
    if (error_code == ERROR_SUCCESS)
      {
        restore_sucessfull = TRUE;
      }
    else
      {
        msg_error("Failed to restore registry key", evt_tag_str("key", src_key), evt_tag_errno_win("error", GetLastError()), NULL);
      }
    RegCloseKey(phkResult);
    return restore_sucessfull;
}



GHashTable *
GPODisover(const gboolean write_gpo_tmp_regkeys)
{
  gchar *user_domain = get_user_domain();
  GHashTable *result = NULL;
  if (user_domain == NULL)
    {
      return NULL;
    }

  if (write_gpo_tmp_regkeys)
    {
      ConfigStore *reg = config_store_new(STORE_TYPE_REGISTRY);

      if (config_store_open(reg, GPO_TMP))
        {
          config_store_delete_key(reg);
        }
      config_store_create_key(reg, GPO_TMP);
      config_store_set_string(reg, "UserDnsDomain", user_domain);
      config_store_close(reg);
      config_store_free(reg);
    }

  WIN32_FIND_DATA FindFileData;
  HANDLE hFind = INVALID_HANDLE_VALUE;
  GString *policy_dir = g_string_new("\\\\");

  g_string_append_printf(policy_dir, "%s\\SYSVOL\\%s\\Policies\\", user_domain, user_domain);

  GString *filter = g_string_new(policy_dir->str);
  g_string_append(filter, "*");

  // iterate over the gpo directoris
  hFind = FindFirstFileEx(filter->str, FindExInfoStandard, &FindFileData, FindExSearchNameMatch, NULL, 0 );
  if (hFind == INVALID_HANDLE_VALUE)
    {
       msg_error("Invalid file handle", evt_tag_errno_win("error", GetLastError()), NULL);
       return NULL;
    }
  GString *name_str = g_string_sized_new(4096);
  do
    {
      g_string_truncate(name_str, 0);
      if (FindFileData.cFileName[0] == '{' && (FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
        {
          GString *full_path = g_string_new(policy_dir->str);
          gchar *buffer;
          gsize length;
          g_string_append_printf(full_path, "%s\\GPT.INI",  FindFileData.cFileName);

          // try to open the gtp file
          GString *display_name = g_string_new("Unknown Display Name");

          if (g_file_test(full_path->str, G_FILE_TEST_EXISTS) && g_file_get_contents(full_path->str, &buffer, &length, NULL))
            {
              gchar *d_name = g_strstr_len(buffer, length, "displayName=");
              if (d_name)
                {
                  d_name += 12;
                  gchar *p = strchr(d_name, '\r');
                  if (p)
                    {
                      *p = '\0';
                    }
                  g_string_assign(display_name,d_name);
                }

              gchar *binary_file_name = g_strdup_printf("%s%s\\Machine\\BalaBit\\syslog-ng\\Settings.reg.bin",policy_dir->str, FindFileData.cFileName);
              gchar *root_key_string = g_strdup_printf("SOFTWARE\\BalaBit\\syslog-ng Agent\\Client Group Policy\\%s", FindFileData.cFileName);
              if (write_gpo_tmp_regkeys)
                {
                  if (GetFileAttributes(binary_file_name) != INVALID_FILE_ATTRIBUTES)
                    {
                      ConfigStore *reg = config_store_new(STORE_TYPE_REGISTRY);
                      gchar *key_string = g_strdup_printf("%s%s", GPO_TMP, FindFileData.cFileName);
                      config_store_create_key(reg, key_string);

                      // create registry keys
                      config_store_set_string(reg, "displayName", display_name->str);
                      config_store_set_string(reg, "fullFileName", binary_file_name);
                      config_store_set_string(reg, "registryRootKey", root_key_string);
                      config_store_close(reg);
                      config_store_free(reg);
                    }
                }
              if (!result)
                {
                  result = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, (GDestroyNotify)gpo_struct_free);
                }
              g_hash_table_insert(result, g_strdup(FindFileData.cFileName), gpo_struct_new(display_name->str, binary_file_name, root_key_string));
            }
        }
    }
  while (FindNextFile(hFind, &FindFileData) != 0);

  DWORD dwError = GetLastError();
  FindClose(hFind);
  if (dwError != ERROR_NO_MORE_FILES)
    {
      msg_error("FindNextFile error",evt_tag_errno_win("error", dwError), NULL);
      g_hash_table_destroy(result);
      return NULL;
    }
  return result;
}

static int
upgrade_registry(const gchar *path)
{
  int result = 0;
  ConfigStore *store = config_store_new(STORE_TYPE_REGISTRY);
  UpgradeConfig *up = upgrade_config_new(path, store);
  if (!upgrade_config_do_upgrade(up))
    {
      result = -1;
    }
  upgrade_config_free(up);
  return result;
}

int
upgrade_gpo_config(const gchar *gpoid)
{
  int result = 0;
  GHashTable *gpo_map = GPODisover(FALSE);
  GpoStruct *gpo = NULL;
  gchar *filename = NULL;
  if (gpo_map == NULL)
    {
      msg_error("Discover GPOs is failed", NULL);
      return -6;
    }
  gpo = g_hash_table_lookup(gpo_map, gpoid);
  if (gpo == NULL)
    {
      msg_error("Can't find GPOid", evt_tag_str("gpoid", gpoid), NULL);
      g_hash_table_destroy(gpo_map);
      return -7;
    }
  filename = g_strdup(gpo->full_file_name);
  g_hash_table_destroy(gpo_map);
  result = upgrade_binary_reg_file(filename);
  g_free(filename);
  return result;
}

int
upgrade_binary_reg_file(const gchar *filename)
{
  int result = 0;
  RegSaveKeyExFunc = GetRegSaveKeyEx();
  gchar *backup_file = g_strdup_printf("%s%s",filename, BIN_FILE_BACKUP);
  if (!RegSaveKeyExFunc)
    {
      msg_error("On this version of windows the gpo isn't upgradeable", NULL);
      return -200;
    }
  if (CopyFile(filename,backup_file, FALSE) == 0)
    {
      msg_error("Can't create backup file", evt_tag_str("file", filename), evt_tag_str("backup_file", backup_file), evt_tag_errno_win("error", GetLastError()), NULL);
      g_free(backup_file);
      return -4;
    }
  if (!import_to_bin_file(SYSLOG_NG_AGENT_GPO_MIGRATION, filename))
    {
      msg_error("importing binary file is failed", NULL);
      return -5;
    }
  result = upgrade_registry(SYSLOG_NG_AGENT_GPO_MIGRATION);
  if (result == 0)
    {
      if (export_to_bin_file(SYSLOG_NG_AGENT_GPO_MIGRATION, filename))
        {
          if (!SHDeleteKey(HKEY_LOCAL_MACHINE, SYSLOG_NG_AGENT_GPO_MIGRATION) == ERROR_SUCCESS)
            {
              msg_error("Can't delete subey", evt_tag_str("subkey", SYSLOG_NG_AGENT_GPO_MIGRATION), NULL);
              result = -5;
            }
        }
      else
        {
          msg_error("Can't export to binary file", NULL);
          result = -5;
        }
    }
  g_free(backup_file);
  return result;
}

int
upgrade_registry_path(const gchar *path)
{
  int result = 0;
  gchar *full_path = g_strdup_printf("%s%s\\", SYSLOG_NG_AGENT_REGISTRY_ROOT, path);
  gchar *backup_path = g_strdup_printf("%s%s\\", SYSLOG_NG_AGENT_REGISTRY_BACKUP, path);
  /* Do backup */
  HKEY hKey;
  HKEY hBackupKey;
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, backup_path, 0, KEY_ALL_ACCESS, &hBackupKey) == ERROR_SUCCESS)
    {
      SHDeleteKey(hBackupKey, NULL);
      RegCloseKey(hBackupKey);
    }
  if (RegCreateKeyEx(HKEY_LOCAL_MACHINE, backup_path, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &hBackupKey, NULL) != ERROR_SUCCESS)
    {
      msg_error("Can't create backup key", evt_tag_str("key", backup_path), evt_tag_errno_win("error", GetLastError()), NULL);
      g_free(full_path);
      g_free(backup_path);
      return -1;
    }
  if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, full_path, 0, KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
    {
      msg_error("Can't open key", evt_tag_str("key", full_path), evt_tag_errno_win("error", GetLastError()), NULL);
      g_free(full_path);
      g_free(backup_path);
      RegCloseKey(hBackupKey);
      return -1;
    }
  if (SHCopyKey(hKey, NULL, hBackupKey, 0) != ERROR_SUCCESS)
    {
      msg_error("Can't copy key", evt_tag_str("src_key", full_path), evt_tag_str("dst_key", backup_path), evt_tag_errno_win("error", GetLastError()), NULL);
      g_free(full_path);
      g_free(backup_path);
      RegCloseKey(hBackupKey);
      RegCloseKey(hKey);
      return -1;
    }
  g_free(backup_path);
  RegCloseKey(hBackupKey);
  RegCloseKey(hKey);
  /* End of backup */

  result = upgrade_registry(full_path);
  g_free(full_path);
  return result;
}
