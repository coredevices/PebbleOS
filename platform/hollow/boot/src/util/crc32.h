#pragma once

//! \file
//! Calculate the CRC-32 checksum of data.
//!
//! The checksum is the standard CRC-32 used by zlib, PNG and others.
//! The model parameters for the algorithm, as described in A Painless Guide to
//! CRC Error Detection Algorithms (http://www.zlib.net/crc_v3.txt), are:
//!   Name: "CRC-32"
//!   Width: 32
//!   Poly: 04C11DB7
//!   Init: FFFFFFFF
//!   RefIn: True
//!   RefOut: True
//!   XorOut: FFFFFFFF
//!   Check: CBF43926

#include <stdint.h>
#include <string.h>

//! Update a running CRC-32 checksum with the bytes of data and return the
//! updated CRC-32. If data is NULL, the function returns the required initial
//! value for the CRC.
//!
//! This function is drop-in compatible with zlib's crc32 function.
//!
//! \par Usage
//! \code
//!   uint32_t crc = crc32(0, NULL, 0);
//!   while (read_buffer(data, length)) {
//!     crc = crc32(crc, data, length);
//!   }
//! \endcode
uint32_t crc32(uint32_t crc, const void * restrict data, size_t length);

//! The initial CRC register value for a standard CRC-32 checksum.
//!
//! It is the same value as is returned by the `crc32` function when data is
//! NULL.
//!
//! \code
//!   assert(CRC32_INIT == crc32(0, NULL, 0));
//! \endcode
#define CRC32_INIT (0)

//! The residue constant of the CRC-32 algorithm.
//!
//! If the CRC-32 value of a message is appended (little-endian) onto the
//! end of the message, the CRC-32 of the concatenated message and CRC will be
//! equal to CRC32_RESIDUE if the message has not been corrupted in transit.
#define CRC32_RESIDUE (0x2144DF1C)
