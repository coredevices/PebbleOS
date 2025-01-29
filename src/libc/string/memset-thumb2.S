/*
 * Copyright (c) 2015 ARM Ltd
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
 * 3. The name of the company may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ARM LTD ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ARM LTD BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* modified 2016-06-22:
 *	converted aeabi_memset to normal memset (save r0, different argument order) */

#if __ARM_ARCH_ISA_THUMB >= 2

	.syntax unified
	.text
	.align	2
	.thumb
	.thumb_func
	.global memset
	.type	memset, %function
memset:
	.cfi_startproc
	push	{r0, r4, r5, r6}
	lsls	r4, r0, #30
	beq	.L14
	subs	r4, r2, #1
	cmp	r2, #0
	beq	.L16
	uxtb	r5, r1
	mov	r3, r0
	b	.L4
.L6:
	subs	r2, r4, #1
	cbz	r4, .L16
	mov	r4, r2
.L4:
	strb	r5, [r3], #1
	lsls	r2, r3, #30
	bne	.L6
.L2:
	cmp	r4, #3
	bls	.L11
	uxtb	r5, r1
	orr	r5, r5, r5, lsl #8
	cmp	r4, #15
	orr	r5, r5, r5, lsl #16
	bls	.L9
	add	r2, r3, #16
	mov	r6, r4
.L10:
	subs	r6, r6, #16
	cmp	r6, #15
	str	r5, [r2, #-16]
	str	r5, [r2, #-12]
	str	r5, [r2, #-8]
	str	r5, [r2, #-4]
	add	r2, r2, #16
	bhi	.L10
	sub	r2, r4, #16
	bic	r2, r2, #15
	and	r4, r4, #15
	adds	r2, r2, #16
	cmp	r4, #3
	add	r3, r3, r2
	bls	.L11
.L9:
	mov	r6, r3
	mov	r2, r4
.L12:
	subs	r2, r2, #4
	cmp	r2, #3
	str	r5, [r6], #4
	bhi	.L12
	subs	r2, r4, #4
	bic	r2, r2, #3
	adds	r2, r2, #4
	add	r3, r3, r2
	and	r4, r4, #3
.L11:
	cbz	r4, .L16
	uxtb	r1, r1
	add	r4, r4, r3
.L13:
	strb	r1, [r3], #1
	cmp	r3, r4
	bne	.L13
.L16:
	pop	{r0, r4, r5, r6}
	bx	lr
.L14:
	mov	r4, r2
	mov	r3, r0
	b	.L2
	.cfi_endproc
	.size memset, . - memset

#endif
