# Add-ons

Add-ons are hardware plugged into a connector of a board: display modules,
expansion boards on a header. Each fits the boards with its connectors, and is
plugged in when the build is configured:

```shell
pbl configure --board <board> --addon <addon> <application>
```

The page of each add-on describes it, what it supports and how to set it up
on each board. Its files are in `boards/addons/<addon>/`; see
{ref}`devicetree-addons`.
