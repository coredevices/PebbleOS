# Add-ons

Add-ons are hardware plugged into a connector of a board: display modules,
expansion boards on a header. Each fits the boards with its connectors, and is
plugged in when the build is configured:

```shell
pbl configure --board <board> --addon <addon> <application>
```

The page of each add-on describes it, and the page of each board how to set
up the add-ons that fit it. Its files are in `boards/addons/<addon>/`; see
{ref}`devicetree-addons`.

| Add-on | Description | Connectors |
| --- | --- | --- |
| [`sifli_lcm_1p85`](sifli_lcm_1p85/index.md) | SiFli 1.85" 390x450 AMOLED module, with touch | SiFli quad SPI LCD |

```{toctree}
:hidden:

sifli_lcm_1p85/index.md
```
