/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "pbl/util/list.h"
#include "pbl/kernel/compiler.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @defgroup util_shared_cbuf Shared circular buffer
 * @ingroup util
 * @brief Circular buffer with one writer and several independent readers.
 *
 * Data stays available until every client has read it; each client has its own read index. The
 * writer can force the clients that fall behind forward to make room for new data. Writes are
 * dropped while there are no clients. One byte of the storage is never used. There is no locking.
 *
 * @code{.c}
 * static uint8_t s_storage[512];
 * static struct pbl_shared_cbuf s_buf;
 * static struct pbl_shared_cbuf_client s_client;
 *
 * pbl_shared_cbuf_init(&s_buf, s_storage, sizeof(s_storage));
 * pbl_shared_cbuf_add_client(&s_buf, &s_client);
 *
 * pbl_shared_cbuf_write(&s_buf, sample, sizeof(sample), true);
 *
 * uint16_t len;
 * pbl_shared_cbuf_read_consume(&s_buf, &s_client, sizeof(out), out, &len);
 * @endcode
 * @{
 */

/** @brief Read client, allocated by the caller. */
struct PBL_PACKED pbl_shared_cbuf_client {
  /** Node in the client list of the buffer. */
  ListNode list_node;
  /** Offset of the next byte to read for this client. */
  uint16_t read_index;
};

/** @brief Shared circular buffer state. */
struct pbl_shared_cbuf {
  /** Storage. */
  uint8_t *buffer;
  /** Size of @ref buffer in bytes. */
  uint16_t buffer_size;
  /** Offset where the next byte is written; a client whose read index equals it has no data. */
  uint16_t write_index;
  /** List of clients. */
  ListNode *clients;
};

/**
 * @brief Initialize a shared circular buffer, without clients.
 *
 * @param[out] buffer Buffer.
 * @param storage Storage, must outlive @p buffer.
 * @param storage_size Size of @p storage in bytes.
 */
void pbl_shared_cbuf_init(struct pbl_shared_cbuf *buffer, uint8_t *storage, uint16_t storage_size);

/**
 * @brief Write data to the buffer.
 *
 * @param buffer Buffer.
 * @param data Data to write.
 * @param length Number of bytes to write, less than the storage size.
 * @param advance_slackers Make room by moving the read index of the client farthest behind
 * forward, as many times as needed.
 * @return false if there are no clients, @p length does not fit the storage, or there is not
 * enough space and @p advance_slackers is false.
 */
bool pbl_shared_cbuf_write(struct pbl_shared_cbuf *buffer, const uint8_t *data, uint16_t length,
                           bool advance_slackers);

/**
 * @brief Reserve write space to fill in place.
 *
 * Fill the returned segment(s), then call pbl_shared_cbuf_write_commit() with the same length.
 * The reservation is split into two segments when it wraps the end of the storage.
 *
 * @param buffer Buffer.
 * @param length Number of bytes to reserve.
 * @param advance_slackers See pbl_shared_cbuf_write().
 * @param[out] seg1 First segment.
 * @param[out] seg1_length Length of the first segment, @p length when there is no wrap.
 * @param[out] seg2 Second segment, at the start of the storage, or NULL when there is no wrap.
 * @return false under the same conditions as pbl_shared_cbuf_write().
 */
bool pbl_shared_cbuf_write_reserve(struct pbl_shared_cbuf *buffer, uint16_t length,
                                   bool advance_slackers, uint8_t **seg1, uint16_t *seg1_length,
                                   uint8_t **seg2);

/**
 * @brief Commit a reservation made with pbl_shared_cbuf_write_reserve().
 *
 * @param buffer Buffer.
 * @param length Reserved length.
 */
void pbl_shared_cbuf_write_commit(struct pbl_shared_cbuf *buffer, uint16_t length);

/**
 * @brief Add a read client.
 *
 * The client starts with no data: it only sees what is written after it is added.
 *
 * @param buffer Buffer.
 * @param client Client, must stay valid until removed. Must not be added already.
 * @return true.
 */
bool pbl_shared_cbuf_add_client(struct pbl_shared_cbuf *buffer,
                                struct pbl_shared_cbuf_client *client);

/**
 * @brief Remove a read client.
 *
 * @param buffer Buffer.
 * @param client Client added with pbl_shared_cbuf_add_client().
 */
void pbl_shared_cbuf_remove_client(struct pbl_shared_cbuf *buffer,
                                   struct pbl_shared_cbuf_client *client);

/**
 * @brief Get a pointer to the oldest data of a client, without consuming it.
 *
 * When the requested data wraps around the end of the storage, only the part up to the end is
 * returned and @p length_out is smaller than @p length; read again after consuming it to get the
 * rest. @p data_out stays valid until the data is consumed.
 *
 * @param buffer Buffer.
 * @param client Client.
 * @param length Number of bytes to read.
 * @param[out] data_out Start of the data.
 * @param[out] length_out Number of contiguous bytes at @p data_out.
 * @return false if the client has fewer than @p length bytes available.
 */
bool pbl_shared_cbuf_read(const struct pbl_shared_cbuf *buffer,
                          struct pbl_shared_cbuf_client *client, uint16_t length,
                          const uint8_t **data_out, uint16_t *length_out);

/**
 * @brief Remove the oldest data of a client.
 *
 * @param buffer Buffer.
 * @param client Client.
 * @param length Number of bytes to remove.
 * @return false if the client has fewer than @p length bytes available (nothing is removed).
 */
bool pbl_shared_cbuf_consume(struct pbl_shared_cbuf *buffer, struct pbl_shared_cbuf_client *client,
                             uint16_t length);

/**
 * @brief Get the free space, limited by the client farthest behind.
 *
 * @param buffer Buffer.
 * @return Number of bytes that can be written without advancing any client.
 */
uint16_t pbl_shared_cbuf_get_write_space_remaining(const struct pbl_shared_cbuf *buffer);

/**
 * @brief Get the amount of data available to a client.
 *
 * @param buffer Buffer.
 * @param client Client.
 * @return Number of bytes the client can read.
 */
uint16_t pbl_shared_cbuf_get_read_space_remaining(const struct pbl_shared_cbuf *buffer,
                                                  struct pbl_shared_cbuf_client *client);

/**
 * @brief Copy out and consume data of a client, handling the wrap.
 *
 * @param buffer Buffer.
 * @param client Client.
 * @param length Maximum number of bytes to read.
 * @param[out] data Destination, at least @p length bytes.
 * @param[out] length_out Number of bytes read.
 * @return true if @p length bytes were read, false if fewer were available.
 */
bool pbl_shared_cbuf_read_consume(struct pbl_shared_cbuf *buffer,
                                  struct pbl_shared_cbuf_client *client, uint16_t length,
                                  uint8_t *data, uint16_t *length_out);

/** @brief Read client that keeps only a fraction of fixed-size items. */
struct pbl_shared_cbuf_subsampled_client {
  /** Underlying client. */
  struct pbl_shared_cbuf_client buffer_client;
  /** Numerator of the subsampling ratio. */
  uint32_t numerator;
  /** Denominator of the subsampling ratio. */
  uint32_t denominator;
  /** Tracks whether to copy or discard each successive item. */
  uint32_t subsample_state;
};

/**
 * @brief Add a read client that subsamples the data.
 *
 * @param buffer Buffer.
 * @param client Client, must stay valid until removed.
 * @param subsample_numerator Numerator of the initial subsampling ratio, at least 1.
 * @param subsample_denominator Denominator of the initial subsampling ratio, at least
 * @p subsample_numerator.
 * @see pbl_shared_cbuf_subsampled_client_set_ratio()
 */
void pbl_shared_cbuf_add_subsampled_client(struct pbl_shared_cbuf *buffer,
                                           struct pbl_shared_cbuf_subsampled_client *client,
                                           uint32_t subsample_numerator,
                                           uint32_t subsample_denominator);

/**
 * @brief Remove a subsampling read client.
 *
 * @param buffer Buffer.
 * @param client Client added with pbl_shared_cbuf_add_subsampled_client().
 */
void pbl_shared_cbuf_remove_subsampled_client(struct pbl_shared_cbuf *buffer,
                                              struct pbl_shared_cbuf_subsampled_client *client);

/**
 * @brief Change the subsampling ratio of a client.
 *
 * A different ratio resets the subsampling state, which may introduce jitter on the next read. A
 * numerator greater than 1 introduces jitter in the subsampled stream.
 *
 * @param client Client.
 * @param numerator Numerator of the ratio, at least 1.
 * @param denominator Denominator of the ratio, at least @p numerator.
 */
void pbl_shared_cbuf_subsampled_client_set_ratio(struct pbl_shared_cbuf_subsampled_client *client,
                                                 uint32_t numerator, uint32_t denominator);

/**
 * @brief Read and consume items with subsampling.
 *
 * Discarded items are consumed too.
 *
 * @param buffer Buffer.
 * @param client Client.
 * @param item_size Size of an item in bytes.
 * @param[out] data Destination, at least @p item_size * @p num_items bytes.
 * @param num_items Maximum number of items to return, after subsampling.
 * @return Number of items written to @p data.
 */
size_t pbl_shared_cbuf_read_subsampled(struct pbl_shared_cbuf *buffer,
                                       struct pbl_shared_cbuf_subsampled_client *client,
                                       size_t item_size, void *data, uint16_t num_items);

/** @} */
