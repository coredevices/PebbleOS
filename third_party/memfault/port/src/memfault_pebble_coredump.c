/* SPDX-CopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

//
// Reconstruct a Memfault coredump from the PebbleOS flash-based coredump.
//
// On SF32LB52, RAM does not survive reboot (HAL_PMU_Reboot power-cycles the
// SoC). PebbleOS already captures a full coredump to SPI flash during the NMI
// handler, including all FreeRTOS thread registers and a complete RAM dump.
//
// After reboot, this module reads the PebbleOS coredump from flash, extracts
// the crash registers and per-thread stack memory, and feeds them to the
// Memfault SDK via memfault_coredump_save(). The Memfault packetizer then
// picks up the coredump and sends it via the data logging chunk collector.
//

#include "memfault_pebble_coredump.h"

#include <stddef.h>
#include <string.h>

#include "memfault/components.h"
#include "memfault/panics/coredump.h"
#include "memfault/panics/coredump_impl.h"
#include "memfault/panics/platform/coredump.h"

#include "drivers/flash.h"
#include "flash_region/flash_region.h"
#include "kernel/core_dump.h"
#include "kernel/core_dump_private.h"
#include "kernel/pbl_malloc.h"
#include "system/reboot_reason.h"

// Maximum number of threads we'll process from the PebbleOS coredump
#define MAX_THREADS 20

// Amount of stack to capture per thread (bytes from pxTopOfStack upward).
// Needs to cover the context-switch frame (~80+ bytes with FP) plus enough
// call frames for meaningful backtraces.
#define STACK_BYTES_PER_THREAD 768

// Size of a FreeRTOS TCB to capture per thread.
// tskTaskControlBlock on this platform is 88 bytes (from DWARF info).
// Use 96 to allow for alignment padding.
#define TCB_SIZE 96

// Total budget for cached memory data (stack + TCB per thread + kernel state).
// Each thread needs: stack (768) + TCB (96) + 2x cached block header (12 bytes each) = 888.
// Plus kernel state: ~540 bytes. For 20 threads: 20*888 + 540 ~+ 18.3KB.
#define STACK_DATA_BUFFER_SIZE (MAX_THREADS * (STACK_BYTES_PER_THREAD + TCB_SIZE + 24) + 1024)

// SRAM base address (same across all platforms we support)
#define SRAM_BASE_ADDR 0x20000000

// Flash base address of the coredump that was reconstructed into Memfault
// storage. Set after successful memfault_coredump_save(), used by
// memfault_pebble_coredump_mark_exported() to mark the flash coredump as
// exported once the SDK has finished reading all chunks.
static uint32_t s_exported_coredump_flash_base = CORE_DUMP_FLASH_INVALID_ADDR;

// -----------------------------------------------------------------------
// PebbleOS coredump flash location helpers (mirrors get_bytes_storage_coredump.c)

static uint32_t prv_find_unexported_coredump_flash_base(void) {
  CoreDumpFlashHeader flash_hdr;
  CoreDumpFlashRegionHeader region_hdr;
  uint32_t max_last_used = 0;
  uint32_t last_used_idx = 0;

  flash_read_bytes((uint8_t *)&flash_hdr, CORE_DUMP_FLASH_START, sizeof(flash_hdr));

  if (flash_hdr.magic != CORE_DUMP_FLASH_HDR_MAGIC ||
      flash_hdr.unformatted == CORE_DUMP_ALL_UNFORMATTED) {
    return CORE_DUMP_FLASH_INVALID_ADDR;
  }

  for (unsigned int i = 0; i < CORE_DUMP_MAX_IMAGES; i++) {
    if (flash_hdr.unformatted & (1 << i)) {
      continue;
    }

    uint32_t base_address = core_dump_get_slot_address(i);
    flash_read_bytes((uint8_t *)&region_hdr, base_address, sizeof(region_hdr));

    // Skip coredumps that have already been read.
    if (!region_hdr.unread) {
      continue;
    }

    if (region_hdr.last_used > max_last_used) {
      max_last_used = region_hdr.last_used;
      last_used_idx = i;
    }
  }

  if (max_last_used == 0) {
    return CORE_DUMP_FLASH_INVALID_ADDR;
  }

  return core_dump_get_slot_address(last_used_idx);
}

// -----------------------------------------------------------------------
// Thread info extracted from PebbleOS coredump

typedef struct {
  char name[CORE_DUMP_THREAD_NAME_SIZE];
  uint32_t id;
  bool running;
  uint32_t registers[17];  // r0-r12, sp, lr, pc, xpsr
} ThreadInfo;

// -----------------------------------------------------------------------
// State for the RAM dump region in flash - used to read per-thread stack data

typedef struct {
  uint32_t flash_offset;  // Offset within the coredump image in flash
  uint32_t ram_start;     // Start address of the RAM region (0x20000000)
  uint32_t ram_size;      // Size of the RAM dump (256KB)
} RamDumpInfo;

// Read a chunk of the RAM dump from flash, translating a RAM address to a flash
// offset within the coredump's memory chunk.
static bool prv_read_ram_from_coredump(const RamDumpInfo *ram_dump,
                                       uint32_t image_base,
                                       uint32_t ram_addr, void *buf,
                                       size_t len) {
  if (ram_addr < ram_dump->ram_start ||
      (ram_addr + len) > (ram_dump->ram_start + ram_dump->ram_size)) {
    return false;
  }

  uint32_t offset_in_ram = ram_addr - ram_dump->ram_start;
  uint32_t flash_addr = image_base + ram_dump->flash_offset + offset_in_ram;
  flash_read_bytes((uint8_t *)buf, flash_addr, len);
  return true;
}

// -----------------------------------------------------------------------
// Map PebbleOS RebootReasonCode to Memfault reboot reason

static eMemfaultRebootReason prv_pbl_to_mflt_reason(RebootReasonCode code) {
  switch (code) {
    case RebootReasonCode_Assert:
      return kMfltRebootReason_Assert;
    case RebootReasonCode_HardFault:
      return kMfltRebootReason_HardFault;
    case RebootReasonCode_StackOverflow:
      return kMfltRebootReason_StackOverflow;
    case RebootReasonCode_Watchdog:
      return kMfltRebootReason_SoftwareWatchdog;
    case RebootReasonCode_OutOfMemory:
      return kMfltRebootReason_OutOfMemory;
    case RebootReasonCode_ForcedCoreDump:
      return kMfltRebootReason_ForcedCoreDump;
    case RebootReasonCode_CoreDump:
      return kMfltRebootReason_CoreDump;
    case RebootReasonCode_LauncherPanic:
      return kMfltRebootReason_LauncherPanic;
    case RebootReasonCode_EventQueueFull:
      return kMfltRebootReason_EventQueueFull;
    default:
      return kMfltRebootReason_UnknownError;
  }
}

// -----------------------------------------------------------------------
// Main reconstruction logic

void memfault_pebble_coredump_reconstruct(void) {
  // Check if a Memfault coredump already exists (e.g. from a previous boot)
  size_t existing_size = 0;
  if (memfault_coredump_has_valid_coredump(&existing_size)) {
    MEMFAULT_LOG_INFO("Memfault coredump already present, skipping reconstruction");
    return;
  }

  // Find the most recent PebbleOS coredump that hasn't been exported to Memfault yet
  uint32_t flash_base = prv_find_unexported_coredump_flash_base();
  if (flash_base == CORE_DUMP_FLASH_INVALID_ADDR) {
    return;  // No unexported coredump available
  }

  // Read and validate the image header (static to reduce stack usage)
  uint32_t image_base = flash_base + sizeof(CoreDumpFlashRegionHeader);
  static CoreDumpImageHeader image_hdr;
  flash_read_bytes((uint8_t *)&image_hdr, image_base, sizeof(image_hdr));

  if (image_hdr.magic != CORE_DUMP_MAGIC) {
    MEMFAULT_LOG_ERROR("Invalid coredump magic: 0x%08" PRIx32, image_hdr.magic);
    return;
  }

  MEMFAULT_LOG_INFO("Reconstructing Memfault coredump (build: %.16s, time: %" PRIu32 ")",
                    image_hdr.build_id, image_hdr.time_stamp);

  // Walk the coredump chunks to extract thread info and find the RAM dump.
  // These are static to avoid blowing the KernelMain stack (threads[] alone is
  // ~1.8KB). This function is only called once at boot so static is fine.
  static ThreadInfo threads[MAX_THREADS];
  int num_threads = 0;
  static CoreDumpExtraRegInfo extra_regs;
  memset(&extra_regs, 0, sizeof(extra_regs));
  bool have_extra_regs = false;
  static RamDumpInfo ram_dump;
  memset(&ram_dump, 0, sizeof(ram_dump));
  bool have_ram_dump = false;

  uint32_t chunk_offset = sizeof(CoreDumpImageHeader);
  CoreDumpChunkHeader chunk_hdr;

  while (chunk_offset < CORE_DUMP_MAX_SIZE) {
    flash_read_bytes((uint8_t *)&chunk_hdr, image_base + chunk_offset, sizeof(chunk_hdr));

    if (chunk_hdr.key == CORE_DUMP_CHUNK_KEY_TERMINATOR) {
      break;
    }

    uint32_t data_offset = chunk_offset + sizeof(CoreDumpChunkHeader);

    switch (chunk_hdr.key) {
      case CORE_DUMP_CHUNK_KEY_THREAD: {
        if (num_threads < MAX_THREADS) {
          CoreDumpThreadInfo thread_info;
          flash_read_bytes((uint8_t *)&thread_info, image_base + data_offset,
                           sizeof(thread_info));

          ThreadInfo *t = &threads[num_threads];
          memcpy(t->name, thread_info.name, CORE_DUMP_THREAD_NAME_SIZE);
          t->id = thread_info.id;
          t->running = thread_info.running;
          memcpy(t->registers, thread_info.registers, sizeof(t->registers));
          num_threads++;
        }
        break;
      }

      case CORE_DUMP_CHUNK_KEY_EXTRA_REG: {
        flash_read_bytes((uint8_t *)&extra_regs, image_base + data_offset,
                         sizeof(extra_regs));
        have_extra_regs = true;
        break;
      }

      case CORE_DUMP_CHUNK_KEY_MEMORY: {
        // Read the memory header to find the RAM dump region
        CoreDumpMemoryHeader mem_hdr;
        flash_read_bytes((uint8_t *)&mem_hdr, image_base + data_offset,
                         sizeof(mem_hdr));

        uint32_t mem_size = chunk_hdr.size - sizeof(CoreDumpMemoryHeader);

        // Check if this is the main SRAM region
        if (mem_hdr.start == SRAM_BASE_ADDR && mem_size >= (256 * 1024)) {
          ram_dump.flash_offset = data_offset + sizeof(CoreDumpMemoryHeader);
          ram_dump.ram_start = mem_hdr.start;
          ram_dump.ram_size = mem_size;
          have_ram_dump = true;
        }
        break;
      }

      default:
        break;
    }

    chunk_offset = data_offset + chunk_hdr.size;
  }

  if (num_threads == 0) {
    MEMFAULT_LOG_ERROR("No threads found in coredump");
    return;
  }

  MEMFAULT_LOG_INFO("Found %d threads, ram_dump=%d, extra_regs=%d",
                    num_threads, have_ram_dump, have_extra_regs);

  // Find the crash thread (the one marked as "running") or the ISR thread
  const ThreadInfo *crash_thread = NULL;
  for (int i = 0; i < num_threads; i++) {
    // Prefer the "ISR" synthetic thread if present (crash from interrupt context)
    if (strncmp(threads[i].name, "ISR", CORE_DUMP_THREAD_NAME_SIZE) == 0) {
      crash_thread = &threads[i];
      break;
    }
    if (threads[i].running && crash_thread == NULL) {
      crash_thread = &threads[i];
    }
  }

  if (crash_thread == NULL) {
    // Fall back to first thread
    crash_thread = &threads[0];
  }

  MEMFAULT_LOG_INFO("Crash thread: %.16s (PC=0x%08" PRIx32 " LR=0x%08" PRIx32 ")",
                    crash_thread->name,
                    crash_thread->registers[portCANONICAL_REG_INDEX_PC],
                    crash_thread->registers[portCANONICAL_REG_INDEX_LR]);

  // Build the flat register state expected by the Memfault cloud.
  // The SDK's own fault handler (memfault_fault_handling_arm.c) converts from
  // sMfltRegState to sMfltCortexMRegs before saving. Since we're calling
  // memfault_coredump_save() directly, we must provide sMfltCortexMRegs.
  typedef MEMFAULT_PACKED_STRUCT {
    uint32_t r0;
    uint32_t r1;
    uint32_t r2;
    uint32_t r3;
    uint32_t r4;
    uint32_t r5;
    uint32_t r6;
    uint32_t r7;
    uint32_t r8;
    uint32_t r9;
    uint32_t r10;
    uint32_t r11;
    uint32_t r12;
    uint32_t sp;
    uint32_t lr;
    uint32_t pc;
    uint32_t psr;
    uint32_t msp;
    uint32_t psp;
  } sCortexMRegs;

  uint32_t crash_sp = crash_thread->registers[portCANONICAL_REG_INDEX_SP];

  static sCortexMRegs s_core_regs;
  s_core_regs = (sCortexMRegs){
    .r0 = crash_thread->registers[portCANONICAL_REG_INDEX_R0],
    .r1 = crash_thread->registers[portCANONICAL_REG_INDEX_R1],
    .r2 = crash_thread->registers[portCANONICAL_REG_INDEX_R2],
    .r3 = crash_thread->registers[portCANONICAL_REG_INDEX_R3],
    .r4 = crash_thread->registers[portCANONICAL_REG_INDEX_R4],
    .r5 = crash_thread->registers[portCANONICAL_REG_INDEX_R5],
    .r6 = crash_thread->registers[portCANONICAL_REG_INDEX_R6],
    .r7 = crash_thread->registers[portCANONICAL_REG_INDEX_R7],
    .r8 = crash_thread->registers[portCANONICAL_REG_INDEX_R8],
    .r9 = crash_thread->registers[portCANONICAL_REG_INDEX_R9],
    .r10 = crash_thread->registers[portCANONICAL_REG_INDEX_R10],
    .r11 = crash_thread->registers[portCANONICAL_REG_INDEX_R11],
    .r12 = crash_thread->registers[portCANONICAL_REG_INDEX_R12],
    .sp = crash_sp,
    .lr = crash_thread->registers[portCANONICAL_REG_INDEX_LR],
    .pc = crash_thread->registers[portCANONICAL_REG_INDEX_PC],
    .psr = crash_thread->registers[portCANONICAL_REG_INDEX_XPSR],
    // MSP/PSP from extra regs if available, otherwise use the crash thread's SP
    .msp = have_extra_regs ? extra_regs.msp : 0,
    .psp = have_extra_regs ? extra_regs.psp : crash_sp,
  };

  // Build memory regions for each thread's stack using cached memory blocks.
  //
  // The Memfault SDK uses `region_start` both as the source pointer to read
  // data from AND as the address recorded in the coredump. Since the original
  // RAM is no longer in crash state, we use CachedMemory regions: each has an
  // sMfltCachedBlock header that tells the SDK the real RAM address while the
  // data is read from our buffer.

  #define CACHED_BLOCK_OVERHEAD sizeof(sMfltCachedBlock)
  #define MAX_REGIONS (MAX_THREADS * 2 + 2)

  // Dynamically allocate the staging buffer for cached memory blocks.
  // This is freed after memfault_coredump_save() serializes it all to storage.
  uint8_t *stack_data = kernel_malloc(STACK_DATA_BUFFER_SIZE);
  sMfltCoredumpRegion *regions = kernel_malloc(MAX_REGIONS * sizeof(sMfltCoredumpRegion));
  if (stack_data == NULL || regions == NULL) {
    MEMFAULT_LOG_ERROR("Failed to allocate coredump staging buffers");
    kernel_free(stack_data);
    kernel_free(regions);
    return;
  }

  int num_regions = 0;
  size_t stack_buf_offset = 0;

  // Derive valid address range from the RAM dump we found in the coredump.
  // This adapts to different platforms (SF32LB52 vs nRF52840) automatically.
  const uint32_t ram_end = have_ram_dump
      ? (ram_dump.ram_start + ram_dump.ram_size)
      : (SRAM_BASE_ADDR + 256 * 1024);

  // Helper: add a CachedMemory region from the RAM dump
  #define ADD_CACHED_REGION(addr, size) do { \
    uint32_t _addr = (addr); \
    uint32_t _size = (size); \
    if (_addr >= SRAM_BASE_ADDR && (_addr + _size) <= ram_end && \
        num_regions < MAX_REGIONS) { \
      size_t block_total = CACHED_BLOCK_OVERHEAD + _size; \
      if (stack_buf_offset + block_total <= STACK_DATA_BUFFER_SIZE) { \
        sMfltCachedBlock *_cached = (sMfltCachedBlock *)&stack_data[stack_buf_offset]; \
        _cached->valid_cache = 1; \
        _cached->cached_address = _addr; \
        _cached->blk_size = _size; \
        if (prv_read_ram_from_coredump(&ram_dump, image_base, _addr, \
                                       (uint8_t *)_cached->blk, _size)) { \
          regions[num_regions++] = (sMfltCoredumpRegion){ \
            .type = kMfltCoredumpRegionType_CachedMemory, \
            .region_start = _cached, \
            .region_size = block_total, \
          }; \
          stack_buf_offset += block_total; \
        } \
      } \
    } \
  } while (0)

  if (have_ram_dump) {
    // Include FreeRTOS kernel state so the Memfault cloud can discover all
    // threads by walking the task lists. The cloud needs:
    //  - pxCurrentTCB: identifies the running task
    //  - pxReadyTasksLists[]: linked lists of ready tasks by priority
    //  - xDelayedTaskList1/2: delayed tasks
    //  - xSuspendedTaskList: suspended tasks
    //  - uxCurrentNumberOfTasks: total task count
    //  - pxDelayedTaskList: pointer to active delayed list
    //
    // These are all globals in FreeRTOS tasks.c. We include two regions:
    // one for pxCurrentTCB (in .data), one for the task lists (in .bss).

    extern void *volatile pxCurrentTCB;
    extern uint8_t pxReadyTasksLists;  // Start of FreeRTOS task lists BSS

    // pxCurrentTCB (4 bytes, may be in a different section than the lists)
    ADD_CACHED_REGION((uint32_t)(uintptr_t)&pxCurrentTCB, sizeof(pxCurrentTCB));

    // FreeRTOS kernel state region: covers pxReadyTasksLists, pxDelayedTaskList,
    // pxOverflowDelayedTaskList, uxCurrentNumberOfTasks, xDelayedTaskList1/2,
    // xPendingReadyList, xTasksWaitingTermination, xSuspendedTaskList,
    // xSchedulerRunning, uxTopReadyPriority, xIdleTaskHandle, etc.
    // 512 bytes is generous enough to cover all task-related globals in tasks.c
    // regardless of configMAX_PRIORITIES or config options.
    ADD_CACHED_REGION((uint32_t)(uintptr_t)&pxReadyTasksLists, 512);

    // Add TCB regions for all threads. The Memfault cloud parses FreeRTOS
    // TCBs (found via the task lists) to get thread names and stack pointers.
    // CoreDumpThreadInfo.id == FreeRTOS task handle == TCB address.
    for (int i = 0; i < num_threads; i++) {
      if (strncmp(threads[i].name, "ISR", CORE_DUMP_THREAD_NAME_SIZE) == 0) {
        continue;
      }
      uint32_t tcb_addr = threads[i].id;
      MEMFAULT_LOG_INFO("Thread %.16s: TCB=0x%08" PRIx32 " SP=0x%08" PRIx32,
                        threads[i].name, tcb_addr,
                        threads[i].registers[portCANONICAL_REG_INDEX_SP]);
      ADD_CACHED_REGION(tcb_addr, TCB_SIZE);
    }

    // Add stack regions for non-running threads. We read from pxTopOfStack
    // (first word of the TCB) rather than from the PebbleOS coredump SP.
    // The Memfault cloud unwinds each non-running thread from pxTopOfStack,
    // where FreeRTOS saves the context-switch frame. The running thread's
    // stack is provided via the crash registers + the crash SP region below.
    for (int i = 0; i < num_threads; i++) {
      if (strncmp(threads[i].name, "ISR", CORE_DUMP_THREAD_NAME_SIZE) == 0) {
        continue;
      }

      // Skip the running thread — the cloud uses the crash registers for it,
      // and its pxTopOfStack is stale (last context switch, not crash state).
      if (threads[i].running) {
        // Instead, provide the crash thread's stack from its actual SP
        ADD_CACHED_REGION(crash_sp, STACK_BYTES_PER_THREAD);
        continue;
      }

      uint32_t tcb_addr = threads[i].id;
      if (tcb_addr < SRAM_BASE_ADDR || tcb_addr >= ram_end) {
        continue;
      }

      // Read pxTopOfStack from the TCB (first field, offset 0)
      uint32_t top_of_stack = 0;
      if (!prv_read_ram_from_coredump(&ram_dump, image_base, tcb_addr,
                                       &top_of_stack, sizeof(top_of_stack))) {
        continue;
      }

      if (top_of_stack < SRAM_BASE_ADDR || top_of_stack >= ram_end) {
        MEMFAULT_LOG_WARN("Thread %.16s: pxTopOfStack 0x%08" PRIx32 " out of SRAM",
                          threads[i].name, top_of_stack);
        continue;
      }

      uint32_t available = ram_end - top_of_stack;
      uint32_t to_read = STACK_BYTES_PER_THREAD;
      if (to_read > available) {
        to_read = available;
      }

      ADD_CACHED_REGION(top_of_stack, to_read);
    }
  }

  #undef ADD_CACHED_REGION

  MEMFAULT_LOG_INFO("Memfault coredump: %d regions, %u bytes cached, %d threads",
                    num_regions, (unsigned)stack_buf_offset, num_threads);

  // Get the reboot reason for the trace
  RebootReason reason;
  reboot_reason_get(&reason);
  eMemfaultRebootReason mflt_reason = prv_pbl_to_mflt_reason(reason.code);

  // Save the Memfault coredump to RAM-backed storage
  sMemfaultCoredumpSaveInfo save_info = {
    .regs = &s_core_regs,
    .regs_size = sizeof(s_core_regs),
    .trace_reason = mflt_reason,
    .regions = regions,
    .num_regions = num_regions,
  };

  // Compute the exact serialized size and allocate only what's needed,
  // rather than a fixed 32KB buffer that may fail on low-memory devices.
  size_t save_size = memfault_coredump_get_save_size(&save_info);
  if (save_size == 0) {
    MEMFAULT_LOG_ERROR("Failed to compute coredump save size");
    kernel_free(stack_data);
    kernel_free(regions);
    return;
  }
  memfault_coredump_storage_alloc(save_size);

  bool saved = memfault_coredump_save(&save_info);

  // Free the staging buffers — data has been serialized to coredump storage.
  kernel_free(stack_data);
  kernel_free(regions);

  if (saved) {
    MEMFAULT_LOG_INFO("Memfault coredump saved successfully");

    // Tell the reboot tracking that a coredump was saved so the cloud
    // associates the coredump data with this reboot event. This must be
    // called before memfault_reboot_tracking_collect_reset_info().
    memfault_reboot_tracking_mark_coredump_saved();

    // Don't mark as exported yet — the coredump is in RAM and could be lost
    // if the watch reboots before the chunks are fully read. The export flag
    // is set later in memfault_pebble_coredump_mark_exported(), which is
    // called from memfault_platform_coredump_storage_clear() after the
    // packetizer has finished reading all chunks.
    s_exported_coredump_flash_base = flash_base;
  } else {
    MEMFAULT_LOG_ERROR("Failed to save Memfault coredump");
  }
}

void memfault_pebble_coredump_mark_exported(void) {
  if (s_exported_coredump_flash_base == CORE_DUMP_FLASH_INVALID_ADDR) {
    return;
  }

  // Mark the PebbleOS coredump as read so we don't
  // reconstruct it again on the next reboot.
  core_dump_mark_read(s_exported_coredump_flash_base);
  s_exported_coredump_flash_base = CORE_DUMP_FLASH_INVALID_ADDR;
}
