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
count is `CONFIG_NUM_IRQS` and its priority bits `CONFIG_NUM_IRQ_PRIO_BITS`,
both set in `soc/*/Kconfig.defconfig`.

`PBL_IRQ_CONNECT()` calls `isr(arg)` with whatever type `isr` takes (an
empty `arg` calls `isr()`); `PBL_IRQ_DIRECT()` takes the handler body
instead. Binding a line twice fails to link, and a line without a
`PBL_SOC_IRQN_<line>` fails to compile.

The priority is in controller units (0 is the most urgent) and is
programmed for every connected line by `pbl_irq_init()` at boot, so drivers
only call `pbl_irq_enable()` and `pbl_irq_disable()`. A priority more urgent
than `PBL_IRQ_PRIO_MAX_SYSCALL` is a build error unless the line is flagged
`PBL_IRQ_ZERO_LATENCY`, in which case the ISR must not call the kernel. An
enabled line nobody connected lands in `arch_irq_spurious()`, which asserts.
