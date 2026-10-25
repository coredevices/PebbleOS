# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

set(z zephyr)
set(host ${z}/subsys/bluetooth/host)

set(sources
  ${z}/lib/net_buf/buf.c
  ${z}/lib/net_buf/buf_simple.c
  ${z}/subsys/bluetooth/common/addr.c
  ${z}/subsys/bluetooth/common/bt_str.c
  ${z}/subsys/bluetooth/common/dummy.c
  ${z}/subsys/bluetooth/common/hci_lockstep.c
  ${z}/subsys/bluetooth/common/hci_pkt.c
  ${host}/addr.c
  ${host}/att.c
  ${host}/buf.c
  ${host}/conn.c
  ${host}/data.c
  ${host}/gatt.c
  ${host}/hci_common.c
  ${host}/hci_core.c
  ${host}/id.c
  ${host}/l2cap.c
  ${host}/smp_null.c
  ${host}/uuid.c
  ${host}/classic/at.c
  ${host}/classic/br.c
  ${host}/classic/conn_br.c
  ${host}/classic/goep.c
  ${host}/classic/hfp_hf.c
  ${host}/classic/keys_br.c
  ${host}/classic/l2cap_br.c
  ${host}/classic/obex.c
  ${host}/classic/pbap.c
  ${host}/classic/rfcomm.c
  ${host}/classic/sco.c
  ${host}/classic/sdp.c
  ${host}/classic/ssp.c
  port/src/zbt_hci.c
  port/src/zbt_hfp.c
  port/src/zbt_kernel.c
  port/src/zbt_pbap.c
  port/src/zbt_power.c
  port/src/zbt_util.c
)

pbl_include_directories(include)

pbl_library_named(zephyr_bt)
pbl_library_sources(${sources})
# The port's headers first: they replace Zephyr's kernel, logging and device model.
pbl_library_include_directories(
  port/include
  port/src
  ctl
  ${z}/include
  ${z}/subsys/bluetooth
  ${host}
)
pbl_library_compile_options(
  -std=gnu17
  -imacros ${CMAKE_CURRENT_SOURCE_DIR}/config/zephyr_bt_config.h
  # Zephyr's own warning set does not include these.
  -Wno-sign-compare
  -Wno-type-limits
  -Wno-pointer-sign
  # Left by LOG_DBG() compiled out (CONFIG_LOG=n), in the ERTM paths PBAP brings in.
  -Wno-unused-but-set-variable
)

# PebbleOS has its own uuid_to_string() (lib/util/uuid.c); keep Zephyr's apart.
pbl_library_compile_definitions(
  uuid_to_string=zbt_uuid_to_string
  uuid_from_string=zbt_uuid_from_string
)

# Starting and stopping the host; kept out of the worker RAM (linker/zbt_ram.ld
# takes this directory's zephyr_bt objects only).
pbl_library_named(zephyr_bt_ctl)
pbl_library_sources(ctl/zbt_ctl.c ctl/zbt_ctkd.c ctl/zbt_call_log.c ctl/zbt_book.c)
pbl_library_include_directories(ctl)

pbl_linker_sources(rom-start linker/zbt_rom.ld)
pbl_linker_sources(ram-sections linker/zbt_ram.ld)
