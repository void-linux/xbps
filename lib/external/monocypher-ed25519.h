// Monocypher version 3.1.3
//
// This file is dual-licensed.  Choose whichever licence you want from
// the two licences listed below.
//
// The first licence is a regular 2-clause BSD licence.  The second licence
// is the CC-0 from Creative Commons. It is intended to release Monocypher
// to the public domain.  The BSD licence serves as a fallback option.
//
// SPDX-License-Identifier: BSD-2-Clause OR CC0-1.0
//
// ------------------------------------------------------------------------
//
// Copyright (c) 2017-2019, Loup Vaillant
// All rights reserved.
//
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the
//    distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// ------------------------------------------------------------------------
//
// Written in 2017-2019 by Loup Vaillant
//
// To the extent possible under law, the author(s) have dedicated all copyright
// and related neighboring rights to this software to the public domain
// worldwide.  This software is distributed without any warranty.
//
// You should have received a copy of the CC0 Public Domain Dedication along
// with this software.  If not, see
// <https://creativecommons.org/publicdomain/zero/1.0/>

#ifndef ED25519_H
#define ED25519_H

#include "monocypher.h"

#ifdef MONOCYPHER_CPP_NAMESPACE
namespace MONOCYPHER_CPP_NAMESPACE {
#elif defined(__cplusplus)
extern "C" {
#endif

////////////////////////
/// Type definitions ///
////////////////////////

// Do not rely on the size or content on any of those types,
// they may change without notice.
typedef struct {
    uint64_t hash[8];
    uint64_t input[16];
    uint64_t input_size[2];
    size_t   input_idx;
} crypto_sha512_ctx;

typedef struct {
    uint8_t key[128];
    crypto_sha512_ctx ctx;
} crypto_hmac_sha512_ctx;

typedef struct {
    crypto_sign_ctx_abstract ctx;
    crypto_sha512_ctx        hash;
} crypto_sign_ed25519_ctx;
typedef crypto_sign_ed25519_ctx crypto_check_ed25519_ctx;

// SHA 512
// -------
void crypto_sha512_init  (crypto_sha512_ctx *ctx);
void crypto_sha512_update(crypto_sha512_ctx *ctx,
                          const uint8_t *message, size_t  message_size);
void crypto_sha512_final (crypto_sha512_ctx *ctx, uint8_t hash[64]);
void crypto_sha512(uint8_t hash[64], const uint8_t *message, size_t message_size);

// vtable for signatures
extern const crypto_sign_vtable crypto_sha512_vtable;


// HMAC SHA 512
// ------------
void crypto_hmac_sha512_init(crypto_hmac_sha512_ctx *ctx,
                             const uint8_t *key, size_t key_size);
void crypto_hmac_sha512_update(crypto_hmac_sha512_ctx *ctx,
                               const uint8_t *message, size_t  message_size);
void crypto_hmac_sha512_final(crypto_hmac_sha512_ctx *ctx, uint8_t hmac[64]);
void crypto_hmac_sha512(uint8_t hmac[64],
                        const uint8_t *key    , size_t key_size,
                        const uint8_t *message, size_t message_size);


// Ed25519
// -------

// Generate public key
void crypto_ed25519_public_key(uint8_t       public_key[32],
                               const uint8_t secret_key[32]);

// Direct interface
void crypto_ed25519_sign(uint8_t        signature [64],
                         const uint8_t  secret_key[32],
                         const uint8_t  public_key[32], // optional, may be 0
                         const uint8_t *message, size_t message_size);
int crypto_ed25519_check(const uint8_t  signature [64],
                         const uint8_t  public_key[32],
                         const uint8_t *message, size_t message_size);

// Incremental interface
void crypto_ed25519_sign_init_first_pass(crypto_sign_ctx_abstract *ctx,
                                         const uint8_t secret_key[32],
                                         const uint8_t public_key[32]);
#define crypto_ed25519_sign_update crypto_sign_update
#define crypto_ed25519_sign_init_second_pass crypto_sign_init_second_pass
// use crypto_ed25519_sign_update() again.
#define crypto_ed25519_sign_final crypto_sign_final

void crypto_ed25519_check_init(crypto_check_ctx_abstract *ctx,
                               const uint8_t signature[64],
                               const uint8_t public_key[32]);
#define crypto_ed25519_check_update crypto_check_update
#define crypto_ed25519_check_final crypto_check_final

void crypto_from_ed25519_private(uint8_t x25519[32], const uint8_t eddsa[32]);
#define crypto_from_ed25519_public crypto_from_eddsa_public

#ifdef __cplusplus
}
#endif

#endif // ED25519_H
