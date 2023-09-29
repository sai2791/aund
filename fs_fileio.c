/*-
 * Copyright (c) 2013, 2010 Simon Tatham
 * Copyright (c) 1998, 2010 Ben Harris
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
 * This is part of aund, an implementation of Acorn Universal
 * Networking for Unix.
 */ 
/*
 * fs_fileio.c - File server file I/O calls
 */

#include <fts.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/stat.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "aun.h"
#include "fs_proto.h"
#include "fs_errors.h"
#include "extern.h"
#include "fileserver.h"

#define OUR_DATA_PORT 0x97

static ssize_t fs_data_send(struct fs_context *, int, size_t, uint8_t);
static ssize_t fs_data_recv(struct fs_context *, int, size_t, int);
static int fs_close1(struct fs_context *c, int h);

/*
 * Acorn OSes implement mandatory locking in OSFIND, delegating that
 * to the fileserver on Econet.  This implementation uses BSD flock()
 * locks to achieve the same effect.  On real BSD systems, we can use
 * O_SHLOCK and O_EXLOCK, but Linux doesn't have these and we have to
 * resort to calling flock() after open().
 *
 * Using flock() causes a problem when creating a new file, because
 * another client could get in after the file is created and before we
 * lock it, which shouldn't be able to happen.  This doesn't matter
 * while aund is single-threaded, but once it isn't, the best approach
 * is probably to flock() the containing directory while opening files
 * in it.
 */

#if defined(O_SHLOCK) && defined(O_EXLOCK)
#define HAVE_O_xxLOCK
#endif

void
fs_open(struct fs_context *c)
{
    struct ec_fs_reply_open reply;
    struct ec_fs_reply_open_32 reply_32;
    struct ec_fs_req_open *request;
    struct stat st;
    char *upath;
    int openopt, fd;
    uint8_t h;
    bool is_owner  = false;
    bool did_create = false;
    bool found_file = true;  // Assume the file exists, but check later
    FTS *ftsp;
    FTSENT *f;
    char *path_argv[2];

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }
    request = (struct ec_fs_req_open *)(c->req);
    request->path[strcspn(request->path, "\r")] = '\0';
    if (debug) printf("open [%s/%s, %s]\n",
        request->must_exist ? "exist":"create",
        request->read_only ? "read":"rdwr", request->path);
    upath = fs_unixify_path(c, request->path);
    if (upath == NULL) return;

    is_owner = fs_is_owner(c, upath);

    if ((fd = open(upath, O_RDONLY)) == -1) {
        // we cannot open the file for read only so 
        // it probably does not exist
        found_file = false;
        }

    if (fstat(fd, &st) == -1) {
        // As a check I will try to see if I can get any information
        // about the file as a secondary check
        found_file = false;
    }
    close(fd);

    if ((found_file == false) && (request->must_exist))
    {
        fs_err(c, EC_FS_E_CHANNEL);
        free(upath);
        return;
    }

    openopt = 0;
    if (!request->must_exist) {
      openopt |= O_CREAT;
      if ( found_file == false)
      {
          did_create = true;
      }
      if (is_owner == false) {
        fs_err(c, EC_FS_E_NOACCESS);
        free(upath);
        return;
      }
    }

    if (request->read_only) {
        openopt |= O_RDONLY;
#ifdef HAVE_O_xxLOCK
        openopt |= O_SHLOCK | O_NONBLOCK;
#endif
    } else {
        openopt |= O_RDWR;
#ifdef HAVE_O_xxLOCK
        openopt |= O_EXLOCK | O_NONBLOCK;
#endif
    }
    if ((h = fs_open_handle(c->client, upath, openopt, true)) == 0) {
#ifdef HAVE_O_xxLOCK
        if (errno == EAGAIN)
            fs_err(c, EC_FS_E_OPEN);
        else
#endif
            fs_errno(c);
        free(upath);

        return;
        }

    path_argv[0] = upath;
    path_argv[1] = NULL;

    // Acorn Permissions on file handle 
    // moved to fs_handle where the handle
    // is created.

    c->client->handles[h]->read_only = request->read_only;
    ftsp = fts_open(path_argv, FTS_LOGICAL, NULL);
    f = fts_read(ftsp);
    if (f->fts_statp->st_mode & S_IWUSR) {
        c->client->handles[h]->can_write = true;
    }
    if (f->fts_statp->st_mode & S_IWOTH) {
        c->client->handles[h]->can_write = true;
    }
    if (f->fts_statp->st_mode & S_IRUSR) {
        c->client->handles[h]->can_read = true;
    }
    if (f->fts_statp->st_mode & S_IROTH) {
        c->client->handles[h]->can_read = true;
    }
    if (f->fts_statp->st_mode & S_IXUSR) {
        c->client->handles[h]->is_locked = true;
    }
    fts_close(ftsp);

    // OPENIN sends:   open file for read only
    // 7: non-zero, file must exist (NFS 3.60 sends &80)
    // 8: non-zero, open file for reading (NFS 3.60 sends &01)

    // OPENOUT sends:  open file only creates if it does not exist
    // 7: zero, delete any existing file
    // 8: zero, not read-only, ie open file for output

    // OPENUP sends:   open file for read/write but file must exist
    // 7: non-zero, file must exist (NFS 3.60 sends &80)
    // 8: zero, not read-only, ie open file for output

    if (h != 0) {
      c->client->handles[h]->is_owner = is_owner;
      c->client->handles[h]->did_create = did_create;
    }
    free(upath);
#ifdef HAVE_O_xxLOCK
    if ((openopt = fcntl(c->client->handles[h]->fd, F_GETFL)) == -1 ||
            fcntl(c->client->handles[h]->fd,
                F_SETFL, openopt & ~O_NONBLOCK) == -1) {
        fs_errno(c);
        fs_close_handle(c->client, h);
        return;
    }
#else
    if (flock(c->client->handles[h]->fd,
                (request->read_only ? LOCK_SH : LOCK_EX) | LOCK_NB) == -1) {
        if (errno == EAGAIN)
            fs_err(c, EC_FS_E_OPEN);
        else
            fs_errno(c);
        fs_close_handle(c->client, h);
        return;
    }
#endif
    if (c->req->function == EC_FS_FUNC_OPEN) {
        reply.std_tx.command_code = EC_FS_CC_DONE;
        reply.std_tx.return_code = EC_FS_RC_OK;
        reply.handle = h;
        fs_reply(c, &(reply.std_tx), sizeof(reply));
    } else {
        reply_32.std_tx.command_code = EC_FS_CC_DONE;
        reply_32.std_tx.return_code = EC_FS_RC_OK;
        reply_32.type = fs_mode_to_type(st.st_mode);
        reply_32.access = fs_mode_to_access(st.st_mode);
        reply_32.unknown = 0xff;
        reply_32.handle = h;
        fs_write_val(reply_32.size, st.st_size, sizeof(reply_32.size));
        fs_write_val(reply_32.size1, st.st_size, sizeof(reply_32.size1));
        fs_reply(c, &(reply_32.std_tx), sizeof(reply_32));
    }
}

void
fs_close(struct fs_context *c)
{
    struct ec_fs_reply reply;
    struct ec_fs_req_close *request;
    int h, error, thiserr;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }
    request = (struct ec_fs_req_close *)(c->req);
    if (debug) printf("close [%d]\n", request->handle);
    if (request->handle == 0) {
        error = 0;
        for (h = 1; h < c->client->nhandles; h++)
            if (c->client->handles[h] &&
                c->client->handles[h]->type == FS_HANDLE_FILE &&
                (thiserr = fs_close1(c, h)))
                error = thiserr;
    } else {
        error = fs_close1(c, request->handle);
    }
    if (error) 
        {
            fs_errno(c);
        } else {
            reply.command_code = EC_FS_CC_DONE;
            reply.return_code = EC_FS_RC_OK;
            fs_reply(c, &reply, sizeof(reply));
        }
}

/*
 * Close a single handle.
 */
static int
fs_close1(struct fs_context *c, int h)
{
    struct fs_handle *hp;
    int error = 0;

    if ((h = fs_check_handle(c->client, h)) != 0) {
        hp = c->client->handles[h];
        /* ESUG says this is needed */
        if (hp->type == FS_HANDLE_FILE && fsync(hp->fd) == -1) {
            if (errno != EINVAL) /* fundamentally unfsyncable */
                error = errno;
        }
        close(hp->fd);
        fs_close_handle(c->client, h);
    }
    return error;
}

void
fs_get_args(struct fs_context *c)
{
    struct stat st;
    struct ec_fs_reply_get_args reply;
    struct ec_fs_reply_get_args_32 reply_32;
    off_t ptr;
    int h, fd;
    uint8_t handle;
    uint8_t arg;
    bool is_32 = false;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }

    if (c->req->function == EC_FS_FUNC_GET_ARGS) {
        struct ec_fs_req_get_args *request;

        request = (struct ec_fs_req_get_args *)(c->req);
        handle = request->handle;
        arg = request->arg;
        if (debug) printf("get args [%d, %d]", handle, arg);
    } else {
        struct ec_fs_req_get_args_32 *request_32;

        request_32 = (struct ec_fs_req_get_args_32 *)(c->req);
        handle = request_32->handle;
        arg = request_32->arg;
        if (debug) printf("get args 32 [%d, %d]", handle, arg);
        is_32 = true;
    }
    if ((h = fs_check_handle(c->client, handle)) != 0) {
        fd = c->client->handles[h]->fd;
        switch (arg) {
        case EC_FS_ARG_PTR:
            if ((ptr = lseek(fd, 0, SEEK_CUR)) == -1) {
                fs_errno(c);
                return;
            }
            if (is_32)
                fs_write_val(reply_32.val, ptr, sizeof(reply_32.val));
            else
                fs_write_val(reply.val, ptr, sizeof(reply.val));
            break;
        case EC_FS_ARG_EXT:
            if (fstat(fd, &st) == -1) {
                fs_errno(c);
                return;
            }
            if (is_32)
                fs_write_val(reply_32.val, st.st_size, sizeof(reply_32.val));
            else
                fs_write_val(reply.val, st.st_size, sizeof(reply.val));
            break;
        case EC_FS_ARG_SIZE:
            if (fstat(fd, &st) == -1) {
                fs_errno(c);
                return;
            }
            if (is_32)
                fs_write_val(reply_32.val, st.st_blocks * S_BLKSIZE,
                    sizeof(reply_32.val));
            else
                fs_write_val(reply.val, st.st_blocks * S_BLKSIZE,
                    sizeof(reply.val));
            break;
        default:
            if (debug) printf("\n");
            fs_err(c, EC_FS_E_BADARGS);
            return;
        }
        // Differences between apple uint64_t and linux
        // removes complier warning
        #ifdef __APPLE__
        if (debug)
            printf(" <- %llu\n",
                fs_read_val(reply.val, sizeof(reply.val)));
        #endif
            
        #ifdef __LINUX__
        if (debug)
            printf(" <- %lu\n",
                fs_read_val(reply.val, sizeof(reply.val)));
        #endif    

        if (c->req->function == EC_FS_FUNC_GET_ARGS) {
            reply.std_tx.command_code = EC_FS_CC_DONE;
            reply.std_tx.return_code = EC_FS_RC_OK;
            fs_reply(c, &(reply.std_tx), sizeof(reply));
        } else {
            reply_32.std_tx.command_code = EC_FS_CC_DONE;
            reply_32.std_tx.return_code = EC_FS_RC_OK;
            fs_reply(c, &(reply_32.std_tx), sizeof(reply_32));
        }
    } else {
        fs_err(c, EC_FS_E_CHANNEL);
    }
}

void
fs_set_args(struct fs_context *c)
{
    struct ec_fs_reply reply;
    off_t val;
    int h, fd;
    uint8_t handle;
    uint8_t arg;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }
    if (c->req->function == EC_FS_FUNC_SET_ARGS) {
        struct ec_fs_req_set_args *request;

        request = (struct ec_fs_req_set_args *)(c->req);
        val = fs_read_val(request->val, sizeof(request->val));
        handle = request->handle;
        arg = request->arg;
        if (debug)
            printf("set args [%d, %d := %ju]\n",
                request->handle, request->arg, (uintmax_t)val);
    } else {
        struct ec_fs_req_set_args_32 *request_32;

        request_32 = (struct ec_fs_req_set_args_32 *)(c->req);
        val = fs_read_val(request_32->val, sizeof(request_32->val));
        handle = request_32->handle;
        arg = request_32->arg;
        if (debug)
            printf("set args 32 [%d, %d := %ju]\n",
                request_32->handle, request_32->arg, (uintmax_t)val);
    }
    if ((h = fs_check_handle(c->client, handle)) != 0) {
        fd = c->client->handles[h]->fd;
        switch (arg) {
        case EC_FS_ARG_PTR:
            if (lseek(fd, val, SEEK_SET) == -1) {
                fs_errno(c);
                return;
            }
            break;
        case EC_FS_ARG_EXT:
            if (ftruncate(fd, val) == -1) {
                fs_errno(c);
                return;
            }
            break;
        case EC_FS_ARG_SIZE:
            // I don't understand this - it does something 32 bit mode.
            // Just say we handled it - seems to keep
            // everything happy.
            break;
        default:
            fs_error(c, 0xff, "bad argument to set_args");
            return;
        }
        reply.command_code = EC_FS_CC_DONE;
        reply.return_code = EC_FS_RC_OK;
        fs_reply(c, &reply, sizeof(reply));
    } else {
        fs_err(c, EC_FS_E_CHANNEL);
    }
}

static int
fs_randomio_common(struct fs_context *c, int h)
{
    off_t off;
    int fd;

    fd = c->client->handles[h]->fd;
    printf(" [[->%c %0x]]", (c->req->aun.flag & 1) ? '/' : '\\', (c->req->aun.flag));
    if (c->client->handles[h]->sequence != (c->req->aun.flag & 1)) {
        /*
         * Different sequence number from last request.  Save
         * our current offset.
         */
        if ((off = lseek(fd, 0, SEEK_CUR)) == -1) {
            fs_errno(c);
            return -1;
        }
        c->client->handles[h]->oldoffset = off;
        c->client->handles[h]->sequence = (c->req->aun.flag & 1);
    } else {
        /* This is a repeated request. */
        if (debug) printf("<repeat>");
        off = c->client->handles[h]->oldoffset;
        if (lseek(fd, off, SEEK_SET) == -1) {
            fs_errno(c);
            return -1;
        }
    }
    return 0;
}

void
fs_putbyte(struct fs_context *c)
{
    struct ec_fs_reply reply;
    struct ec_fs_req_putbyte *request;
    int h, fd;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }
    request = (struct ec_fs_req_putbyte *)(c->req);
    if (debug)
        printf("putbyte [%d, 0x%02x]\n",
            request->handle, request->byte);
    if ((h = fs_check_handle(c->client, request->handle)) != 0) {
        if (fs_randomio_common(c, request->handle)) return;
        if (c->client->handles[h]->read_only)
        {
            fs_err(c, EC_FS_E_RDONLY);
            return;
        }
        if (c->client->handles[h]->can_write == false)
        {
            // we are trying to write to a file that we do 
            // not have permission for
            fs_err(c, EC_FS_E_NOACCESS);
            return;
        }
        if (c->client->handles[h]->is_locked == true)
        {
            // we are trying to write to a file that is locked
            fs_err(c, EC_FS_E_LOCKED);
            return;
        }
        fd = c->client->handles[h]->fd;
        if (write(fd, &request->byte, 1) < 0) {
            fs_errno(c);
            return;
        }
        reply.command_code = EC_FS_CC_DONE;
        reply.return_code = EC_FS_RC_OK;
        fs_reply(c, &reply, sizeof(reply));
    } else {
        fs_err(c, EC_FS_E_CHANNEL);
    }
}

static int
at_eof(int fd)
{
    struct stat st;
    off_t off = lseek(fd, 0, SEEK_CUR);
    return (off != (off_t)-1 && fstat(fd, &st) >= 0 && off >= st.st_size);
}

void
fs_get_eof(struct fs_context *c)
{
    struct ec_fs_reply_get_eof reply;
    struct ec_fs_req_get_eof *request;
    int h, fd;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }
    request = (struct ec_fs_req_get_eof *)(c->req);
    if (debug) printf("get eof [%d]\n", request->handle);
    if ((h = fs_check_handle(c->client, request->handle)) != 0) {
        fd = c->client->handles[h]->fd;
        reply.status = at_eof(fd) ? 0xFF : 0;
        reply.std_tx.command_code = EC_FS_CC_DONE;
        reply.std_tx.return_code = EC_FS_RC_OK;
        fs_reply(c, &(reply.std_tx), sizeof(reply));
    }
}

void
fs_getbytes(struct fs_context *c)
{
    struct ec_fs_reply reply1;
    int h, fd;
    off_t off;
    size_t size, got;
    uint8_t handle;
    bool use_ptr = false;
    uint8_t reply_port;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }

    if (c->req->function == EC_FS_FUNC_GETBYTES) {
        struct ec_fs_req_getbytes *request;
        request = (struct ec_fs_req_getbytes *)(c->req);
        size = fs_read_val(request->nbytes, sizeof(request->nbytes));
        off = fs_read_val(request->offset, sizeof(request->offset));
        handle = request->handle;
        use_ptr = request->use_ptr;
        reply_port = c->req->urd;
        if (debug)
            printf("getbytes [%d, %zu%s%ju]\n",
                request->handle, size, request->use_ptr ? "!" : "@",
                (uintmax_t)off);
    } else {
        struct ec_fs_req_getbytes_32 *request_32;
        request_32 = (struct ec_fs_req_getbytes_32 *)(c->req);
        size = fs_read_val(request_32->nbytes, sizeof(request_32->nbytes));
        off = fs_read_val(request_32->offset, sizeof(request_32->offset));
        handle = request_32->handle;
        reply_port = request_32->reply_port;
        /* FIXME: Where is use_ptr in this request */
        if (debug)
            printf("getbytes 32 [%d, %zu@%ju]\n",
                request_32->handle, size,
                (uintmax_t)off);
    }
    if ((h = fs_check_handle(c->client, handle)) != 0) {
        if (fs_randomio_common(c, handle)) return;
        if (c->client->handles[h]->can_read == false)
        {
            // We are trying to read from a file without the correct permission
            fs_err(c, EC_FS_E_NOACCESS);
            return;
        }
        fd = c->client->handles[h]->fd;
        if (!use_ptr) { 
            if (lseek(fd, off, SEEK_SET) == -1) {
                fs_errno(c);
                return;
            }
        }
        reply1.command_code = EC_FS_CC_DONE;
        reply1.return_code = EC_FS_RC_OK;
        fs_reply(c, &reply1, sizeof(reply1));
        got = fs_data_send(c, fd, size, reply_port);
        if (got == -1) {
            /* Error */
            fs_errno(c);
        } else {
            if (c->req->function == EC_FS_FUNC_GETBYTES) {
                struct ec_fs_reply_getbytes2 reply2;

                reply2.std_tx.command_code = EC_FS_CC_DONE;
                reply2.std_tx.return_code = EC_FS_RC_OK;
                if (got == size && !at_eof(fd))
                    reply2.flag = 0;
                else
                    reply2.flag = 0x80; /* EOF reached */
                fs_write_val(reply2.nbytes, got, sizeof(reply2.nbytes));
                fs_reply(c, &(reply2.std_tx), sizeof(reply2));
            } else {
                struct ec_fs_reply_getbytes2_32 reply2_32;

                reply2_32.std_tx.command_code = EC_FS_CC_DONE;
                reply2_32.std_tx.return_code = EC_FS_RC_OK;
                if (got == size && !at_eof(fd))
                    reply2_32.flag = 0;
                else
                    reply2_32.flag = 0x80; /* EOF reached */
                fs_write_val(reply2_32.nbytes, got, sizeof(reply2_32.nbytes));
                fs_reply(c, &(reply2_32.std_tx), sizeof(reply2_32));
            }
        }
    } else {
        fs_err(c, EC_FS_E_CHANNEL);
    }
}

void
fs_getbyte(struct fs_context *c)
{
    struct ec_fs_reply_getbyte reply;
    struct ec_fs_req_getbyte *request;
    int h, fd, ret;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }
    request = (struct ec_fs_req_getbyte *)(c->req);
    if (debug) printf("getbyte [%d]\n", request->handle);
    if ((h = fs_check_handle(c->client, request->handle)) != 0) {
        if (fs_randomio_common(c, request->handle)) return;
        if (c->client->handles[h]->can_read == false)
        {
            // We are trying to read from a file that we do not
            // have permissions to.
            fs_err(c, EC_FS_E_NOACCESS);
            return;
        }
        fd = c->client->handles[h]->fd;
        if ((ret = read(fd, &reply.byte, 1)) < 0) {
            fs_errno(c);
            return;
        }
        reply.std_tx.command_code = EC_FS_CC_DONE;
        reply.std_tx.return_code = EC_FS_RC_OK;
        if (ret == 0) {
            reply.flag = 0xC0;
            reply.byte = 0xFF;
        } else {
            reply.flag = at_eof(fd) ? 0x80 : 0;
        }
        fs_reply(c, &(reply.std_tx), sizeof(reply));
    } else {
        fs_err(c, EC_FS_E_CHANNEL);
    }
}

void
fs_putbytes(struct fs_context *c)
{
    struct ec_fs_reply_putbytes1 reply1;
    struct ec_fs_reply_putbytes2 reply2;
    struct ec_fs_reply_putbytes2_32 reply2_32;
    int h, fd, replyport;
    off_t off;
    size_t size, got;
    uint8_t handle;
    int ackport;
    uint8_t use_ptr = false;
    bool api_32 = false;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }
    replyport = c->req->reply_port;
    if (c->req->function == EC_FS_FUNC_PUTBYTES) {
        struct ec_fs_req_putbytes *request;

        request = (struct ec_fs_req_putbytes *)(c->req);
        size = fs_read_val(request->nbytes, sizeof(request->nbytes));
        off = fs_read_val(request->offset, sizeof(request->offset));
        use_ptr = request->use_ptr;
        if (debug)
            printf("putbytes [%d, %zu%s%ju]\n",
                request->handle, size, request->use_ptr ? "!" : "@",
                (uintmax_t)off);
        handle = request->handle;
        ackport = c->req->urd;
    } else {
        struct ec_fs_req_putbytes_32 *request_32;

        api_32 = true;
        request_32 = (struct ec_fs_req_putbytes_32 *)(c->req);
        size = fs_read_val(request_32->nbytes, sizeof(request_32->nbytes));
        off = fs_read_val(request_32->offset, sizeof(request_32->offset));
        /* FIXME: Is use_ptr used here? */
        if (debug)
            printf("putbytes 32 [%d, %zu@%ju]\n",
                request_32->handle, size,
                (uintmax_t)off);
        handle = request_32->handle;
        ackport = request_32->ack_port;
    }
    if ((h = fs_check_handle(c->client, handle)) != 0) {
        if (fs_randomio_common(c, handle)) return;
        if (c->client->handles[h]->read_only)
        {
            // Trying to write to a read only file
            fs_err(c, EC_FS_E_RDONLY);
            return;
        }
        if (c->client->handles[h]->can_write == false)
        {
            // we are trying to write to a file without permission
            fs_err(c, EC_FS_E_NOACCESS);
            return;
        }
        if (c->client->handles[h]->is_locked == true)
        {
            // we are trying to write to a locked file 
            fs_err(c, EC_FS_E_LOCKED);
            return;
        }
        fd = c->client->handles[h]->fd;
        if (!use_ptr) {
            if (lseek(fd, off, SEEK_SET) == -1) {
                if (debug) printf("Fs_file error\n");
                fs_errno(c);
                return;
            }
        }
        reply1.std_tx.command_code = EC_FS_CC_DONE;
        reply1.std_tx.return_code = EC_FS_RC_OK;
        reply1.data_port = OUR_DATA_PORT;

        if (debug) {
            printf("blocksize: %lu, maxsize: %d \n",
                    sizeof(reply1.block_size),
                    aunfuncs->max_block);
        }

        fs_write_val(reply1.block_size, aunfuncs->max_block,
                sizeof(reply1.block_size));
        fs_reply(c, &(reply1.std_tx), sizeof(reply1));
        got = fs_data_recv(c, fd, size, ackport);
        if (got == -1) {
            /* Error */
            if (debug) printf("got error\n");
            fs_errno(c);
        } else {
            if (api_32) {
                reply2_32.flag = 0;
                reply2_32.std_tx.command_code = EC_FS_CC_DONE;
                reply2_32.std_tx.return_code = EC_FS_RC_OK;
                fs_write_val(reply2_32.nbytes, got, sizeof(reply2_32.nbytes));
                c->req->reply_port = replyport;
                fs_reply(c, &(reply2_32.std_tx), sizeof(reply2_32));
            } else {
                reply2.std_tx.command_code = EC_FS_CC_DONE;
                reply2.std_tx.return_code = EC_FS_RC_OK;
                reply2.zero = 0;
                fs_write_val(reply2.nbytes, got, sizeof(reply2.nbytes));
                c->req->reply_port = replyport;
                fs_reply(c, &(reply2.std_tx), sizeof(reply2));
            }
        }
    } else {
        fs_err(c, EC_FS_E_CHANNEL);
    }
}

void
fs_load(struct fs_context *c)
{
    struct ec_fs_reply_load1 reply1;
    struct ec_fs_reply_load1_32 reply1_32;
    struct ec_fs_reply_load2 reply2;
    char *upath = NULL;
    char *upathlib, *path_argv[3];
    int fd, as_command;
    size_t got;
    FTS *ftsp;
    FTSENT *f;
    bool is_owner = false;
    bool can_read = false;
    bool use_reply_32 = false;
    char *ro_path = NULL;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }
    if (c->req->function == EC_FS_FUNC_LOAD ||
        c->req->function == EC_FS_FUNC_LOAD_COMMAND) {
        struct ec_fs_req_load *request;

        request = (struct ec_fs_req_load *)(c->req);
        request->path[strcspn(request->path, "\r")] = '\0';
        as_command = c->req->function == EC_FS_FUNC_LOAD_COMMAND;
        if (debug) printf("load%s [%s]\n",
             as_command ? " as command" : "", request->path);
        /*
         * 8-bit clients tend to send the whole command line for "load
         * as command", so we trim it for them.
         */
        request->path[strcspn(request->path, " ")] = '\0';
        upath = fs_unixify_path(c, request->path);
        ro_path = request->path;
    } else if (c->req->function == EC_FS_FUNC_LOAD_32) {
        struct ec_fs_req_load_32 *request_32;

        as_command = false;
        use_reply_32 = true;
        request_32 = (struct ec_fs_req_load_32 *)(c->req);
        request_32->path[strcspn(request_32->path, "\r")] = '\0';
        /*
         * 8-bit clients tend to send the whole command line for "load
         * as command", so we trim it for them.
         */
        request_32->path[strcspn(request_32->path, " ")] = '\0';
        upath = fs_unixify_path(c, request_32->path);
        ro_path = request_32->path;
    }

    if (upath == NULL) return;
    is_owner = fs_is_owner(c, upath);

    path_argv[0] = upath;
    path_argv[1] = NULL;
    if (as_command) {
        c->req->csd = c->req->lib;
        upathlib = fs_unixify_path(c, ro_path);
        if (upathlib == NULL) {
            free(upath);
            return;
        }
        path_argv[1] = upathlib;
        path_argv[2] = NULL;
    }
    ftsp = fts_open(path_argv, FTS_LOGICAL, NULL);
    f = fts_read(ftsp);
    if (as_command && f->fts_info == FTS_NS && f->fts_errno == ENOENT)
        f = fts_read(ftsp);
    if (f->fts_info == FTS_ERR || f->fts_info == FTS_NS) {
        fs_errno(c);
        goto out;
    }
    if (S_ISDIR(f->fts_statp->st_mode)) {
        fs_err(c, EC_FS_E_ISDIR);
        goto out;
    }

    if ((fd = open(f->fts_accpath, O_RDONLY)) == -1) {
        fs_errno(c);
        goto out;
    }

    if (is_owner == true) 
    {
        if (f->fts_statp->st_mode & S_IRUSR)
        {
            can_read = true;
        } 
    }
    if (is_owner == false)
    {
        if (f->fts_statp->st_mode & S_IROTH) 
        {
            can_read = true;
        }
    }

    if (can_read == false) {
        fs_err(c, EC_FS_E_NOACCESS);
        goto out;
    }

    if (use_reply_32) {
        fs_get_meta(f, &reply1_32.meta);
        fs_write_val(reply1_32.size, f->fts_statp->st_size, sizeof(reply1_32.size));
        reply1_32.access = fs_mode_to_access(f->fts_statp->st_mode);
        fs_write_date(&(reply1_32.date), fs_get_birthtime(f));
        reply1_32.std_tx.command_code = EC_FS_CC_DONE;
        reply1_32.std_tx.return_code = EC_FS_RC_OK;
        fs_reply(c, &(reply1_32.std_tx), sizeof(reply1_32));
        reply2.std_tx.command_code = EC_FS_CC_DONE;
        reply2.std_tx.return_code = EC_FS_RC_OK;
    } else {
        fs_get_meta(f, &(reply1.meta));
        fs_write_val(reply1.size, f->fts_statp->st_size, sizeof(reply1.size));
        reply1.access = fs_mode_to_access(f->fts_statp->st_mode);
        fs_write_date(&(reply1.date), fs_get_birthtime(f));
        reply1.std_tx.command_code = EC_FS_CC_DONE;
        reply1.std_tx.return_code = EC_FS_RC_OK;
        fs_reply(c, &(reply1.std_tx), sizeof(reply1));
        reply2.std_tx.command_code = EC_FS_CC_DONE;
        reply2.std_tx.return_code = EC_FS_RC_OK;
    }
    got = fs_data_send(c, fd, f->fts_statp->st_size, c->req->urd);
    if (got == -1) {
        /* Error */
        fs_errno(c);
    } else {
        fs_reply(c, &(reply2.std_tx), sizeof(reply2));
    }
    close(fd);
out:
    fts_close(ftsp);
    free(upath);
    if (as_command) free(upathlib);
    return;
}

void
fs_save(struct fs_context *c)
{
    struct ec_fs_reply_save1 reply1;
    struct ec_fs_reply_save2 reply2;
    struct ec_fs_meta meta;
    char *upath, *path_argv[2];
    int fd, ackport, replyport;
    size_t size, got;
    FTS *ftsp;
    FTSENT *f;
    bool is_owner;
    bool can_write;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }
    if (c->req->function == EC_FS_FUNC_SAVE) {
        struct ec_fs_req_save *request;

        request = (struct ec_fs_req_save *)(c->req);
        request->path[strcspn(request->path, "\r")] = '\0';
        meta = request->meta;
        if (debug) printf("save [%s]\n", request->path);
        size = fs_read_val(request->size, sizeof(request->size));
        upath = fs_unixify_path(c, request->path);
        ackport = c->req->urd;
    } else {
        struct ec_fs_req_save_32 *request_32;

        request_32 = (struct ec_fs_req_save_32 *)(c->req);
        request_32->path[strcspn(request_32->path, "\r")] = '\0';
        meta = request_32->meta;
        if (debug) printf("save 32 [%s]\n", request_32->path);
        size = fs_read_val(request_32->size, sizeof(request_32->size));
        upath = fs_unixify_path(c, request_32->path);
        ackport = request_32->ack_port;
    }
    replyport = c->req->reply_port;
    if (upath == NULL) return;

    /* Check that we have owner permission in the directory we 
       are about to save in */

    is_owner = fs_is_owner(c , upath);

    if (is_owner)
    {
        if ((fd = open(upath, O_CREAT|O_TRUNC|O_RDWR, 0666)) == -1) {
            fs_errno(c);
            free(upath);
            return;
        }
    } else {
    if ((fd = open(upath, O_TRUNC|O_RDWR, 0666)) == -1) {
            fs_errno(c);
            free(upath);
        return;
        }
    }

    // Now if the data exists we need to check if we actually 
    // have the correct write permisson
    
    can_write = false;  // Assume we dont have access
    path_argv[0] = upath;
    path_argv[1] = NULL;
    ftsp = fts_open(path_argv, FTS_LOGICAL, NULL);
    f = fts_read(ftsp);
    if (f->fts_statp->st_mode & S_IWUSR)
    {
        // Owner permission to write
        if (is_owner == true)
        {
            if (f->fts_statp->st_mode & S_IXUSR)
            {
                // the file is locked
                goto locked;
            }
            can_write = true;
        }
    }
    if (f->fts_statp->st_mode & S_IWOTH)
    {
        // Public permission to write
        if (is_owner == false) 
        {
            if (f->fts_statp->st_mode & S_IXUSR)
            {
                // the file is locked
                goto locked;
            }
            can_write = true;
        }
    }
    // We can now decide if writing the file is allowed.
    // if is_owner and can_write then ok
    // is is_owner = false and can_write then ok

    if (can_write == false) {
        // need to delete the file we just created
        // Is this a TODO that has not been done?
        close(fd);
        fts_close(ftsp);
        goto not_allowed_write;
    }
    fts_close(ftsp);

    reply1.std_tx.command_code = EC_FS_CC_DONE;
    reply1.std_tx.return_code = EC_FS_RC_OK;
    reply1.data_port = OUR_DATA_PORT;
    fs_write_val(reply1.block_size, aunfuncs->max_block,
             sizeof(reply1.block_size));
    fs_reply(c, &(reply1.std_tx), sizeof(reply1));
    reply2.std_tx.command_code = EC_FS_CC_DONE;
    reply2.std_tx.return_code = EC_FS_RC_OK;
    got = fs_data_recv(c, fd, size, ackport);
    close(fd);
    if (got == -1) {
        /* Error */
        fs_errno(c);
    } else {
        /*
         * Write load and execute addresses from the
         * request, and return the file date in the
         * response.
         */
        path_argv[0] = upath;
        path_argv[1] = NULL;
        ftsp = fts_open(path_argv, FTS_LOGICAL, NULL);
        f = fts_read(ftsp);
        fs_set_meta(f, &meta);
        fs_write_date(&(reply2.date), fs_get_birthtime(f));
        reply2.access = fs_mode_to_access(f->fts_statp->st_mode);
        fts_close(ftsp);
        c->req->reply_port = replyport;
        fs_reply(c, &(reply2.std_tx), sizeof(reply2));
    }
    free(upath);
    return;

not_allowed_write:
    free(upath);
    fs_err(c, EC_FS_E_NOACCESS);    
    return;

locked:
    free(upath);
    fs_err(c, EC_FS_E_LOCKED);
    return;
}

void
fs_create(struct fs_context *c)
{
    struct ec_fs_reply_create reply;
    struct ec_fs_meta meta;
    char *upath, *path_argv[2];
    int fd, replyport;
    size_t size;
    FTS *ftsp;
    FTSENT *f;

    if (c->client == NULL) {
        fs_err(c, EC_FS_E_WHOAREYOU);
        return;
    }
    if (c->req->function == EC_FS_FUNC_CREATE) {
        struct ec_fs_req_create *request;

        request = (struct ec_fs_req_create *)(c->req);
        request->path[strcspn(request->path, "\r")] = '\0';
        meta = request->meta;
        if (debug) printf("create [%s]\n", request->path);
        size = fs_read_val(request->size, sizeof(request->size));
        upath = fs_unixify_path(c, request->path);
    } else {
        struct ec_fs_req_create_32 *request_32;

        request_32 = (struct ec_fs_req_create_32 *)(c->req);
        request_32->path[strcspn(request_32->path, "\r")] = '\0';
        meta = request_32->meta;
        if (debug) printf("create 32 [%s]\n", request_32->path);
        size = fs_read_val(request_32->size, sizeof(request_32->size));
        upath = fs_unixify_path(c, request_32->path);
    }
    replyport = c->req->reply_port;
    if (upath == NULL) return;
    if ((fd = open(upath, O_CREAT|O_TRUNC|O_RDWR, 0666)) == -1) {
        fs_errno(c);
        free(upath);
        return;
    }
    if (ftruncate(fd, size) != 0) {
        fs_errno(c);
        close(fd);
        free(upath);
        return;
    }
    reply.std_tx.command_code = EC_FS_CC_DONE;
    reply.std_tx.return_code = EC_FS_RC_OK;
    close(fd);
    /*
     * Write load and execute addresses from the
     * request, and return the file date in the
     * response.
     */
    path_argv[0] = upath;
    path_argv[1] = NULL;
    ftsp = fts_open(path_argv, FTS_LOGICAL, NULL);
    f = fts_read(ftsp);
    fs_set_meta(f, &meta);
    fs_write_date(&(reply.date), fs_get_birthtime(f));
    reply.access = fs_mode_to_access(f->fts_statp->st_mode);
    fts_close(ftsp);
    free(upath);
    c->req->reply_port = replyport;
    fs_reply(c, &(reply.std_tx), sizeof(reply));
}

static ssize_t
fs_data_send(struct fs_context *c, int fd, size_t size, uint8_t reply_port)
{
    struct aun_packet *pkt;
    void *buf;
    ssize_t result;
    size_t this, done;
    int faking;

    if ((pkt = malloc(sizeof(*pkt) +
        (size > aunfuncs->max_block ? aunfuncs->max_block : size))) ==
        NULL) { 
        fs_err(c, EC_FS_E_NOMEM);
        return -1;
    }
    buf = pkt->data;
    faking = 0;
    done = 0;
    while (size) {
        this = size > aunfuncs->max_block ? aunfuncs->max_block : size;
        if (!faking) {
            result = read(fd, buf, this);
            if (result > 0) {
                /* Normal -- the kernel had something for us */
                this = result;
                done += this;
            } else { /* EOF or error */
                if (result == -1) done = result;
                faking = 1;
            }
        }
        pkt->type = AUN_TYPE_UNICAST;
        pkt->dest_port = reply_port; //c->req->urd;
        pkt->flag = c->req->aun.flag & 1;
        if (aunfuncs->xmit(pkt, sizeof(*pkt) + this, c->from) == -1)
            warn("send data");
        size -= this;
    }
    free(pkt);
    return done;
}

static ssize_t
fs_data_recv(struct fs_context *c, int fd, size_t size, int ackport)
{
    struct aun_packet *pkt, *ack;
    ssize_t msgsize, result;
    struct aun_srcaddr from;
    size_t done;

    if ((ack = malloc(sizeof(*ack) + 1)) == NULL) {
        fs_err(c, EC_FS_E_NOMEM);
        return -1;
    }
    done = 0;
    while (size) {
        from = *c->from;
        pkt = aunfuncs->recv(&msgsize, &from, OUR_DATA_PORT);
        if (!pkt) {
            warn("receive data");
            return -1;     /* no reply: client has gone away */
        }
        msgsize -= sizeof(struct aun_packet);
        if (pkt->dest_port != OUR_DATA_PORT ||
            memcmp(&from, c->from, sizeof(from))) {
            fs_error(c, 0xFF, "I'm confused");
            return -1;
        }
        result = write(fd, pkt->data, msgsize);
        if (result < 0) {
            fs_errno(c);
            return -1;
        }
        size -= msgsize;
        if (size) {
            /*
             * Send partial ACK.
             */
            ack->type = AUN_TYPE_UNICAST;
            ack->dest_port = ackport;
            ack->flag = 0;
            ack->data[0] = 0;
            if (aunfuncs->xmit(ack, sizeof(*ack) + 1, c->from) ==
                -1)
                warn("send data");
        }
    }
    free(ack);
    return done;
}
