# Interrupts

SoC interrupts are bound at build time; there is no runtime handler
registration and the vector table is in flash:

```c
PBL_IRQ_CONNECT(I2C1, 5, pbl_i2c_sf32lb_irq_handler, &s_i2c1, 0);

PBL_IRQ_DIRECT(AON, 0, PBL_IRQ_ZERO_LATENCY) {
  /* handler body */
}
```

A line is named after its `PBL_SOC_IRQN_<line>` define in the SoC's
checked-in `soc_irqs.h`, which also gives its number: `PBL_IRQN(I2C1)` is
`PBL_SOC_IRQN_I2C1`. Binding a line needs no vendor header. The SoC's line
count is `CONFIG_NUM_IRQS`, set in `soc/*/*/Kconfig.defconfig`, and its priority
bits the `arm,num-irq-priority-bits` of the devicetree NVIC.

`PBL_IRQ_CONNECT()` calls `isr(arg)` with whatever type `isr` takes (an
empty `arg` calls `isr()`); `PBL_IRQ_DIRECT()` takes the handler body
instead. Binding a line twice fails to link, and a line without a
`PBL_SOC_IRQN_<line>` fails to compile.

The priority is programmed for every connected line by `pbl_irq_init()` at
boot, so drivers only call `pbl_irq_enable()` and `pbl_irq_disable()`. It
goes from 0, the most urgent interrupt that may call the kernel, to
`PBL_IRQ_PRIO_LOWEST`, and the same number means the same on every SoC
whatever its number of priority bits: the controller levels above 0 are
reserved, and priority 0 is the first level below them.

| Controller level | Taken by |
|---|---|
| 0 | MemManage, BusFault and UsageFault, so that a fault in an ISR or under the kernel lock is taken rather than escalated |
| 1 | Lines flagged `PBL_IRQ_ZERO_LATENCY`, with `CONFIG_KERNEL_ZERO_LATENCY_IRQS` |
| next ones | Priority 0 to `PBL_IRQ_PRIO_LOWEST` |
| least urgent | The kernel's own exceptions, PendSV and SVC |

`pbl_irq_lock()` masks priority 0 and everything less urgent. A line flagged
`PBL_IRQ_ZERO_LATENCY` is never masked by it, must not call the kernel, and
ignores its priority; flagging one without `CONFIG_KERNEL_ZERO_LATENCY_IRQS`
is a build error, as is a priority out of range. Drivers, the system timers
included, default to priority 0, but on the nRF52 to 1, so that the NimBLE
radio at 0 preempts every other interrupt. An enabled line nobody
connected lands in `arch_irq_spurious()`, which asserts.
