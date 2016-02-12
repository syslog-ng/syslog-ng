/*
 * Copyright (c) 2012 Balabit
 * Copyright (c) 2012 Balazs Scheidler <bazsi@balabit.hu>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * As an additional exemption you are allowed to compile & link against the
 * OpenSSL libraries as published by the OpenSSL project. See the file
 * COPYING for details.
 *
 */

#include "file-perms.h"
#include "userdb.h"
#include "messages.h"
#include "cfg.h"
#include "gprocess.h"

#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>

void
file_perm_options_set_file_uid(FilePermOptions *self, const gchar *file_uid)
{
  self->file_uid = 0;
  if (!resolve_user(file_uid, &self->file_uid))
    {
      msg_error("Error resolving user",
                 evt_tag_str("user", file_uid),
                 NULL);
    }
}

void
file_perm_options_set_file_gid(FilePermOptions *self, const gchar *file_gid)
{
  self->file_gid = 0;
  if (!resolve_group(file_gid, &self->file_gid))
    {
      msg_error("Error resolving group",
                 evt_tag_str("group", file_gid),
                 NULL);
    }
}

void
file_perm_options_set_file_perm(FilePermOptions *self, gint file_perm)
{
  self->file_perm = file_perm;
}

void
file_perm_options_set_dir_uid(FilePermOptions *self, const gchar *dir_uid)
{
  self->dir_uid = 0;
  if (!resolve_user(dir_uid, &self->dir_uid))
    {
      msg_error("Error resolving user",
                 evt_tag_str("user", dir_uid),
                 NULL);
    }
}

void
file_perm_options_set_dir_gid(FilePermOptions *self, const gchar *dir_gid)
{
  self->dir_gid = 0;
  if (!resolve_group(dir_gid, &self->dir_gid))
    {
      msg_error("Error resolving group",
                 evt_tag_str("group", dir_gid),
                 NULL);
    }
}

void
file_perm_options_set_dir_perm(FilePermOptions *self, gint dir_perm)
{
  self->dir_perm = dir_perm;
}

void
file_perm_options_defaults(FilePermOptions *self)
{
  self->file_uid = self->file_gid = -1;
  self->file_perm = -1;
  self->dir_uid = self->dir_gid = -1;
  self->dir_perm = -1;
}

void
file_perm_options_init(FilePermOptions *self, GlobalConfig *cfg)
{
  if (self->file_uid == -1)
    self->file_uid = cfg->file_uid;
  if (self->file_gid == -1)
    self->file_gid = cfg->file_gid;
  if (self->file_perm == -1)
    self->file_perm = cfg->file_perm;
  if (self->dir_uid == -1)
    self->dir_uid = cfg->dir_uid;
  if (self->dir_gid == -1)
    self->dir_gid = cfg->dir_gid;
  if (self->dir_perm == -1)
    self->dir_perm = cfg->dir_perm;
}

gboolean
file_perm_options_apply_file(const FilePermOptions *self, gchar *name)
{
#ifndef _MSC_VER
  gboolean result = TRUE;

  if (self->file_uid >= 0 && chown(name, (uid_t) self->file_uid, -1) < 0)
    result = FALSE;
  if (self->file_gid >= 0 && chown(name, -1, (gid_t) self->file_gid) < 0)
    result = FALSE;
  if (self->file_perm >= 0 && chmod(name, (mode_t) self->file_perm) < 0)
    result = FALSE;
  return result;
#endif
}

gboolean
file_perm_options_apply_dir(const FilePermOptions *self, gchar *name)
{
#ifndef _MSC_VER
  gboolean result = TRUE;

  if (self->dir_uid >= 0 && chown(name, (uid_t) self->dir_uid, -1) < 0)
    result = FALSE;
  if (self->dir_gid >= 0 && chown(name, -1, (gid_t) self->dir_gid) < 0)
    result = FALSE;
  if (self->dir_perm >= 0 && chmod(name, (mode_t) self->dir_perm) < 0)
    result = FALSE;
  return result;
#endif
}

gboolean
file_perm_options_apply_fd(const FilePermOptions *self, gint fd)
{
#ifndef _MSC_VER
  gboolean result = TRUE;

  if (self->file_uid >= 0 && fchown(fd, (uid_t) self->file_uid, -1) < 0)
    result = FALSE;
  if (self->file_gid >= 0 && fchown(fd, -1, (gid_t) self->file_gid) < 0)
    result = FALSE;
  if (self->file_perm >= 0 && fchmod(fd, (mode_t) self->file_perm) < 0)
    result = FALSE;

  return result;
#endif
}

/**
 *
 * This function receives a complete path (directory + filename) and creates
 * the directory portion if it does not exist. The point is that the caller
 * wants to ensure that the given filename can be opened after this function
 * returns. (at least it won't fail because of missing directories).
 **/
gboolean
file_perm_options_create_containing_directory(const FilePermOptions *self, gchar *name)
{
  gchar *dirname;
  struct stat st;
  gint rc;
  gchar *p;
  cap_t saved_caps;

  /* check that the directory exists */
  dirname = g_path_get_dirname(name);
  rc = stat(dirname, &st);
  g_free(dirname);

  if (rc == 0)
    {
      /* directory already exists */
      return TRUE;
    }
  else if (rc < 0 && errno != ENOENT)
    {
      /* some real error occurred */
      return FALSE;
    }

  /* directory does not exist */
  p = name + 1;

  p = strchr(p, '/');
  while (p)
    {
      *p = 0;
      if (stat(name, &st) == 0)
        {
          if (!S_ISDIR(st.st_mode))
            return FALSE;
        }
      else if (errno == ENOENT)
        {
          if (mkdir(name, self->dir_perm < 0 ? 0700 : (mode_t) self->dir_perm) == -1)
            return FALSE;
          saved_caps = g_process_cap_save();
          g_process_cap_modify(CAP_CHOWN, TRUE);
          g_process_cap_modify(CAP_FOWNER, TRUE);
          file_perm_options_apply_dir(self, name);
          g_process_cap_restore(saved_caps);
        }
      *p = '/';
      p = strchr(p + 1, '/');
    }
  return TRUE;
}
