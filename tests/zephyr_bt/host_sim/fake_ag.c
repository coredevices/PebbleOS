/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

// The phone's side of hands-free for the host simulation: L2CAP signalling,
// an SDP server with the gateway's record, RFCOMM (with credit based flow
// control) and the AT commands of the service level connection, an incoming
// call, its answer and its end. Just enough for what Zephyr's HF sends.

#include "fake_ag.h"

#include <stdio.h>
#include <string.h>

#define RFCOMM_CHANNEL 3
#define PSM_SDP        0x0001
#define PSM_RFCOMM     0x0003

static fake_ag_send_t s_send;
static uint16_t s_handle;
static uint8_t s_addr[6];

static struct {
  uint16_t psm;
  uint16_t local;  // our CID
  uint16_t remote; // the watch's CID
} s_chan[2];

static uint8_t s_dlci;
static char s_line[128];
static size_t s_line_len;
static struct fake_ag_state s_st;

/* ACL and L2CAP */

static void prv_acl(uint16_t cid, const uint8_t *data, size_t len) {
  uint8_t p[300];
  p[0] = 0x02;
  p[1] = s_handle & 0xFF;
  p[2] = ((s_handle >> 8) & 0x0F) | 0x20; // first automatically flushable
  p[3] = (uint8_t)(len + 4);
  p[4] = (uint8_t)((len + 4) >> 8);
  p[5] = (uint8_t)len;
  p[6] = (uint8_t)(len >> 8);
  p[7] = cid & 0xFF;
  p[8] = cid >> 8;
  memcpy(&p[9], data, len);
  s_send(p, 9 + len);
}

static void prv_sig(uint8_t code, uint8_t id, const uint8_t *data, size_t len) {
  uint8_t p[64] = {code, id, (uint8_t)len, 0};
  memcpy(&p[4], data, len);
  prv_acl(0x0001, p, 4 + len);
}

static int prv_chan_by_local(uint16_t cid) {
  for (int i = 0; i < 2; i++) {
    if (s_chan[i].local == cid && s_chan[i].remote != 0) {
      return i;
    }
  }
  return -1;
}

static void prv_signalling(const uint8_t *p, size_t len) {
  const uint8_t code = p[0], id = p[1];
  const uint8_t *d = &p[4];
  switch (code) {
    case 0x02: { // Connection Request
      const uint16_t psm = d[0] | (d[1] << 8), scid = d[2] | (d[3] << 8);
      const int i = psm == PSM_SDP ? 0 : 1;
      s_chan[i].psm = psm;
      s_chan[i].local = 0x0040 + i;
      s_chan[i].remote = scid;
      const uint8_t rsp[] = {
        s_chan[i].local & 0xFF, s_chan[i].local >> 8, scid & 0xFF, scid >> 8, 0, 0, 0, 0
      };
      prv_sig(0x03, id, rsp, sizeof(rsp));
      // Our configuration: MTU 672.
      const uint8_t req[] = {scid & 0xFF, scid >> 8, 0, 0, 0x01, 2, 0xA0, 0x02};
      prv_sig(0x04, 0x80 + i, req, sizeof(req));
      break;
    }
    case 0x04: { // Configuration Request: accept as is
      const uint16_t dcid = d[0] | (d[1] << 8);
      const int i = prv_chan_by_local(dcid);
      if (i < 0) {
        break;
      }
      const uint8_t rsp[] = {s_chan[i].remote & 0xFF, s_chan[i].remote >> 8, 0, 0, 0, 0};
      prv_sig(0x05, id, rsp, sizeof(rsp));
      break;
    }
    case 0x06: { // Disconnection Request
      const uint16_t dcid = d[0] | (d[1] << 8), scid = d[2] | (d[3] << 8);
      const uint8_t rsp[] = {dcid & 0xFF, dcid >> 8, scid & 0xFF, scid >> 8};
      prv_sig(0x07, id, rsp, sizeof(rsp));
      const int i = prv_chan_by_local(dcid);
      if (i >= 0) {
        s_chan[i].remote = 0;
      }
      break;
    }
    case 0x0A: { // Information Request: not supported
      const uint8_t rsp[] = {d[0], d[1], 0x01, 0x00};
      prv_sig(0x0B, id, rsp, sizeof(rsp));
      break;
    }
    default:
      break;
  }
}

/* SDP: one record, the gateway's, whatever is asked */

static void prv_sdp(int chan, const uint8_t *p, size_t len) {
  if (p[0] != 0x06) { // Service Search Attribute Request
    return;
  }
  static const uint8_t attrs[] = {
    0x35, 0x24,                                               // attribute list
    0x09, 0x00, 0x04, 0x35, 0x0C,                             // protocol descriptor list
    0x35, 0x03, 0x19, 0x01, 0x00,                             // L2CAP
    0x35, 0x05, 0x19, 0x00, 0x03, 0x08, RFCOMM_CHANNEL,       // RFCOMM, channel
    0x09, 0x00, 0x09, 0x35, 0x08,                             // profile descriptor list
    0x35, 0x06, 0x19, 0x11, 0x1E, 0x09, 0x01,           0x08, // hands-free 1.8
    0x09, 0x03, 0x11, 0x09, 0x00, 0x00,                       // supported features
  };
  uint8_t rsp[80] = {0x07, p[1], p[2]};
  const size_t lists = 2 + sizeof(attrs);
  const size_t plen = 2 + lists + 1;
  rsp[3] = (uint8_t)(plen >> 8);
  rsp[4] = (uint8_t)plen;
  rsp[5] = (uint8_t)(lists >> 8);
  rsp[6] = (uint8_t)lists;
  rsp[7] = 0x35;
  rsp[8] = sizeof(attrs);
  memcpy(&rsp[9], attrs, sizeof(attrs));
  rsp[9 + sizeof(attrs)] = 0x00; // no continuation
  prv_acl(s_chan[chan].remote, rsp, 10 + sizeof(attrs));
  s_st.sdp_requests++;
}

/* RFCOMM */

static uint8_t prv_crc(const uint8_t *p, size_t len) {
  uint8_t fcs = 0xFF;
  for (size_t i = 0; i < len; i++) {
    fcs ^= p[i];
    for (int b = 0; b < 8; b++) {
      fcs = (fcs & 1) ? (uint8_t)((fcs >> 1) ^ 0xE0) : (uint8_t)(fcs >> 1);
    }
  }
  return 0xFF - fcs;
}

// A frame to the watch. @p response: a response to its command (UA), else our command.
static void prv_rfcomm(uint8_t dlci, uint8_t ctrl, bool response, int credits, const uint8_t *data,
                       size_t len) {
  uint8_t f[200];
  size_t n = 0;
  f[n++] = (uint8_t)((dlci << 2) | (response ? 0x02 : 0x00) | 0x01);
  f[n++] = (uint8_t)(ctrl | (credits >= 0 ? 0x10 : 0x00));
  if (len > 127) {
    f[n++] = (uint8_t)(len << 1);
    f[n++] = (uint8_t)(len >> 7);
  } else {
    f[n++] = (uint8_t)((len << 1) | 1);
  }
  if (credits >= 0) {
    f[n++] = (uint8_t)credits;
  }
  memcpy(&f[n], data, len);
  n += len;
  // UIH: the check covers the address and control fields only.
  f[n] = prv_crc(f, (ctrl & 0xEF) == 0xEF ? 2 : 3);
  prv_acl(s_chan[1].remote, f, n + 1);
}

// The call the phone lists (AT+CLCC): direction, status (-1: none), number.
static struct {
  int dir;
  int status;
  char number[24];
} s_clcc = {.status = -1};

static void prv_clcc(int dir, int status, const char *number) {
  s_clcc.dir = dir;
  s_clcc.status = status;
  snprintf(s_clcc.number, sizeof(s_clcc.number), "%s", number);
}

static void prv_at_send(const char *s) {
  prv_rfcomm(s_dlci, 0xEF, false, -1, (const uint8_t *)s, strlen(s));
}

static void prv_at(const char *cmd) {
  printf("  AG <- %s\n", cmd);
  if (strncmp(cmd, "AT+BRSF=", 8) == 0) {
    // Enhanced call status: the watch may list the calls (AT+CLCC).
    prv_at_send("\r\n+BRSF: 64\r\n");
  } else if (strcmp(cmd, "AT+CIND=?") == 0) {
    prv_at_send(
        "\r\n+CIND: (\"service\",(0,1)),(\"call\",(0,1)),(\"callsetup\",(0-3)),"
        "(\"callheld\",(0-2)),(\"signal\",(0-5)),(\"roam\",(0,1)),(\"battchg\",(0-5))"
        "\r\n");
  } else if (strcmp(cmd, "AT+CIND?") == 0) {
    prv_at_send("\r\n+CIND: 1,0,0,0,4,0,3\r\n");
  } else if (strncmp(cmd, "AT+CMER=", 8) == 0) {
    s_st.slc = true;
  } else if (strcmp(cmd, "AT+CNUM") == 0) {
    // The phone's own number.
    prv_at_send("\r\n+CNUM: ,\"" FAKE_AG_OWN_NUMBER "\",145,,4\r\n");
  } else if (strcmp(cmd, "AT+CLCC") == 0) {
    if (s_clcc.status >= 0) {
      char line[80];
      snprintf(line, sizeof(line), "\r\n+CLCC: 1,%d,%d,0,0,\"%s\",%d\r\n", s_clcc.dir,
               s_clcc.status, s_clcc.number, s_clcc.number[0] ? 129 : 0);
      prv_at_send(line);
    }
  } else if (strcmp(cmd, "ATA") == 0) {
    s_st.answers++;
    s_clcc.status = 0;
    prv_at_send("\r\nOK\r\n");
    prv_at_send("\r\n+CIEV: 2,1\r\n");
    prv_at_send("\r\n+CIEV: 3,0\r\n");
    s_st.call = true;
    // The phone opens the voice link: Connection Request, eSCO.
    uint8_t req[] = {0x04, 0x04, 10, 0, 0, 0, 0, 0, 0, 0x0C, 0x02, 0x5A, 0x02};
    memcpy(&req[3], s_addr, 6);
    s_send(req, sizeof(req));
    return;
  } else if (strcmp(cmd, "AT+CHUP") == 0) {
    s_st.hangups++;
    s_clcc.status = -1;
    prv_at_send("\r\nOK\r\n");
    prv_at_send("\r\n+CIEV: 2,0\r\n");
    s_st.call = false;
    if (s_st.sco) {
      const uint8_t disc[] = {0x04, 0x05, 4, 0, FAKE_AG_SCO_HANDLE & 0xFF, FAKE_AG_SCO_HANDLE >> 8,
                              0x13};
      s_send(disc, sizeof(disc));
      s_st.sco = false;
    }
    return;
  }
  prv_at_send("\r\nOK\r\n");
}

static void prv_mux(const uint8_t *p, size_t len) {
  const uint8_t type = p[0];
  uint8_t rsp[16];
  switch (type) {
    case 0x83: { // PN command: take credit based flow control, give 7 credits
      memcpy(rsp, p, 10);
      rsp[0] = 0x81;
      rsp[3] = 0xE0;
      rsp[9] = 7;
      prv_rfcomm(0, 0xEF, false, -1, rsp, 10);
      break;
    }
    case 0xE3: { // MSC command: answer it, then send ours
      memcpy(rsp, p, len);
      rsp[0] = 0xE1;
      prv_rfcomm(0, 0xEF, false, -1, rsp, len);
      rsp[0] = 0xE3;
      prv_rfcomm(0, 0xEF, false, -1, rsp, len);
      break;
    }
    case 0x93: { // RPN command: echo it
      memcpy(rsp, p, len);
      rsp[0] = 0x91;
      prv_rfcomm(0, 0xEF, false, -1, rsp, len);
      break;
    }
    default:
      break;
  }
}

static void prv_rfcomm_rx(const uint8_t *f, size_t len) {
  const uint8_t dlci = f[0] >> 2;
  const uint8_t ctrl = f[1] & 0xEF;
  const bool pf = f[1] & 0x10;
  size_t hdr = 3, n = f[2] >> 1;
  if (!(f[2] & 1)) {
    n |= f[3] << 7;
    hdr = 4;
  }
  switch (ctrl) {
    case 0x2F: // SABM
      if (dlci != 0) {
        s_dlci = dlci;
      }
      prv_rfcomm(dlci, 0x73, true, -1, NULL, 0); // UA, final
      break;
    case 0x43: // DISC
      prv_rfcomm(dlci, 0x73, true, -1, NULL, 0);
      if (dlci != 0) {
        s_st.slc = false;
      }
      break;
    case 0xEF: { // UIH
      if (dlci == 0) {
        prv_mux(&f[hdr], n);
        break;
      }
      if (pf) {
        hdr++; // credits for us
      }
      if (n == 0) {
        break;
      }
      // One credit back for each frame.
      prv_rfcomm(dlci, 0xEF, false, 1, NULL, 0);
      for (size_t i = 0; i < n; i++) {
        const char c = (char)f[hdr + i];
        if (c == '\r') {
          s_line[s_line_len] = '\0';
          prv_at(s_line);
          s_line_len = 0;
        } else if (c != '\n' && s_line_len < sizeof(s_line) - 1) {
          s_line[s_line_len++] = c;
        }
      }
      break;
    }
    default:
      break;
  }
}

/* Public */

void fake_ag_init(fake_ag_send_t send, uint16_t handle, const uint8_t addr[6]) {
  s_send = send;
  s_handle = handle;
  memcpy(s_addr, addr, 6);
}

void fake_ag_sco_accepted(void) {
  // Synchronous Connection Complete: eSCO, CVSD.
  uint8_t e[] = {
    0x04,
    0x2C,
    17,
    0,
    FAKE_AG_SCO_HANDLE & 0xFF,
    FAKE_AG_SCO_HANDLE >> 8,
    0,
    0,
    0,
    0,
    0,
    0,
    0x02,
    0x0C,
    0x02,
    60,
    0,
    60,
    0,
    0x02
  };
  memcpy(&e[6], s_addr, 6);
  s_send(e, sizeof(e));
  s_st.sco = true;
}

void fake_ag_reset(void) {
  memset(s_chan, 0, sizeof(s_chan));
  s_dlci = 0;
  s_line_len = 0;
  const uint32_t sdp = s_st.sdp_requests, answers = s_st.answers, hangups = s_st.hangups;
  s_st = (struct fake_ag_state){.sdp_requests = sdp, .answers = answers, .hangups = hangups};
  s_clcc.status = -1;
}

void fake_ag_acl(const uint8_t *acl, size_t len) {
  if (len < 8) {
    return;
  }
  const uint8_t *l2 = &acl[4];
  const uint16_t l2len = l2[0] | (l2[1] << 8), cid = l2[2] | (l2[3] << 8);
  if (l2len + 8U > len) {
    return;
  }
  if (cid == 0x0001) {
    prv_signalling(&l2[4], l2len);
    return;
  }
  const int i = prv_chan_by_local(cid);
  if (i == 0) {
    prv_sdp(i, &l2[4], l2len);
  } else if (i == 1) {
    prv_rfcomm_rx(&l2[4], l2len);
  }
}

void fake_ag_incoming_call(const char *number) {
  char clip[64];
  prv_clcc(1, 4, number);
  prv_at_send("\r\n+CIEV: 3,1\r\n");
  prv_at_send("\r\nRING\r\n");
  snprintf(clip, sizeof(clip), "\r\n+CLIP: \"%s\",129\r\n", number);
  prv_at_send(clip);
}

void fake_ag_outgoing_call(void) {
  // Android's virtual call: listed active from the start, with the phone's own number.
  prv_clcc(0, 0, FAKE_AG_OWN_NUMBER);
  // Dialing: the phone opens the voice link at once, as Android does, then the other side
  // rings and answers.
  prv_at_send("\r\n+CIEV: 3,2\r\n");
  uint8_t req[] = {0x04, 0x04, 10, 0, 0, 0, 0, 0, 0, 0x0C, 0x02, 0x5A, 0x02};
  memcpy(&req[3], s_addr, 6);
  s_send(req, sizeof(req));
  prv_at_send("\r\n+CIEV: 3,3\r\n");
  prv_at_send("\r\n+CIEV: 2,1\r\n");
  prv_at_send("\r\n+CIEV: 3,0\r\n");
  s_st.call = true;
}

void fake_ag_outgoing_ringing(void) {
  prv_clcc(0, 3, "0312345678");
  // Dialed on the phone: the voice link at once, then the other side rings, for a while.
  prv_at_send("\r\n+CIEV: 3,2\r\n");
  uint8_t req[] = {0x04, 0x04, 10, 0, 0, 0, 0, 0, 0, 0x0C, 0x02, 0x5A, 0x02};
  memcpy(&req[3], s_addr, 6);
  s_send(req, sizeof(req));
  prv_at_send("\r\n+CIEV: 3,3\r\n");
}

void fake_ag_remote_answer(void) {
  s_clcc.status = 0;
  prv_at_send("\r\n+CIEV: 2,1\r\n");
  prv_at_send("\r\n+CIEV: 3,0\r\n");
  s_st.call = true;
}

void fake_ag_answer_on_phone(void) {
  // Answered on the phone, which opens the voice link to the hands-free device all the same.
  prv_at_send("\r\n+CIEV: 2,1\r\n");
  prv_at_send("\r\n+CIEV: 3,0\r\n");
  s_st.call = true;
  s_clcc.status = 0;
  uint8_t req[] = {0x04, 0x04, 10, 0, 0, 0, 0, 0, 0, 0x0C, 0x02, 0x5A, 0x02};
  memcpy(&req[3], s_addr, 6);
  s_send(req, sizeof(req));
}

void fake_ag_forget_call(void) {
  s_clcc.status = -1;
  s_st.call = false;
}

void fake_ag_close_sco(void) {
  if (s_st.sco) {
    const uint8_t disc[] = {0x04, 0x05, 4, 0, FAKE_AG_SCO_HANDLE & 0xFF, FAKE_AG_SCO_HANDLE >> 8,
                            0x13};
    s_send(disc, sizeof(disc));
    s_st.sco = false;
  }
}

void fake_ag_sco_closed(void) {
  s_st.sco = false;
}

void fake_ag_link_down(void) {
  if (s_st.sco) {
    const uint8_t disc[] = {0x04, 0x05, 4, 0, FAKE_AG_SCO_HANDLE & 0xFF, FAKE_AG_SCO_HANDLE >> 8,
                            0x16};
    s_send(disc, sizeof(disc));
    s_st.sco = false;
  }
}

const struct fake_ag_state *fake_ag_state(void) {
  return &s_st;
}
