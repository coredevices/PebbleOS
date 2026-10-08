/* SPDX-FileCopyrightText: 2026 Core Devices LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <string.h>

#include <pbl/services/voice/voice.h>
#include <pbl/shell/shell.h>

static VoiceSessionId s_session_id = VOICE_SESSION_ID_INVALID;

static int prv_cmd_start(const struct pbl_shell *sh, size_t argc, char **argv) {
  VoiceEndpointSessionType type = VoiceEndpointSessionTypeDictation;

  if (argc > 1) {
    if (strcmp(argv[1], "nlp") == 0) {
      type = VoiceEndpointSessionTypeNLP;
    } else if (strcmp(argv[1], "dictation") != 0) {
      pbl_shell_error(sh, "invalid session type '%s'", argv[1]);
      return -EINVAL;
    }
  }

  VoiceSessionId session_id = voice_start_dictation(type);
  if (session_id == VOICE_SESSION_ID_INVALID) {
    pbl_shell_error(sh, "a session is already in progress");
    return -EBUSY;
  }

  s_session_id = session_id;
  pbl_shell_print(sh, "session %u started", session_id);
  return 0;
}

static int prv_cmd_stop(const struct pbl_shell *sh, size_t argc, char **argv) {
  voice_stop_dictation(s_session_id);
  return 0;
}

static int prv_cmd_cancel(const struct pbl_shell *sh, size_t argc, char **argv) {
  voice_cancel_dictation(s_session_id);
  return 0;
}

static const struct pbl_shell_cmd sub_voice[] = {
  PBL_SHELL_CMD_ARG(start, NULL, "Start a session [dictation|nlp]", prv_cmd_start, 1, 1),
  PBL_SHELL_CMD(stop, NULL, "Stop recording and wait for the result", prv_cmd_stop),
  PBL_SHELL_CMD(cancel, NULL, "Cancel the session", prv_cmd_cancel),
  PBL_SHELL_SUBCMD_SET_END,
};

PBL_SHELL_CMD_REGISTER(voice, sub_voice, "Voice dictation", NULL);
