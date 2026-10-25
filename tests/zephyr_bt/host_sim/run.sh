#!/bin/bash
# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0
#
# Build and run the Zephyr BR/EDR host simulation; see README.md.
# Usage: tests/zephyr_bt/host_sim/run.sh [build dir, default build-host-sim]
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
P=$(cd "$HERE/../../.." && pwd)
Z=$P/third_party/zephyr_bt
OUT=$(realpath -m "${1:-$P/build-host-sim}")
OBJ=$OUT/obj
mkdir -p "$OBJ"
rm -f "$OBJ"/*.o

# 32-bit, like the watch; the port's kernel is the POSIX one of the unit tests.
CC="clang -m32 -g -O0"
KDEF="-DCONFIG_KERNEL_IDLE_STACK_SIZE=4096 -DCONFIG_KERNEL_NUM_PRIORITIES=5 \
  -DCONFIG_KERNEL_TICK_HZ=1024 -DCONFIG_KERNEL_STACK_ALIGN=8 -DCONFIG_KERNEL_THREAD_TLS_SLOTS=2 \
  -DCONFIG_KERNEL_IRQ_PRIO_KERNEL=255 -DCONFIG_KERNEL_IRQ_PRIO_MAX_SYSCALL=191 \
  -DCONFIG_KERNEL_MUTEX_LOCK_LR=1 -DUNITTEST"
KINC="-I$P/fw -I$P/include -I$P/kernel -I$P/kernel/arch -I$P/kernel/arch/posix/include \
  -I$P/lib/c/include"
ZINC="-I$Z/ctl -I$Z/port/include -I$Z/port/src -I$Z/zephyr/include \
  -I$Z/zephyr/subsys/bluetooth -I$Z/zephyr/subsys/bluetooth/host -I$Z/include"
ZFL="-std=gnu17 -imacros $Z/config/zephyr_bt_config.h -Duuid_to_string=zbt_uuid_to_string \
  -Duuid_from_string=zbt_uuid_from_string -Wno-everything"

MBED="-I$P/third_party/mbedtls/mbedtls/include -I$P/third_party/mbedtls/mbedtls/library \
  -I$P/third_party/mbedtls/include -DMBEDTLS_CONFIG_FILE=\"pebble_mbedtls_config.h\""

cc() { # object source flags...
  local o=$1 src=$2
  shift 2
  $CC "$@" -c "$src" -o "$o" 2> "$o.log" || { echo "FAILED: $src"; cat "$o.log"; exit 1; }
}

# The host side: objects named z_* go to the "worker RAM" sections of host_sim.ld.
for f in $(sed -n '/^set(sources/,/^)/p' "$Z/CMakeLists.txt" | grep -E '\.c' |
           sed 's#\${z}#zephyr#; s#\${host}#zephyr/subsys/bluetooth/host#'); do
  cc "$OBJ/z_$(echo "$f" | tr '/' '_').o" "$Z/$f" $KDEF $ZFL $ZINC $KINC
done
for f in tick sched thread mutex sem msgq poll irq debug arch/posix/arch arch/posix/test; do
  cc "$OBJ/k_$(echo "$f" | tr '/' '_').o" "$P/kernel/$f.c" -std=gnu2x $KDEF $KINC -Wno-everything
done
cc "$OBJ/c_zbt_ctl.o" "$Z/ctl/zbt_ctl.c" -std=gnu2x $KDEF -DCONFIG_BT_CALL_AUDIO -I"$HERE/inc" $KINC \
  -I"$Z/include" -I"$Z/ctl"
cc "$OBJ/c_zbt_ctkd.o" "$Z/ctl/zbt_ctkd.c" -std=gnu2x $MBED
for f in aes cipher cipher_wrap cmac constant_time platform platform_util; do
  cc "$OBJ/t_$f.o" "$P/third_party/mbedtls/mbedtls/library/$f.c" $MBED
done
cc "$OBJ/r_hci_router.o" "$P/subsys/bluetooth/hci_router.c" -std=gnu2x $KDEF $KINC
cc "$OBJ/m_host_sim.o" "$HERE/host_sim.c" -std=gnu2x $KDEF -I"$HERE/inc" $KINC -I"$Z/include" \
  -I"$Z/ctl"
cc "$OBJ/m_fake_ag.o" "$HERE/fake_ag.c" -std=gnu2x
$CC -fuse-ld=bfd -Wl,-T,"$HERE/host_sim.ld" "$OBJ"/*.o -o "$OUT/host_sim" -lpthread

cd "$OUT"
./host_sim > host_sim.log
grep -E '===|passkey|link |phone:|CHECK|SIM' host_sim.log
