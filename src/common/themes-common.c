/* X-Chat
 * Copyright (C) 1998 Peter Zelezny.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#include <stdio.h>
#include <stdlib.h>
#include "xchat.h"
#include "cfgfiles.h"
#include "themes-common.h"
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

static int theme_provided_palette = 0;
static int theme_provided_events = 0;



/*
 * load_themeconfig [PUBLIC]
 *
 * Called from themeaware functions (palette and event texts right now)
 * and first looks for themeprovided config, secondly for one in ~/.xchat/
 *
 */
int
load_themeconfig (int themefile)
{
  char path[256];
  int fh;

  switch (themefile) {
  case THEME_CONF_PALETTE:
    snprintf (path, sizeof (path), "%s/theme/palette.conf", get_xdir ());
    fh = open (path, O_RDONLY);
    theme_provided_palette = TRUE;
    if (fh == -1) {
      snprintf (path, sizeof (path), "%s/palette.conf", get_xdir ());
      fh = open (path, O_RDONLY);
      theme_provided_palette = FALSE;
    }
    break;
  case THEME_CONF_EVENTS:
    snprintf (path, sizeof (path), "%s/theme/pevents.conf", get_xdir ());
    fh = open (path, O_RDONLY);
    theme_provided_events = TRUE;
    if (fh == -1) {
      snprintf (path, sizeof (path), "%s/pevents.conf", get_xdir ());
      fh = open (path, O_RDONLY);
      theme_provided_events = FALSE;
    }
    break;
  default:
    fh = -1;
    break;
  }
  return (fh);
}

/*
 * save_themeconfig [PUBLIC]
 *
 * Called from themeaware functions (palette and event texts right now)
 * opens the same file as load_themeconfig but fot writing
 *
 */
int
save_themeconfig (int themefile)
{
  char path[256];
  int fh;

  switch (themefile) {
  case THEME_CONF_PALETTE:
    if (theme_provided_palette)
      snprintf (path, sizeof (path), "%s/theme/palette.conf", get_xdir ());
    else
      snprintf (path, sizeof (path), "%s/palette.conf", get_xdir ());
    break;
  case THEME_CONF_EVENTS:
    if (theme_provided_events)
      snprintf (path, sizeof (path), "%s/theme/pevents.conf", get_xdir ());
    else
      snprintf (path, sizeof (path), "%s/pevents.conf", get_xdir ());
    break;
  default:
    break;
  }
  
  fh = open (path, O_TRUNC | O_WRONLY | O_CREAT, 0600);
  return (fh);
}

/*
 * init_themes [PUBLIC]
 * 
 * Called each time Xchat is loaded and does some theme related checks
 *
 */
void
init_themes (void)
{
  struct stat buf;
  char buffer1[256];
  char buffer2[256];
  int result;

  /* First check if themes dir exists */
  snprintf (buffer1, sizeof(buffer1), "%s/themes", get_xdir());
  
  if (stat (buffer1, &buf))
    /* There is no themes-dir, create it*/
    mkdir (buffer1,  S_IRUSR | S_IWUSR | S_IXUSR); /* RWX permissions for user */
  
  /* Second, check if the link new-theme exists and if so make it current theme */
  snprintf (buffer1, sizeof(buffer1), "%s/new-theme", get_xdir());
  result = lstat (buffer1, &buf);
  
  if (!result && S_ISLNK(buf.st_mode)) { 
    result = readlink (buffer1, buffer2, sizeof(buffer2));
    /* For some reason readlink doesn't add a NULL */
    buffer2[result] = '\0';
    /* buffer2 is where new-theme is pointing */
    
    if (stat (buffer2, &buf)) {
      /* The dir pointed to by new-theme doesnt exist, remove link */
      unlink (buffer1);
      return;
    }    
    
    /* Remove ~/.xchat/theme, rename ~/.xchat/new-theme to ~/.xchat/theme */
    snprintf (buffer2, sizeof(buffer2), "%s/theme", get_xdir());
    unlink (buffer2);
    rename (buffer1, buffer2);
  } 
}

