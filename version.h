/*-
 * Copyright (c) 2013, 2010 Ben Harris
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
 * At least two bits of protocol expect an Acornish x.yz
 * version-numbering scheme, so that's what aund uses.  To distingush
 * each release from the development versions leading to and from it,
 * we adopt a convention for 'z': Even values are releases (and thus
 * identify a single state of the source tree); odd values are
 * development versions between releases.
 *
 * When a release is made, then, there should be two successive
 * commits changing the version number, first to x.yz (the resulting
 * commit being tagged for release) and then to x.yz+1 for the next
 * phase of development.
 *
 * In the event of aund getting separate release branches, something
 * similar can probably be arranged.
 */

#ifndef AUND_VERSION_H
#define AUND_VERSION_H

/* The actual version number.  Coded in BCD for machine peek. */
#define AUND_VERSION_MAJOR 0x01
#define AUND_VERSION_MINOR 0x05

/* Description for "read file server version number".  Max 9 chars. */
#define AUND_FS_DESCR "aund"

/* Econet machine peek type bytes.  Randomly chosen. */
#define AUND_MACHINE_PEEK_HI 0x66
#define AUND_MACHINE_PEEK_LO 0x40

#endif /* !AUND_VERSION_H */
