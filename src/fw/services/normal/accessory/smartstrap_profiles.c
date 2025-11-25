/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "process_management/process_manager.h"
#include "services/normal/accessory/smartstrap_connection.h"
#include "services/normal/accessory/smartstrap_link_control.h"
#include "services/normal/accessory/smartstrap_profiles.h"
#include "services/normal/accessory/smartstrap_state.h"
#include "system/logging.h"
#include "system/passert.h"
#include "util/size.h"

#define NUM_PROFILES() ARRAY_LENGTH(s_profile_info_functions)
//! Iterates through the list of profiles and provides the info for each by calling the function
#define FOREACH_PROFILE_INFO(info) \
  for (unsigned int i = 0; i < NUM_PROFILES(); i++) \
    if ((info = s_profile_info_functions[i]()) != NULL)

static const SmartstrapProfileGetInfoFunc s_profile_info_functions[] = {
#define REGISTER_SMARTSTRAP_PROFILE(f) f,
#include "services/normal/accessory/smartstrap_profile_registry.def"
#undef REGISTER_SMARTSTRAP_PROFILE
};
// every profile except for SmartstrapProfileInvalid should be registered
_Static_assert(NUM_PROFILES() == (NumSmartstrapProfiles - 1),
               "The number of registered profiles doesn't match the SmartstrapProfiles enum");


static const SmartstrapProfileInfo *prv_get_info_by_profile(SmartstrapProfile profile) {
  const SmartstrapProfileInfo *info;
  FOREACH_PROFILE_INFO(info) {
    if (info->profile == profile) {
      return info;
    }
  }
  return NULL;
}

void smartstrap_profiles_init(void) {
  // call the init handler for all the profiles
  const SmartstrapProfileInfo *info;
  FOREACH_PROFILE_INFO(info) {
    if (info->init) {
      info->init();
    }
  }
}

void smartstrap_profiles_handle_connection_event(bool connected) {
  // send the event to the applicable profiles
  PBL_LOG(LOG_LEVEL_DEBUG, "Dispatching smartstrap connection event (connected=%d)", connected);
  const SmartstrapProfileInfo *info;
  FOREACH_PROFILE_INFO(info) {
    if (info->connected) {
      info->connected(connected);
    }
  }
  if (connected) {
    smartstrap_connection_got_valid_data();
  }
}

static const SmartstrapProfileInfo *prv_get_info_by_service_id(uint16_t service_id) {
  // find the profile with the lowest minimum service_id which is <= the specified one
  const SmartstrapProfileInfo *match_info = NULL;
  const SmartstrapProfileInfo *info;
  FOREACH_PROFILE_INFO(info) {
    if ((info->max_services > 0) && (info->min_service_id <= service_id) &&
        (!match_info || (info->min_service_id > match_info->min_service_id))) {
      // this is either the first match or a better match
      match_info = info;
    }
  }
  return match_info;
}

SmartstrapResult smartstrap_profiles_handle_request(const SmartstrapRequest *request) {
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  // make sure this request is able to be fulfilled
  smartstrap_state_lock();
  const SmartstrapProfileInfo *info = prv_get_info_by_service_id(request->service_id);
  PBL_ASSERTN(info && info->send);
  if (!smartstrap_connection_has_subscriber() || !smartstrap_is_connected() ||
      !smartstrap_link_control_is_profile_supported(info->profile)) {
    smartstrap_state_unlock();
    return SmartstrapResultServiceUnavailable;
  }

  SmartstrapResult result = info->send(request);
  smartstrap_state_unlock();
  return result;
}

void smartstrap_profiles_handle_read(bool success, SmartstrapProfile profile, uint32_t length) {
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  if (!success) {
    // this is a timeout
    PBL_LOG(LOG_LEVEL_WARNING, "Timed-out waiting for a response from the smartstrap");
  }

  // dispatch the read based on the profile
  const SmartstrapProfileInfo *info = prv_get_info_by_profile(profile);
  PBL_ASSERTN(info && info->read_complete);
  smartstrap_state_lock();
  if (info->read_complete(success, length)) {
    smartstrap_connection_got_valid_data();
  }
  smartstrap_state_unlock();
  // If we are connected, kick the connection monitor right away. Otherwise, just let it wake up
  // itself based on its own timer. This prevents us spamming the smartstrap with connection
  // requests.
  if (smartstrap_is_connected()) {
    // send the next message
    smartstrap_connection_kick_monitor();
  }
}

void smartstrap_profiles_handle_read_aborted(SmartstrapProfile profile) {
  PBL_ASSERTN(portIN_CRITICAL());
  // dispatch the aborted read based on the profile
  const SmartstrapProfileInfo *info = prv_get_info_by_profile(profile);
  if (info && info->read_aborted) {
    info->read_aborted();
  }
}

void smartstrap_profiles_handle_notification(bool success, SmartstrapProfile profile) {
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  if (!success) {
    PBL_LOG(LOG_LEVEL_WARNING, "Dropped notification due to a timeout on the context frame.");
    return;
  } else if (!smartstrap_is_connected()) {
    PBL_LOG(LOG_LEVEL_WARNING, "Dropped notification due to not being connected.");
    return;
  }

  // dispatch the notification based on the profile
  const SmartstrapProfileInfo *info = prv_get_info_by_profile(profile);
  if (info && info->notify) {
    smartstrap_state_lock();
    info->notify();
    smartstrap_state_unlock();
  } else {
    PBL_LOG(LOG_LEVEL_WARNING, "Dropped notification for unsupported profile: %d", profile);
  }
}

bool smartstrap_profiles_send_control(void) {
  PBL_ASSERT_TASK(PebbleTask_KernelBackground);
  bool did_send = false;
  smartstrap_state_lock();
  const SmartstrapProfileInfo *info;
  FOREACH_PROFILE_INFO(info) {
    if (info->control && info->control()) {
      did_send = true;
      break;
    }
  }
  smartstrap_state_unlock();
  return did_send;
}


unsigned int smartstrap_profiles_get_max_services(void) {
  unsigned int max = 0;
  const SmartstrapProfileInfo *info;
  FOREACH_PROFILE_INFO(info) {
    max += info->max_services;
  }
  return max;
}
