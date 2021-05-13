/*-
 * Copyright (c) 2010 Simon Tatham
 * Copyright (c) 2010 Ben Harris
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * Password file management for aund.
 * Current format is
 * User:Password:URD:Priv:Opt4
 */

#include "fs_proto.h"
#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <sys/types.h>
#include <sys/file.h>
#include <sys/time.h>

#include <assert.h>
#if HAVE_CRYPT_H
#include <crypt.h>
#endif
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extern.h"
#include "fileserver.h"

char *pwfile = NULL;
char *pwtmp = NULL;
static int pwline;
static FILE *fp, *newfp;

static int
pw_open(int write) {
    assert(pwfile);            /* shouldn't even be called otherwise */

    fp = fopen(pwfile, "r");
    if (!fp) {
        warn("%s: open", pwfile);
        newfp = NULL;
        return -1;
    }

    if (write) {
        int newfd;
        if (!pwtmp) {
            pwtmp = malloc(strlen(pwfile) + 10);
            sprintf(pwtmp, "%s.tmp", pwfile);
        }
        newfd = open(pwtmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (newfd < 0) {
            warn("%s: open", pwtmp);
            fclose(fp);
            fp = NULL;
            return -1;
        }
        newfp = fdopen(newfd, "w");
        if (!newfp) {
            warn("%s: fdopen", pwtmp);
            fclose(fp);
            fp = NULL;
            return -1;
        }
    } else {
        newfp = NULL;
    }

    pwline = 0;
    return 0;
}

static void
pw_close(void) {
    fclose(fp);
    if (newfp)
        fclose(newfp);
    fp = newfp = NULL;
}

static int
pw_close_rename(void) {
    fclose(fp);
    fp = NULL;
    if (newfp) {
        fclose(newfp);
        newfp = NULL;
        if (rename(pwtmp, pwfile) < 0) {
            warn("%s -> %s: rename", pwfile, pwtmp);
            return -1;
        }
    }
    return 0;
}

static int
pw_read_line(char **user, char **pw, char **urd, char **priv, int *opt4)
{
    static char buffer[16384];
    char *password, *directory_name, *r, *s;

    errno = 0;
    if (!fgets(buffer, sizeof(buffer), fp)) {
        if (errno)         /* distinguish clean EOF from error */
            warn("%s", pwfile);
        return -1;
    }
    pwline++;

    buffer[strcspn(buffer, "\r\n")] = '\0';

    if ((password = strchr(buffer, ':')) == NULL ||
            (directory_name = strchr(password+1, ':')) == NULL    ||
            (s = strchr(directory_name+1, ':')) == NULL) {
        warnx("%s:%d: malformatted line\n", pwfile, pwline);
        return -1;
    }

    *password++ = '\0';
    *directory_name++ = '\0';
    *s++ = '\0';

    r = strchr(s, ':');
    if (r) {
        *r++ = '\0';
        *opt4 = atoi(r);
    } else {
        *opt4 = default_opt4;
    }

    *user = buffer;
    *pw = password;
    *urd = directory_name;
    *priv = s;

    return 0;
}

static void
pw_write_line(char *user, char *pw, char *urd, char *priv, int opt4)
{

    fprintf(newfp, "%s:%s:%s:%s:%d\n", user, pw, urd, priv, opt4);
}

static char *
pw_validate(char *user, const char *pw, int *opt4)
{
    char *u, *p, *d, *s;
    char *ret;

    if (pw_open(0) < 0)
        return NULL;

    while (pw_read_line(&u, &p, &d, &s, opt4) == 0) {
        if (!strcasecmp(user, u)) {
            int ok = 0;

            if (*p) {
                char *cp = crypt(pw, p);
                ok = !strcmp(cp, p);
            } else {
                ok = (!pw || !*pw);
            }
            if (!ok)
                ret = NULL;
            else
                ret = strdup(d);
            if (debug) printf("urd is [%s]\n", ret);    
            strcpy(user, u);   /* normalise case */
            pw_close();
            return ret;
        }
    }

    pw_close();
    return NULL;
}

static char *
pw_urd(char const *user)
{
    char *user_name, *user_password, *directory_name;
    char *privilege;
    int opt4;

    if (pw_open(1) < 0)
        return NULL;

    while (pw_read_line(&user_name, &user_password, 
                &directory_name, &privilege, &opt4) == 0) {
        if (!strcasecmp(user, user_name)) {
            pw_close();
            return strdup(directory_name);
        }
    }
    pw_close();
    return NULL;
}

static int
pw_change(const char *user, const char *oldpw, const char *newpw)
{
    char *u, *p, *d, *s;
    int opt4;
    int done = 0;
    char salt[64];
    char *cp;
    struct timeval tv;
    int ok;

    if (pw_open(1) < 0)
        return -1;

    while (pw_read_line(&u, &p, &d, &s, &opt4) == 0) {
        if (!done && !strcasecmp(user, u)) {
            ok = 0;
            ok = !strcmp(s, "L");
            if (ok) {   // User isnt allowed to change passwd
                    pw_close();
                    return -1;
            }
            ok = !strcmp(s, "F");
            if (ok) {
                pw_close();
                return -1;
            }
        }
        ok = 0;
        if (*p) {
            cp = crypt(oldpw, p);
            ok = !strcmp(cp, p);
        } else {
            ok = (!oldpw || !*oldpw);
        }
        if (!ok) {
            pw_close();
            return -1;
        }
        gettimeofday(&tv, NULL);
        sprintf(salt, "$6$%08lx%08lx$",
            tv.tv_sec & 0xFFFFFFFFUL,
            tv.tv_usec & 0xFFFFFFFFUL);
        p = crypt(newpw, salt);
        }
        pw_write_line(u, p, d, s, opt4);

    return pw_close_rename();
}

static int
pw_set_opt4(const char *user, int newopt4)
{
    char *u, *p, *d, *s;
    int opt4;
    int done = 0;
    int ok;

    if (pw_open(1) < 0)
        return -1;

    while (pw_read_line(&u, &p, &d, &s, &opt4) == 0) {
        if (!done && !strcasecmp(user, u)) {
            ok = 0;
            ok = !strcmp(s, "L");
            if (ok) {   // User isnt allowed to change passwd
                    pw_close();
                    return -1;
            }
            ok = !strcmp(s, "F");
            if (ok) {
                pw_close();
                return -1;
            }
            opt4 = newopt4;
        }
        pw_write_line(u, p, d, s, opt4);
    }

    return pw_close_rename();
}

static int
pw_get_priv(const char *user)
{
        char *u, *p, *d, *s;
        int opt4;
        int priv = EC_FS_PRIV_NONE; /* Assume no Priv */

    if (pw_open(1) < 0)
        return -1;

    while (pw_read_line(&u, &p, &d, &s, &opt4) == 0) {
                if (!strcasecmp(user, u)) {
                    pw_close();
                    switch (*s) {
                        case 'S': priv = EC_FS_PRIV_SYST; break;
                        // Limited users cannot change passwords or 
                        // their boot options
                        case 'L': priv = EC_FS_PRIV_LIMIT; break;
                        // Fixed users cannot change their boot options,
                        // passwords, and cannot look at any directories
                        // other than their own root directory
                        case 'F': priv = EC_FS_PRIV_FIXED; break;

                        default : priv = EC_FS_PRIV_NONE;
                    }
                    return priv;
                }
    }

    pw_close();
    return EC_FS_PRIV_NONE;
}

static int
pw_set_priv( struct fs_client *client, const char *user, const char *newpriv)
{
                char *u, *p, *d, *s;
                int opt4;
                int done = 0;

    if (client->priv == EC_FS_PRIV_SYST) {
        if (pw_open(1) < 0)
            return -1;

        while (pw_read_line(&u, &p, &d, &s, &opt4) == 0) {
            if (!done && !strcasecmp(user, u)) {
                strcpy(s, newpriv);
            }
            pw_write_line(u, p, d, s, opt4);
        }

        return pw_close_rename();
    }
    return -1;  // No privilege
}

static int
pw_add_user(char *user)
{
    char *u, *p, *d, *s;
    char directory[30];
    char group[30];
    int opt4;
    int index;
    char *tmp;
    char ch[30] = "./";
    char ct[30] = "./";
    char end[2] = "\0";
    bool has_group = false;
    struct stat *st;

    if (pw_open(1) < 0)
        return -1;

    while (pw_read_line(&u, &p, &d, &s, &opt4) == 0) {
        pw_write_line(u, p, d, s, opt4);
    }
    // Now we need to add the new user

    // Duplicate the username in to the variable directory
    // replace any full stops with a / 
    strcpy(directory, user);
    tmp = strrchr(directory, '.');
    // Replace any group seperator with /
    if (tmp != NULL)
    {
        *tmp = '/';
    }
    // Now build the urd directory path
    strcat(ch, directory);
    strcpy(directory, ch);
    
    pw_write_line(user, "" , directory, "", 0);

    strcat(directory, end);
    // Check if we have a period '.' in the username because
    // if we do then we have group.username and will need to
    // create the group directory as well as the user directory.
    tmp = strchr(user , '.');
    if (tmp != NULL) 
        has_group = true;

    if (has_group == false)
    {
        if (stat(directory, st) == -1) {
        mkdir(directory, 0777);
        }
    }

    if (has_group == true) {
        // Create the group directory
        // this may fail, because the directory already exists
        tmp = strchr(user, '.');
        index = (int)(tmp - user);
        strncpy(group, user, index);
        strcat(ct, group);
        strcpy(group, ct);
        strcat(group, end);

        if (stat(group, st) == -1) {
            mkdir(group, 0777); /* Ignore errors here for the moment */
        }

        // Create the user directory under the group
        if (stat(directory, st) == -1) {
            mkdir(directory, 0777);
        }
    }
    return pw_close_rename();
}

static bool 
pw_is_user(char *user) {
    bool found = false;
    char *u, *p, *d, *s;
    int opt4;
    int match;

    if (pw_open(1) < 0)
        return true; /* FIXME: Might want to return an error code */
    //
    while (pw_read_line(&u, &p, &d, &s, &opt4) == 0) {
    // Check if the user matches u
    // if so set found to true;
    match = strncmp(user, u, strlen(u));
    if (match == 0) {
            // We found a match
            found = true;
        }
    }
    return found;
}

// Returns 0 - ok, -1 failed
static int
pw_del_user(char *user) {
    bool found = false;
    char *user_name, *password, *directory_name;
    char *privilege;
    int opt4;
    int match;
    int result;

    if (pw_open(1) < 0)
        return -1;

    while (pw_read_line(&user_name, &password, &directory_name,
             &privilege, &opt4) == 0) {
        match = strncmp(user, user_name, strlen(user_name));
        if (match == 0) {
            // We found a match
            found = true;
            // Do not write the record to the password file
            // so do nothing else
        }
        if (match != 0) {
            // not a match so we write this back to the password
            // file.
            pw_write_line(user_name, password, directory_name, privilege, opt4);
        }
    }

    result = pw_close_rename();
    if (found == true)
        result = 0;
    // Does not delete the directory or files of the user
    return result;
}


struct user_funcs const user_pw = {
    pw_validate, pw_urd, pw_change, pw_set_opt4, pw_set_priv, pw_get_priv,
    pw_add_user, pw_is_user, pw_del_user
};
