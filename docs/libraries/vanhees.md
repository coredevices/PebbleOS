# van Hees

van Hees tells sleep minutes apart with the method of van Hees et al.,
_A Novel, Open Access Method to Assess Sleep Duration Using a Wrist-Worn
Accelerometer_ (PLOS ONE, 2015). It looks at the posture of the wrist rather
than at how much it moves, unlike the sleep threshold of {doc}`kraepelin`.

| | |
| --- | --- |
| Directory | `lib/vanhees` |
| Header | `pbl/vanhees/vanhees.h` |
| Kconfig | `CONFIG_VANHEES`, selected by the code that uses it |
| API | {apidoc}`API reference <vanhees>` |

## Classification

For each 5 second epoch, the median of each axis gives the angle of the Z
axis, perpendicular to the skin, to the horizontal plane. A _posture
change_ is a difference of more than 5 degrees between the angles of two
successive epochs. A minute is a sleep minute when it lies entirely in a
period of at least 5 minutes without posture change.

It only classifies minutes: {doc}`kraepelin` builds sleep sessions out of
them, with the same not-worn detection. The exceptions that depend on the
VMC do not apply: the wrist angle alone decides. Restful sleep is still
found from the filtered VMC, as the method has no notion of it.
