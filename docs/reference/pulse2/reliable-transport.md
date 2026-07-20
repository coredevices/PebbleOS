PULSEv2 Reliable Transport
==========================

The purpose of this document is to describe the procedures for the PULSEv2
reliable transport (TRAIN) to be used in the initial implementations, with
support for only Stop-and-Wait ARQ (Automatic Repeat reQuest). Hopefully,
limiting the scope in this way will make it simpler to implement compared to a
more performant Go-Back-N ARQ. This document is a supplement to the description
of TRAIN in [pulse2.md](pulse2.md).

The PULSEv2 reliable transport (TRAIN) is based on X.25 LAPB, which implements
reliable datagram delivery using a Go-Back-N ARQ (Automatic Repeat reQuest)
procedure. Since a Stop-and-Wait ARQ is equivalent to Go-Back-N with a window
size of 1, LAPB can be readily adapted for Stop-and-Wait ARQ. The description in
this document should hopefully be compatible with an implementation supporting
the full Go-Back-N LAPB procedures when that implementation is configured with a
window size of 1, so that there is a smooth upgrade path which doesn't require
special cases or compatibility breakages.

Documentation conventions
-------------------------

This document relies heavily on the terminology used in [ITU-T Recommendation
X.25](https://www.itu.int/rec/T-REC-X.25-199610-I/en). Readers are also assumed
to have some familiarity with section 2 of that document.

The term "station" is used in this document to mean "DCE or DTE".

Procedures for information transfer
-----------------------------------

There is no support for communicating a busy condition. It is assumed that a
station in a busy condition will silently drop packets, and that the timer
recovery procedure will be sufficient to ensure reliable delivery of the dropped
packets once the busy condition is cleared. An implementation need not support
sending or receiving RNR packets.

Sending I packets
-----------------

All Information transfer packets must be sent with the Poll bit set to 1. The
procedures from X.25 §2.4.5.1 apply otherwise.

Receiving an I packet
---------------------

When the DCE receives a valid I packet whose send sequence number N(S) is equal
to the DCE receive state variable V(R), the DCE will accept the information
fields of this packet, increment by one its receive state variable V(R), and
transmit an RR response packet with N(R) equal to the value of the DCE receive
state variable V(R). If the received I packet has the Poll bit set to 1, the
transmitted RR packet must be a response packet with Final bit set to 1.
Otherwise the transmitted RR packet should have the Final bit set to 0.

Reception of out-of-sequence I packets
--------------------------------------

Since the DTE should not have more than one packet in-flight at once, an
out-of-sequence I packet would be due to a retransmit: RR response for the most
recently received I packet got lost, so the DTE re-sent the I packet. Discard
the information fields of the packet and send an RR packet with N(R)=V(R).

Receiving acknowledgement
-------------------------

When correctly receiving a RR packet, the DCE will consider this packet as an
acknowledgement of the most recently-sent I packet if N(S) of the most
recently-sent I packet is equal to the received N(R)-1. The DCE will stop timer
T1 when it correctly receives an acknowledgement of the most recently-sent I
packet.

Since all I packets are sent with P=1, the receiving station is obligated to
respond with a supervisory packet. Therefore it is unnecessary to support
acknowledgements embedded in I packets.

Receiving an REJ packet
-----------------------

Since only one I packet may be in-flight at once, the REJ packet is due to the
RR acknowledgement from the DTE getting lost and the DCE retransmitting the I
packet. Treat it like an RR.

Waiting acknowledgement
-----------------------

The DCE maintains an internal transmission attempt variable which is set to 0
when the transport NCP signals a This-Layer-Up event, and when the DCE correctly
receives an acknowledgement of a sent I packet.

If Timer T1 runs out waiting for the acknowledgement from the DTE for an I
packet transmitted, the DCE will add one to its transmission attempt variable,
restart Timer T1 and retransmit the unacknowledged I packet.

If the transmission attempt variable is equal to N2 (a system parameter), the
DCE will initiate a restart of the transport link.

