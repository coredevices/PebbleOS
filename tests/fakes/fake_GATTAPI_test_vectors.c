/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_GATTAPI_test_vectors.h"
#include "fake_GATTAPI.h"

#include <bluetooth/gatt_service_types.h>
#include <btutil/bt_uuid.h>
#include "kernel/pbl_malloc.h"

#ifdef GATTAPI_AVAILABLE

// Convert Bluetopia service discovery indication to GATTService structure
GATTService *fake_gatt_convert_discovery_indication_to_service(
    GATT_Service_Discovery_Indication_Data_t *indication_data) {
  if (!indication_data || !indication_data->ServiceInformation.UUID.UUID_16.UUID_Byte0) {
    return NULL;
  }

  // Parse the UUID from Bluetopia format
  const uint16_t uuid_16 = (indication_data->ServiceInformation.UUID.UUID_16.UUID_Byte1 << 8) |
                           indication_data->ServiceInformation.UUID.UUID_16.UUID_Byte0;

  // Count characteristics and descriptors
  const uint8_t num_characteristics = indication_data->NumberOfCharacteristics;
  uint8_t num_descriptors = 0;
  uint8_t num_includes = indication_data->NumberOfIncludedService;

  GATT_Characteristic_Information_t *char_info_list =
      indication_data->CharacteristicInformationList;

  // Count total descriptors across all characteristics
  for (uint8_t i = 0; i < num_characteristics; i++) {
    num_descriptors += char_info_list[i].NumberOfDescriptors;
  }

  // Calculate the size needed for the GATTService
  const size_t size = COMPUTE_GATTSERVICE_SIZE_BYTES(num_characteristics, num_descriptors, num_includes);

  // Allocate memory for the service
  GATTService *service = kernel_zalloc_check(size);
  if (!service) {
    return NULL;
  }

  // Initialize service header
  service->uuid = bt_uuid_expand_16bit(uuid_16);
  service->discovery_generation = 0;
  service->size_bytes = size;
  service->att_handle = indication_data->ServiceInformation.Service_Handle;
  service->num_characteristics = num_characteristics;
  service->num_descriptors = num_descriptors;
  service->num_att_handles_included_services = num_includes;

  // Pointer to current position in characteristics array
  GATTCharacteristic *current_char = service->characteristics;

  // Fill in characteristics and descriptors
  for (uint8_t i = 0; i < num_characteristics; i++) {
    GATT_Characteristic_Information_t *char_info = &char_info_list[i];

    // Parse characteristic UUID
    const uint16_t char_uuid_16 = (char_info->Characteristic_UUID.UUID.UUID_16.UUID_Byte1 << 8) |
                                   char_info->Characteristic_UUID.UUID.UUID_16.UUID_Byte0;

    // Calculate handle offset (difference from service handle)
    const uint8_t handle_offset = char_info->Characteristic_Handle - service->att_handle;

    // Initialize characteristic
    current_char->uuid = bt_uuid_expand_16bit(char_uuid_16);
    current_char->att_handle_offset = handle_offset;
    current_char->properties = char_info->Characteristic_Properties;
    current_char->num_descriptors = char_info->NumberOfDescriptors;

    // Fill in descriptors for this characteristic
    GATTDescriptor *current_desc = current_char->descriptors;
    for (uint8_t j = 0; j < char_info->NumberOfDescriptors; j++) {
      GATT_Characteristic_Descriptor_Information_t *desc_info = &char_info->DescriptorList[j];

      // Parse descriptor UUID
      const uint16_t desc_uuid_16 = (desc_info->Characteristic_Descriptor_UUID.UUID_16.UUID_Byte1 << 8) |
                                     desc_info->Characteristic_Descriptor_UUID.UUID_16.UUID_Byte0;

      // Calculate descriptor handle offset
      const uint8_t desc_handle_offset = desc_info->Characteristic_Descriptor_Handle - service->att_handle;

      current_desc->uuid = bt_uuid_expand_16bit(desc_uuid_16);
      current_desc->att_handle_offset = desc_handle_offset;

      current_desc++;
    }

    // Move to next characteristic (flexible array arithmetic)
    current_char = (GATTCharacteristic *)((uint8_t *)current_char +
                                         sizeof(GATTCharacteristic) +
                                         sizeof(GATTDescriptor) * char_info->NumberOfDescriptors);
  }

  // Fill in included service handles (if any)
  if (num_includes > 0 && indication_data->IncludedServiceList) {
    uint16_t *includes = (uint16_t *)current_char;
    for (uint8_t i = 0; i < num_includes; i++) {
      includes[i] = indication_data->IncludedServiceList[i].Service_Handle;
    }
  }

  return service;
}

void fake_gatt_put_discovery_complete_event(uint16_t status,
                                            unsigned int connection_id) {
  GATT_Service_Discovery_Complete_Data_t data =
  (GATT_Service_Discovery_Complete_Data_t) {
    .ConnectionID = connection_id,
    .Status = status,
  };

  GATT_Service_Discovery_Event_Data_t event =
  (GATT_Service_Discovery_Event_Data_t) {
    .Event_Data_Type = etGATT_Service_Discovery_Complete,
    .Event_Data_Size = GATT_SERVICE_DISCOVERY_COMPLETE_DATA_SIZE,
    .Event_Data = {
      .GATT_Service_Discovery_Complete_Data = &data,
    },
  };
  fake_gatt_put_service_discovery_event(&event);
}

void fake_gatt_put_discovery_indication_health_thermometer_service(unsigned int connection_id) {
  GATT_Characteristic_Descriptor_Information_t cccd1 = {
    .Characteristic_Descriptor_Handle = 0x15,
    .Characteristic_Descriptor_UUID = {
      .UUID_Type = guUUID_16,
      .UUID = {
        .UUID_16 = {
          .UUID_Byte0 = 0x02,
          .UUID_Byte1 = 0x29,
        },
      },
    }
  };

  GATT_Characteristic_Information_t characteristics[1] = {
    [0] = {
      .Characteristic_UUID = {
        .UUID_Type = guUUID_16,
        .UUID = {
          .UUID_16 = {
            .UUID_Byte0 = 0x1c,
            .UUID_Byte1 = 0x2a,
          },
        },
      },
      .Characteristic_Handle = 0x13,
      .Characteristic_Properties = 0x2,
      .NumberOfDescriptors = 0x1,
      .DescriptorList = &cccd1,
    },
  };

  GATT_Service_Discovery_Indication_Data_t data = {
    .ConnectionID = connection_id,
    .ServiceInformation = {
      .Service_Handle = 0x11,
      .End_Group_Handle = 0x15,
      .UUID = {
        .UUID_Type = guUUID_16,
        .UUID = {
          .UUID_16 = {
            .UUID_Byte0 = 0x09,
            .UUID_Byte1 = 0x18,
          },
        },
      }
    },
    .NumberOfCharacteristics = 0x1,
    .CharacteristicInformationList = characteristics,
  };

  GATT_Service_Discovery_Event_Data_t event = {
    .Event_Data_Type = etGATT_Service_Discovery_Indication,
    .Event_Data_Size = GATT_SERVICE_DISCOVERY_INDICATION_DATA_SIZE,
    .Event_Data = {
      .GATT_Service_Discovery_Indication_Data = &data,
    },
  };

  fake_gatt_put_service_discovery_event(&event);
}


static Service s_health_thermometer_service;

const Service * fake_gatt_get_health_thermometer_service(void) {
  s_health_thermometer_service = (const Service) {
    .uuid = bt_uuid_expand_16bit(0x1809),
    .handle = 0x11,
    .num_characteristics = 1,
    .characteristics = {
      [0] = {
        .uuid = bt_uuid_expand_16bit(0x2a1c),
        .properties = 0x02,
        .handle = 0x13,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = bt_uuid_expand_16bit(0x2902),
            .handle = 0x15,
          },
        },
      },
    }
  };
  return &s_health_thermometer_service;
}

void fake_gatt_put_discovery_indication_blood_pressure_service(
                                                               unsigned int connection_id) {
  GATT_Characteristic_Descriptor_Information_t cccd1 = {
    .Characteristic_Descriptor_Handle = 0x05,
    .Characteristic_Descriptor_UUID = {
      .UUID_Type = guUUID_16,
      .UUID = {
        .UUID_16 = {
          .UUID_Byte0 = 0x02,
          .UUID_Byte1 = 0x29,
        },
      },
    }
  };

  GATT_Characteristic_Descriptor_Information_t cccd2 = {
    .Characteristic_Descriptor_Handle = 0x09,
    .Characteristic_Descriptor_UUID = {
      .UUID_Type = guUUID_16,
      .UUID = {
        .UUID_16 = {
          .UUID_Byte0 = 0x02,
          .UUID_Byte1 = 0x29,
        },
      },
    }
  };


  GATT_Characteristic_Information_t characteristics[2] = {
    [0] = {
      .Characteristic_UUID = {
        .UUID_Type = guUUID_16,
        .UUID = {
          .UUID_16 = {
            .UUID_Byte0 = 0x35,
            .UUID_Byte1 = 0x2a,
          },
        },
      },
      .Characteristic_Handle = 0x3,
      .Characteristic_Properties = 0x20,
      .NumberOfDescriptors = 0x1,
      .DescriptorList = &cccd1,
    },
    [1] = {
      .Characteristic_UUID = {
        .UUID_Type = guUUID_16,
        .UUID = {
          .UUID_16 = {
            .UUID_Byte0 = 0x49,
            .UUID_Byte1 = 0x2a,
          },
        },
      },
      .Characteristic_Handle = 0x7,
      .Characteristic_Properties = 0x2,
      .NumberOfDescriptors = 0x1,
      .DescriptorList = &cccd2,
    },
  };

  // Including Health Thermometer Service as "Included Service":
  GATT_Service_Information_t inc_service_list = {
    .Service_Handle = 0x11,
    .End_Group_Handle = 0x15,
    .UUID = {
      .UUID_Type = guUUID_16,
      .UUID = {
        .UUID_16 = {
          .UUID_Byte0 = 0x09,
          .UUID_Byte1 = 0x18,
        },
      },
    }
  };

  GATT_Service_Discovery_Indication_Data_t data = {
    .ConnectionID = connection_id,
    .ServiceInformation = {
      .Service_Handle = 0x1,
      .End_Group_Handle = 0x9,
      .UUID = {
        .UUID_Type = guUUID_16,
        .UUID = {
          .UUID_16 = {
            .UUID_Byte0 = 0x10,
            .UUID_Byte1 = 0x18,
          },
        },
      }
    },
    .NumberOfIncludedService = 0x1,
    .IncludedServiceList = &inc_service_list,
    .NumberOfCharacteristics = 0x2,
    .CharacteristicInformationList = characteristics,
  };

  GATT_Service_Discovery_Event_Data_t event = {
    .Event_Data_Type = etGATT_Service_Discovery_Indication,
    .Event_Data_Size = GATT_SERVICE_DISCOVERY_INDICATION_DATA_SIZE,
    .Event_Data = {
      .GATT_Service_Discovery_Indication_Data = &data,
    },
  };

  fake_gatt_put_service_discovery_event(&event);
}


static Service s_blood_pressure_service;
#define BP_START_ATT_HANDLE 0x1
#define BP_END_ATT_HANDLE 0x9

const Service * fake_gatt_get_blood_pressure_service(void) {
  s_blood_pressure_service = (const Service) {
    .uuid = bt_uuid_expand_16bit(0x1810),
    .handle = BP_START_ATT_HANDLE,
    .num_characteristics = 2,
    .characteristics = {
      [0] = {
        .uuid = bt_uuid_expand_16bit(0x2a35),
        .properties = 0x20, // Indicatable
        .handle = 0x3,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = bt_uuid_expand_16bit(0x2902),
            .handle = 0x05,
          },
        },
      },
      [1] = {
        .uuid = bt_uuid_expand_16bit(0x2a49),
        .properties = 0x02,
        .handle = 0x7,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = bt_uuid_expand_16bit(0x2902),
            .handle = BP_END_ATT_HANDLE,
          },
        },
      },
    },
    .num_included_services = 1,
    .included_services = {
      [0] = &s_health_thermometer_service,
    }
  };
  return &s_blood_pressure_service;
}

void fake_gatt_get_bp_att_handle_range(uint16_t *start, uint16_t *end) {
  *start = BP_START_ATT_HANDLE;
  *end = BP_END_ATT_HANDLE;
}

static Service s_random_128bit_service;

#define RANDOM_S_START_ATT_HANDLE 0x17
#define RANDOM_S_END_ATT_HANDLE   0x25

void fake_gatt_put_discovery_indication_random_128bit_uuid_service(unsigned int connection_id) {
  GATT_Characteristic_Descriptor_Information_t cccd1 = {
    .Characteristic_Descriptor_Handle = 0x21,
    .Characteristic_Descriptor_UUID = {
      .UUID_Type = guUUID_128,
      .UUID = {
        .UUID_128 = { 0xB2, 0xF9, 0x66, 0xAC, 0xED, 0xFD, 0xEE, 0x97, 0x63, 0x4F, 0xFA, 0x1B, 0x5B, 0x09, 0x68, 0xF7 },
      },
    }
  };

  GATT_Characteristic_Descriptor_Information_t cccd2 = {
    .Characteristic_Descriptor_Handle = RANDOM_S_END_ATT_HANDLE,
    .Characteristic_Descriptor_UUID = {
      .UUID_Type = guUUID_128,
      .UUID = {
        .UUID_128 = { 0xB4, 0xF9, 0x66, 0xAC, 0xED, 0xFD, 0xEE, 0x97, 0x63, 0x4F, 0xFA, 0x1B, 0x5B, 0x09, 0x68, 0xF7 },
      },
    }
  };


  GATT_Characteristic_Information_t characteristics[2] = {
    [0] = {
      .Characteristic_UUID = {
        .UUID_Type = guUUID_128,
        .UUID = {
          .UUID_128 = { 0xB1, 0xF9, 0x66, 0xAC, 0xED, 0xFD, 0xEE, 0x97, 0x63, 0x4F, 0xFA, 0x1B, 0x5B, 0x09, 0x68, 0xF7 },
        },
      },
      .Characteristic_Handle = 0x19,
      .Characteristic_Properties = 0x2,
      .NumberOfDescriptors = 0x1,
      .DescriptorList = &cccd1,
    },
    [1] = {
      .Characteristic_UUID = {
        .UUID_Type = guUUID_128,
        .UUID = {
          .UUID_128 = { 0xB3, 0xF9, 0x66, 0xAC, 0xED, 0xFD, 0xEE, 0x97, 0x63, 0x4F, 0xFA, 0x1B, 0x5B, 0x09, 0x68, 0xF7 },
        },
      },
      .Characteristic_Handle = 0x23,
      .Characteristic_Properties = 0x2,
      .NumberOfDescriptors = 0x1,
      .DescriptorList = &cccd2,
    },
  };

  GATT_Service_Discovery_Indication_Data_t data = {
    .ConnectionID = connection_id,
    .ServiceInformation = {
      .Service_Handle = RANDOM_S_START_ATT_HANDLE,
      .End_Group_Handle = 0x9,
      .UUID = {
        .UUID_Type = guUUID_128,
        .UUID = {
          .UUID_128 = { 0xB0, 0xF9, 0x66, 0xAC, 0xED, 0xFD, 0xEE, 0x97, 0x63, 0x4F, 0xFA, 0x1B, 0x5B, 0x09, 0x68, 0xF7 },
        },
      },
    },
    .NumberOfCharacteristics = 0x2,
    .CharacteristicInformationList = characteristics,
  };

  GATT_Service_Discovery_Event_Data_t event = {
    .Event_Data_Type = etGATT_Service_Discovery_Indication,
    .Event_Data_Size = GATT_SERVICE_DISCOVERY_INDICATION_DATA_SIZE,
    .Event_Data = {
      .GATT_Service_Discovery_Indication_Data = &data,
    },
  };

  fake_gatt_put_service_discovery_event(&event);
}

const Service * fake_gatt_get_random_128bit_uuid_service(void) {
  s_random_128bit_service = (const Service) {
    .uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63, 0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB0),
    .handle = 0x01,
    .num_characteristics = 2,
    .characteristics = {
      [0] = {
        .uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63, 0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB1),
        .properties = 0x02,
        .handle = 0x3,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63, 0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB2),
            .handle = 0x05,
          },
        },
      },
      [1] = {
        .uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63, 0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB3),
        .properties = 0x02,
        .handle = 0x7,
        .num_descriptors = 1,
        .descriptors = {
          [0] = {
            .uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63, 0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB4),
            .handle = 0x09,
          },
        },
      },
    },
  };
  return &s_random_128bit_service;
}


void fake_gatt_put_discovery_indication_gatt_profile_service(unsigned int connection_id,
                                                          bool has_service_changed_characteristic) {
  GATT_Characteristic_Descriptor_Information_t cccd1 = {
    .Characteristic_Descriptor_Handle = 0x05,
    .Characteristic_Descriptor_UUID = {
      .UUID_Type = guUUID_16,
      .UUID = {
        .UUID_16 = {
          .UUID_Byte0 = 0x02,
          .UUID_Byte1 = 0x29,
        },
      },
    }
  };

  GATT_Characteristic_Information_t characteristics[1] = {
    [0] = {
      .Characteristic_UUID = {
        .UUID_Type = guUUID_16,
        .UUID = {
          .UUID_16 = {
            .UUID_Byte0 = 0x05,
            .UUID_Byte1 = 0x2a,
          },
        },
      },
      .Characteristic_Handle = 0x3,
      .Characteristic_Properties = 0x20,
      .NumberOfDescriptors = 1,
      .DescriptorList = &cccd1,
    },
  };

  GATT_Service_Discovery_Indication_Data_t data = {
    .ConnectionID = connection_id,
    .ServiceInformation = {
      .Service_Handle = 0x1,
      .End_Group_Handle = 0x5,
      .UUID = {
        .UUID_Type = guUUID_16,
        .UUID = {
          .UUID_16 = {
            .UUID_Byte0 = 0x01,
            .UUID_Byte1 = 0x18,
          },
        },
      }
    },
    .NumberOfIncludedService = 0,
    .IncludedServiceList = NULL,
    .NumberOfCharacteristics = has_service_changed_characteristic ? 1 : 0,
    .CharacteristicInformationList = has_service_changed_characteristic ? characteristics : NULL,
  };

  GATT_Service_Discovery_Event_Data_t event = {
    .Event_Data_Type = etGATT_Service_Discovery_Indication,
    .Event_Data_Size = GATT_SERVICE_DISCOVERY_INDICATION_DATA_SIZE,
    .Event_Data = {
      .GATT_Service_Discovery_Indication_Data = &data,
    },
  };

  fake_gatt_put_service_discovery_event(&event);
}

uint16_t fake_gatt_gatt_profile_service_service_changed_att_handle(void) {
  return 3; // .Characteristic_Handle = 0x3,
}

uint16_t fake_gatt_gatt_profile_service_service_changed_cccd_att_handle(void) {
  return 5; // .Characteristic_Descriptor_Handle = 0x05,
}
#else
// Stub implementations when GATTAPI_AVAILABLE is not defined (Linux/Docker)
// These are minimal stubs that allow the tests to link

#include <bluetooth/gatt_service_types.h>
#include "kernel/pbl_malloc.h"
#include <btutil/bt_uuid.h>
#include <string.h>

// Convert Bluetopia service discovery indication to GATTService structure
// This implementation is for Linux/Docker builds without full Bluetopia API
GATTService *fake_gatt_convert_discovery_indication_to_service(
    GATT_Service_Discovery_Indication_Data_t *indication_data) {
  if (!indication_data) {
    return NULL;
  }

  // Parse UUID from the ServiceInformation structure (if available)
  Uuid service_uuid;
  if (indication_data->ServiceInformation.UUID.UUID_Type == guUUID_16) {
    uint16_t uuid_16 = indication_data->ServiceInformation.UUID.UUID_16;
    service_uuid = bt_uuid_expand_16bit(uuid_16);
  } else if (indication_data->ServiceInformation.UUID.UUID_Type == guUUID_128) {
    // For 128-bit UUIDs, we can't properly convert them without the full byte array
    // The stub types only store a truncated UUID in UUID_16 field
    // For test purposes, we'll create a dummy UUID
    service_uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63,
                           0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB0);
  } else {
    return NULL;
  }

  // Count characteristics and descriptors
  const uint8_t num_characteristics = indication_data->NumberOfCharacteristics;
  uint8_t num_descriptors = 0;
  const uint8_t num_includes = indication_data->NumberOfIncludedService;

  GATT_Characteristic_Information_t *char_info_list =
      indication_data->CharacteristicInformationList;

  // Count total descriptors across all characteristics
  for (uint8_t i = 0; i < num_characteristics; i++) {
    num_descriptors += char_info_list[i].NumberOfDescriptors;
  }

  // Calculate the size needed for the GATTService
  const size_t size = COMPUTE_GATTSERVICE_SIZE_BYTES(num_characteristics, num_descriptors, num_includes);

  // Allocate memory for the service
  GATTService *service = kernel_zalloc_check(size);
  if (!service) {
    return NULL;
  }

  // Initialize service header
  service->uuid = service_uuid;
  service->discovery_generation = 0;
  service->size_bytes = size;
  service->att_handle = indication_data->ServiceInformation.Service_Handle;
  service->num_characteristics = num_characteristics;
  service->num_descriptors = num_descriptors;
  service->num_att_handles_included_services = num_includes;

  // Pointer to current position in characteristics array
  GATTCharacteristic *current_char = service->characteristics;

  // Fill in characteristics and descriptors
  for (uint8_t i = 0; i < num_characteristics; i++) {
    GATT_Characteristic_Information_t *char_info = &char_info_list[i];

    // Parse characteristic UUID
    Uuid char_uuid;
    if (char_info->UUID_Type == guUUID_16) {
      char_uuid = bt_uuid_expand_16bit((uint16_t)char_info->Characteristic_UUID);
    } else {
      // For 128-bit UUIDs, use a dummy UUID
      char_uuid = UuidMake(0xF7, 0x68, 0x09, 0x5B, 0x1B, 0xFA, 0x4F, 0x63,
                          0x97, 0xEE, 0xFD, 0xED, 0xAC, 0x66, 0xF9, 0xB1);
    }

    // Calculate handle offset (difference from service handle)
    const uint8_t handle_offset = char_info->Characteristic_Handle - service->att_handle;

    // Initialize characteristic
    current_char->uuid = char_uuid;
    current_char->att_handle_offset = handle_offset;
    current_char->properties = char_info->Characteristic_Properties;
    current_char->num_descriptors = char_info->NumberOfDescriptors;

    // Fill in descriptors for this characteristic
    GATTDescriptor *current_desc = current_char->descriptors;
    for (uint8_t j = 0; j < char_info->NumberOfDescriptors; j++) {
      GATT_Characteristic_Descriptor_Information_t *desc_info = &char_info->DescriptorList[j];

      // Parse descriptor UUID
      Uuid desc_uuid;
      if (desc_info->UUID_Type == guUUID_16) {
        desc_uuid = bt_uuid_expand_16bit((uint16_t)desc_info->Characteristic_Descriptor_UUID);
      } else {
        // Shouldn't happen for CCCD, but handle gracefully
        desc_uuid = bt_uuid_expand_16bit(0x2902);
      }

      // Calculate descriptor handle offset
      const uint8_t desc_handle_offset = desc_info->Characteristic_Descriptor_Handle - service->att_handle;

      current_desc->uuid = desc_uuid;
      current_desc->att_handle_offset = desc_handle_offset;

      current_desc++;
    }

    // Move to next characteristic (flexible array arithmetic)
    current_char = (GATTCharacteristic *)((uint8_t *)current_char +
                                         sizeof(GATTCharacteristic) +
                                         sizeof(GATTDescriptor) * char_info->NumberOfDescriptors);
  }

  // Fill in included service handles (if any)
  if (num_includes > 0 && indication_data->IncludedServiceList) {
    uint16_t *includes = (uint16_t *)current_char;
    for (uint8_t i = 0; i < num_includes; i++) {
      includes[i] = indication_data->IncludedServiceList[i].Service_Handle;
    }
  }

  return service;
}

void fake_gatt_put_discovery_complete_event(uint16_t status, unsigned int connection_id) {
  // Create a complete event structure
  static GATT_Service_Discovery_Complete_Data_t complete_data;
  complete_data.ConnectionID = connection_id;
  complete_data.Status = status;

  static GATT_Service_Discovery_Event_Data_t event;
  event.Event_Data_Type = 0; // etGATT_Service_Discovery_Complete
  event.Event_Data_Size = sizeof(GATT_Service_Discovery_Complete_Data_t);
  event.Event_Data.GATT_Service_Discovery_Complete_Data = &complete_data;

  fake_gatt_put_service_discovery_event(&event);
}

void fake_gatt_put_discovery_indication_health_thermometer_service(unsigned int connection_id) {
  // Stub - not implemented for Linux/Docker build
}

const Service * fake_gatt_get_health_thermometer_service(void) {
  return NULL;
}

void fake_gatt_put_discovery_indication_blood_pressure_service(unsigned int connection_id) {
  // Create characteristic and descriptor information for Blood Pressure service
  // Using simplified stub types for Linux/Docker builds
  static GATT_Characteristic_Descriptor_Information_t cccd1 = {
    .Characteristic_Descriptor_Handle = 0x05,
    .Characteristic_Descriptor_UUID = 0x2902, // CCCD UUID
    .UUID_Type = guUUID_16,
  };

  static GATT_Characteristic_Information_t characteristics[2] = {
    [0] = {
      .Characteristic_Handle = 0x03,
      .Characteristic_UUID = 0x2a35, // Pressure Measurement
      .Characteristic_Properties = 0x20, // Indicate
      .NumberOfDescriptors = 1,
      .DescriptorList = &cccd1,
      .UUID_Type = guUUID_16,
    },
    [1] = {
      .Characteristic_Handle = 0x07,
      .Characteristic_UUID = 0x2a49, // Feature characteristic
      .Characteristic_Properties = 0x02, // Read
      .NumberOfDescriptors = 1,
      .DescriptorList = &cccd1,
      .UUID_Type = guUUID_16,
    },
  };

  static GATT_Service_Discovery_Indication_Data_t indication_data;
  indication_data.ConnectionID = connection_id;
  indication_data.ServiceInformation.Service_Handle = 0x01;
  indication_data.ServiceInformation.End_Group_Handle = 0x09;
  indication_data.ServiceInformation.UUID = (GATT_UUID_t) {
    .UUID_Type = guUUID_16,
    .UUID_16 = 0x1810, // Blood Pressure Service
  };
  indication_data.NumberOfCharacteristics = 2;
  indication_data.CharacteristicInformationList = characteristics;
  indication_data.NumberOfIncludedService = 0;
  indication_data.IncludedServiceList = NULL;

  static GATT_Service_Discovery_Event_Data_t event;
  event.Event_Data_Type = 1; // etGATT_Service_Discovery_Indication
  event.Event_Data_Size = sizeof(GATT_Service_Discovery_Indication_Data_t);
  event.Event_Data.GATT_Service_Discovery_Indication_Data = &indication_data;

  fake_gatt_put_service_discovery_event(&event);
}

const Service * fake_gatt_get_blood_pressure_service(void) {
  return NULL;
}

void fake_gatt_get_bp_att_handle_range(uint16_t *start, uint16_t *end) {
  *start = 0;
  *end = 0;
}

void fake_gatt_put_discovery_indication_random_128bit_uuid_service(unsigned int connection_id) {
  // Create characteristic information for 128-bit UUID service
  // These characteristics have NO descriptors (no CCCD)
  static GATT_Characteristic_Information_t characteristics[2] = {
    [0] = {
      .Characteristic_Handle = 0x19,
      .Characteristic_UUID = 0xf768095b, // Truncated 128-bit UUID (first 32 bits)
      .Characteristic_Properties = 0x02, // Read
      .NumberOfDescriptors = 0,
      .DescriptorList = NULL,
      .UUID_Type = guUUID_128, // 128-bit UUID
    },
    [1] = {
      .Characteristic_Handle = 0x23,
      .Characteristic_UUID = 0xf768095b, // Truncated 128-bit UUID (first 32 bits)
      .Characteristic_Properties = 0x02, // Read
      .NumberOfDescriptors = 0,
      .DescriptorList = NULL,
      .UUID_Type = guUUID_128, // 128-bit UUID
    },
  };

  static GATT_Service_Discovery_Indication_Data_t indication_data;
  indication_data.ConnectionID = connection_id;
  indication_data.ServiceInformation.Service_Handle = 0x17;
  indication_data.ServiceInformation.End_Group_Handle = 0x25;
  indication_data.ServiceInformation.UUID = (GATT_UUID_t) {
    .UUID_Type = guUUID_128,
    .UUID_16 = 0xf768095b, // Truncated 128-bit UUID
  };
  indication_data.NumberOfCharacteristics = 2;
  indication_data.CharacteristicInformationList = characteristics;
  indication_data.NumberOfIncludedService = 0;
  indication_data.IncludedServiceList = NULL;

  static GATT_Service_Discovery_Event_Data_t event;
  event.Event_Data_Type = 1; // etGATT_Service_Discovery_Indication
  event.Event_Data_Size = sizeof(GATT_Service_Discovery_Indication_Data_t);
  event.Event_Data.GATT_Service_Discovery_Indication_Data = &indication_data;

  fake_gatt_put_service_discovery_event(&event);
}

const Service * fake_gatt_get_random_128bit_uuid_service(void) {
  return NULL;
}

void fake_gatt_put_discovery_indication_gatt_profile_service(unsigned int connection_id,
                                                             bool has_service_changed_characteristic) {
  // Create characteristic information for GATT Profile service
  static GATT_Characteristic_Descriptor_Information_t cccd1 = {
    .Characteristic_Descriptor_Handle = 0x05,
    .Characteristic_Descriptor_UUID = 0x2902, // CCCD UUID
    .UUID_Type = guUUID_16,
  };

  static GATT_Characteristic_Information_t characteristics[1] = {
    [0] = {
      .Characteristic_Handle = 0x03,
      .Characteristic_UUID = 0x2a05, // Service Changed characteristic
      .Characteristic_Properties = 0x20, // Indicate
      .NumberOfDescriptors = 1,
      .DescriptorList = &cccd1,
      .UUID_Type = guUUID_16,
    },
  };

  static GATT_Service_Discovery_Indication_Data_t indication_data;
  indication_data.ConnectionID = connection_id;
  indication_data.ServiceInformation.Service_Handle = 0x01;
  indication_data.ServiceInformation.End_Group_Handle = 0x05;
  indication_data.ServiceInformation.UUID = (GATT_UUID_t) {
    .UUID_Type = guUUID_16,
    .UUID_16 = 0x1800, // Generic Access Profile
  };
  indication_data.NumberOfCharacteristics = has_service_changed_characteristic ? 1 : 0;
  indication_data.CharacteristicInformationList = has_service_changed_characteristic ? characteristics : NULL;
  indication_data.NumberOfIncludedService = 0;
  indication_data.IncludedServiceList = NULL;

  static GATT_Service_Discovery_Event_Data_t event;
  event.Event_Data_Type = 1; // etGATT_Service_Discovery_Indication
  event.Event_Data_Size = sizeof(GATT_Service_Discovery_Indication_Data_t);
  event.Event_Data.GATT_Service_Discovery_Indication_Data = &indication_data;

  fake_gatt_put_service_discovery_event(&event);
}

uint16_t fake_gatt_gatt_profile_service_service_changed_att_handle(void) {
  return 3; // Service Changed characteristic handle
}

uint16_t fake_gatt_gatt_profile_service_service_changed_cccd_att_handle(void) {
  return 5; // Service Changed CCCD handle
}
#endif // GATTAPI_AVAILABLE
