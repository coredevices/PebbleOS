/* SPDX-FileCopyrightText: 2024 Google LLC */
/* SPDX-License-Identifier: Apache-2.0 */

#include "fake_GATTAPI_test_vectors.h"
#include "fake_GATTAPI.h"

#include <btutil/bt_uuid.h>

void fake_gatt_put_discovery_complete_event(uint8_t status,
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
