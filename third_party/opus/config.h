/*
 * Opus configuration for embedded systems without floating point
 * This file defines the compile-time configuration for Opus codec
 */

#ifndef OPUS_CONFIG_H
#define OPUS_CONFIG_H

/* Basic configuration */
#define OPUS_BUILD 1
#define FIXED_POINT 1
#define DISABLE_FLOAT_API 1
#define USE_ALLOCA 1
#define HAVE_STDINT_H 1
#define VAR_ARRAYS 1

/* Platform capabilities */
#define HAVE_LRINT 1
#define HAVE_LRINTF 1

/* Disable optional features to save space */
#define OPUS_EXPORT 
#define OPUS_GNUC_PREREQ(maj, min) 0

/* ARM optimizations if available */
#ifdef __ARM_ARCH
#define OPUS_ARM_ASM 1
#define OPUS_ARM_INLINE_ASM 1  
#define OPUS_ARM_INLINE_EDSP 1
#define OPUS_ARM_MAY_HAVE_NEON 1
#define OPUS_ARM_MAY_HAVE_MEDIA 1
#define OPUS_ARM_MAY_HAVE_EDSP 1
#endif

/* Memory allocation */
#define OVERRIDE_OPUS_ALLOC 1
#define OVERRIDE_OPUS_FREE 1

/* Stack allocation preferences */
#define NONTHREADSAFE_PSEUDOSTACK 1
#define OPUS_ALLOC_STACK 1

/* Codec complexity settings for embedded */
#define OPUS_DEFAULT_COMPLEXITY 1
#define OPUS_MAX_COMPLEXITY 3

#endif /* OPUS_CONFIG_H */
