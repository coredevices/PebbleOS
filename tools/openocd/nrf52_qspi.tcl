# SPDX-FileCopyrightText: 2026 Core Devices LLC
# SPDX-License-Identifier: Apache-2.0

# Program the QSPI NOR flash of a halted nRF52840 through its QSPI peripheral,
# without running code on the target. The flash takes its quad enable bit in
# SR2 bit 1, written together with SR1 (JESD216 S2B1v1).
#
#   nrf52_qspi_init sck csn io0 io1 io2 io3 ?sckfreq? ?addr32?
#   nrf52_qspi_erase offset size
#   nrf52_qspi_write file offset
#   nrf52_qspi_verify file offset
#   nrf52_qspi_deinit

set _QSPI 0x40029000
set _QSPI_RAM 0x20000000
set _QSPI_CHUNK 0x10000
set _QSPI_BLOCK 0x10000
set _QSPI_SECTOR 0x1000

proc _qspi_min {a b} {
	if {$a < $b} {
		return $a
	}
	return $b
}

proc _qspi_reg {off} {
	global _QSPI
	return [expr {$_QSPI + $off}]
}

proc _qspi_wait_ready {{timeout_ms 5000}} {
	set start [clock milliseconds]
	while {[mrw [_qspi_reg 0x100]] == 0} {
		if {[clock milliseconds] - $start > $timeout_ms} {
			error "qspi: timed out waiting for READY"
		}
	}
	mww [_qspi_reg 0x100] 0
}

# Custom instruction: opcode plus up to 8 data bytes (little-endian words).
proc _qspi_cinstr {opcode {len 0} {dat0 0} {dat1 0}} {
	mww [_qspi_reg 0x638] $dat0
	mww [_qspi_reg 0x63c] $dat1
	mww [_qspi_reg 0x100] 0
	# LIO2 and LIO3 high: they are WP# and HOLD# on most parts
	mww [_qspi_reg 0x634] [expr {$opcode | (($len + 1) << 8) | (1 << 12) | (1 << 13)}]
	_qspi_wait_ready
	return [mrw [_qspi_reg 0x638]]
}

proc _qspi_wait_wip {{timeout_ms 10000}} {
	set start [clock milliseconds]
	while {[expr {[_qspi_cinstr 0x05 1] & 0x1}]} {
		if {[clock milliseconds] - $start > $timeout_ms} {
			error "qspi: flash stayed busy"
		}
	}
}

proc nrf52_qspi_init {sck csn io0 io1 io2 io3 {sckfreq 3} {addr32 1}} {
	mww [_qspi_reg 0x524] $sck
	mww [_qspi_reg 0x528] $csn
	mww [_qspi_reg 0x530] $io0
	mww [_qspi_reg 0x534] $io1
	mww [_qspi_reg 0x538] $io2
	mww [_qspi_reg 0x53c] $io3
	# READ4IO, PP4O, 256-byte pages
	mww [_qspi_reg 0x544] [expr {4 | (2 << 3) | ($addr32 << 6)}]
	# SPI mode 0, SCK delay 5, no DPM
	mww [_qspi_reg 0x600] [expr {($sckfreq << 28) | 5}]
	mww [_qspi_reg 0x500] 1
	mww [_qspi_reg 0x100] 0
	mww [_qspi_reg 0x000] 1
	_qspi_wait_ready

	# Out of deep power-down, then reset whatever the firmware left running
	_qspi_cinstr 0xab
	sleep 1
	_qspi_cinstr 0x66
	_qspi_cinstr 0x99
	sleep 15

	set id [expr {[_qspi_cinstr 0x9f 3] & 0xffffff}]
	echo [format "qspi: JEDEC ID 0x%06x" $id]
	if {$id == 0 || $id == 0xffffff} {
		error "qspi: no flash answering"
	}

	if {$addr32} {
		_qspi_cinstr 0xb7
	}

	# Quad enable: SR2 bit 1, written together with SR1
	set sr1 [expr {[_qspi_cinstr 0x05 1] & 0xff}]
	set sr2 [expr {[_qspi_cinstr 0x35 1] & 0xff}]
	if {!($sr2 & 0x2)} {
		_qspi_cinstr 0x06
		_qspi_cinstr 0x01 2 [expr {$sr1 | (($sr2 | 0x2) << 8)}]
		_qspi_wait_wip
	}
}

# 64 KiB blocks where aligned, 4 KiB sectors elsewhere.
proc nrf52_qspi_erase {offset size} {
	global _QSPI_BLOCK _QSPI_SECTOR
	if {($offset % $_QSPI_SECTOR) != 0 || ($size % $_QSPI_SECTOR) != 0} {
		error "qspi: erase range not 4 KiB aligned"
	}
	set end [expr {$offset + $size}]
	set a $offset
	while {$a < $end} {
		if {($a % $_QSPI_BLOCK) == 0 && $end - $a >= $_QSPI_BLOCK} {
			set len 1
			set step $_QSPI_BLOCK
		} else {
			set len 0
			set step $_QSPI_SECTOR
		}
		mww [_qspi_reg 0x51c] $a
		mww [_qspi_reg 0x520] $len
		mww [_qspi_reg 0x100] 0
		mww [_qspi_reg 0x00c] 1
		_qspi_wait_ready
		_qspi_wait_wip
		incr a $step
	}
}

proc nrf52_qspi_write {file offset} {
	global _QSPI_RAM _QSPI_CHUNK _QSPI_SECTOR
	set size [file size $file]
	set start [clock milliseconds]
	nrf52_qspi_erase $offset [expr {($size + $_QSPI_SECTOR - 1) & ~($_QSPI_SECTOR - 1)}]
	echo [format "qspi: erased in %d ms" [expr {[clock milliseconds] - $start}]]

	for {set pos 0} {$pos < $size} {incr pos $_QSPI_CHUNK} {
		set n [_qspi_min $_QSPI_CHUNK [expr {$size - $pos}]]
		set n4 [expr {($n + 3) & ~3}]
		if {$n4 != $n} {
			mww [expr {$_QSPI_RAM + $n4 - 4}] 0xffffffff
		}
		# Load file bytes [pos, pos + n) at _QSPI_RAM
		load_image $file [expr {$_QSPI_RAM - $pos}] bin $_QSPI_RAM $n
		mww [_qspi_reg 0x510] [expr {$offset + $pos}]
		mww [_qspi_reg 0x514] $_QSPI_RAM
		mww [_qspi_reg 0x518] $n4
		mww [_qspi_reg 0x100] 0
		mww [_qspi_reg 0x008] 1
		_qspi_wait_ready
		_qspi_wait_wip
	}
	echo [format "qspi: wrote %d bytes at 0x%x in %d ms" $size $offset \
		[expr {[clock milliseconds] - $start}]]
}

# Read back through the QSPI peripheral into RAM and compare with the file.
proc nrf52_qspi_verify {file offset} {
	global _QSPI_RAM _QSPI_CHUNK
	set size [file size $file]
	set f [open $file r]
	set tmp [file join [file dirname $file] .qspi_verify.bin]
	set start [clock milliseconds]
	for {set pos 0} {$pos < $size} {incr pos $_QSPI_CHUNK} {
		set n [_qspi_min $_QSPI_CHUNK [expr {$size - $pos}]]
		mww [_qspi_reg 0x504] [expr {$offset + $pos}]
		mww [_qspi_reg 0x508] $_QSPI_RAM
		mww [_qspi_reg 0x50c] [expr {($n + 3) & ~3}]
		mww [_qspi_reg 0x100] 0
		mww [_qspi_reg 0x004] 1
		_qspi_wait_ready
		dump_image $tmp $_QSPI_RAM $n
		set g [open $tmp r]
		set got [read $g]
		close $g
		if {$got ne [read $f $n]} {
			close $f
			file delete $tmp
			error [format "qspi: verify failed in chunk at 0x%x" [expr {$offset + $pos}]]
		}
	}
	close $f
	file delete $tmp
	echo [format "qspi: verified %d bytes in %d ms" $size [expr {[clock milliseconds] - $start}]]
}

proc nrf52_qspi_deinit {} {
	mww [_qspi_reg 0x100] 0
	mww [_qspi_reg 0x010] 1
	mww [_qspi_reg 0x500] 0
}
