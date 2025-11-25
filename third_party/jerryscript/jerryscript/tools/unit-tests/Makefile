# SPDX-FileCopyrightText: 2016 Samsung Electronics Co., Ltd.
# SPDX-FileCopyrightText: 2016 University of Szeged
# SPDX-License-Identifier: Apache-2.0

CC=gcc
LDFLAGS=-lm

GENS=gen-test-libm

.PHONY: build
build: $(GENS)

.PHONY: clean
clean:
	rm -f $(GENS)

gen-test-libm: gen-test-libm.c
	$(CC) $< -o $@ $(LDFLAGS)
