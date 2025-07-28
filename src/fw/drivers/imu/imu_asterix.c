#include "board/board.h"
#include "board/boards/board_asterix.h"
#include "drivers/accel.h"
#include "drivers/exti.h"
#include "drivers/i2c_definitions.h"
#include "drivers/i2c.h"
#include "drivers/imu.h"
#include "kernel/util/delay.h"
#include "system/logging.h"

// MMC5603NJ magnetometer registers
#define MMC5603_PRODUCT_ID 0x39
#define MMC5603_PRODUCT_ID_VALUE 0x10
#define MMC5603_CONTROL2 0x1D

// BMP390 pressure sensor registers
#define BMP390_CHIP_ID 0x00
#define BMP390_CHIP_ID_VALUE 0x60
#define BMP390_PWR_CTRL 0x1B

// LSM6DSO accelerometer/gyroscope registers
#define LSM6D_FUNC_CFG_ACCESS 0x01
#define LSM6D_WHO_AM_I 0x0F
#define LSM6D_WHO_AM_I_VALUE 0x6C

// Control registers
#define LSM6D_CTRL1_XL 0x10        // Accelerometer control
#define LSM6D_CTRL2_G 0x11         // Gyroscope control
#define LSM6D_CTRL3_C 0x12         // Common control
#define LSM6D_CTRL4_C 0x13         // Common control
#define LSM6D_CTRL5_C 0x14         // Common control
#define LSM6D_CTRL6_C 0x15         // Accelerometer control
#define LSM6D_CTRL7_G 0x16         // Gyroscope control
#define LSM6D_CTRL8_XL 0x17        // Accelerometer control
#define LSM6D_CTRL9_XL 0x18        // Accelerometer control
#define LSM6D_CTRL10_C 0x19        // Common control

// Data output registers
#define LSM6D_OUTX_L_XL 0x28       // Accelerometer X-axis low byte
#define LSM6D_OUTX_H_XL 0x29       // Accelerometer X-axis high byte
#define LSM6D_OUTY_L_XL 0x2A       // Accelerometer Y-axis low byte
#define LSM6D_OUTY_H_XL 0x2B       // Accelerometer Y-axis high byte
#define LSM6D_OUTZ_L_XL 0x2C       // Accelerometer Z-axis low byte
#define LSM6D_OUTZ_H_XL 0x2D       // Accelerometer Z-axis high byte

// Status registers
#define LSM6D_STATUS_REG 0x1E         // Status register
#define LSM6D_STATUS_XLDA 0x01         // Accelerometer data available

// Interrupt registers
#define LSM6D_INT1_CTRL 0x0D       // INT1 pin control
#define LSM6D_INT2_CTRL 0x0E       // INT2 pin control
#define LSM6D_MD1_CFG 0x5E         // Functions routing on INT1
#define LSM6D_MD2_CFG 0x5F         // Functions routing on INT2

// Wake-up and activity/inactivity
#define LSM6D_WAKE_UP_THS 0x5B     // Wake-up threshold
#define LSM6D_WAKE_UP_DUR 0x5C     // Wake-up duration
#define LSM6D_FREE_FALL 0x5D       // Free-fall configuration
#define LSM6D_TAP_CFG 0x58         // General tap configuration

// Source registers for interrupt status
#define LSM6D_WAKE_UP_SRC 0x1B     // Wake-up source register
#define LSM6D_TAP_SRC 0x1C         // Tap source register
#define LSM6D_ALL_INT_SRC 0x1A     // All interrupt sources register

// Tap detection
#define LSM6D_TAP_CFG0 0x56        // Tap configuration
#define LSM6D_TAP_CFG1 0x57        // Tap configuration
#define LSM6D_TAP_CFG2 0x58        // Tap configuration
#define LSM6D_TAP_THS_6D 0x59      // Tap threshold and 6D configuration
#define LSM6D_TAP_DUR 0x5A         // Tap duration

// Control register bit definitions
#define LSM6D_CTRL4_C_SLEEP_G 0x40

// Accelerometer ODR (Output Data Rate) settings for CTRL1_XL
#define LSM6D_XL_ODR_OFF 0x00
#define LSM6D_XL_ODR_12_5_HZ 0x10
#define LSM6D_XL_ODR_26_HZ 0x20
#define LSM6D_XL_ODR_52_HZ 0x30
#define LSM6D_XL_ODR_104_HZ 0x40
#define LSM6D_XL_ODR_208_HZ 0x50
#define LSM6D_XL_ODR_417_HZ 0x60
#define LSM6D_XL_ODR_833_HZ 0x70
#define LSM6D_XL_ODR_1667_HZ 0x80
#define LSM6D_XL_ODR_3333_HZ 0x90
#define LSM6D_XL_ODR_6667_HZ 0xA0

// Accelerometer full-scale settings
#define LSM6D_XL_FS_2G 0x00
#define LSM6D_XL_FS_4G 0x08
#define LSM6D_XL_FS_8G 0x0C
#define LSM6D_XL_FS_16G 0x04

// Driver state
static struct {
  uint32_t sampling_interval_us;
  uint32_t num_samples;
  bool shake_detection_enabled;
  bool double_tap_detection_enabled;
  bool initialized;
  uint8_t current_odr_setting;
  uint8_t current_fs_setting;
} s_lsm6d_state = {
  .sampling_interval_us = 10000, // 10ms default (100 Hz)
  .num_samples = 0,
  .shake_detection_enabled = false,
  .double_tap_detection_enabled = false,
  .initialized = false,
  .current_odr_setting = LSM6D_XL_ODR_104_HZ,
  .current_fs_setting = LSM6D_XL_FS_2G,
};

// Accelerometer driver info
const AccelDriverInfo ACCEL_DRIVER_INFO = {
  .sample_interval_max = 80000,        // 12.5 Hz (80ms)
  .sample_interval_low_power = 38461,  // 26 Hz (~38.5ms) - good for low power
  .sample_interval_ui = 19230,         // 52 Hz (~19.2ms) - good for UI
  .sample_interval_game = 4807,        // 208 Hz (~4.8ms) - good for games
  .sample_interval_min = 600,          // 1667 Hz (~0.6ms) - fastest supported
};

// Interrupt state
static bool s_data_interrupt_enabled = false;
static bool s_exti_configured = false;
static volatile bool s_interrupt_pending = false;

// Forward declaration (circular dependency with prv_configure_exti_once)
static void prv_accel_interrupt_handler(bool *should_context_switch);

static bool prv_read_register(const struct I2CSlavePort *i2c, uint8_t register_address, uint8_t *result) {
  i2c_use(i2c);
  bool rv = i2c_write_block(i2c, 1, &register_address);
  if (rv)
    rv = i2c_read_block(i2c, 1, result);
  i2c_release(i2c);
  return rv;
}

static bool prv_write_register(const struct I2CSlavePort *i2c, uint8_t register_address, uint8_t datum) {
  i2c_use(i2c);
  uint8_t d[2] = { register_address, datum };
  bool rv = i2c_write_block(i2c, 2, d);
  i2c_release(i2c);
  return rv;
}

static bool prv_read_multiple_registers(const struct I2CSlavePort *i2c, uint8_t register_address, uint8_t *data, uint8_t length) {
  i2c_use(i2c);
  bool rv = i2c_write_block(i2c, 1, &register_address);
  if (rv)
    rv = i2c_read_block(i2c, length, data);
  i2c_release(i2c);
  return rv;
}

static uint32_t prv_odr_setting_to_interval_us(uint8_t odr_setting) {
  switch (odr_setting) {
    case LSM6D_XL_ODR_12_5_HZ: return 80000;    // 12.5 Hz -> 80ms
    case LSM6D_XL_ODR_26_HZ:   return 38461;    // 26 Hz -> ~38.5ms
    case LSM6D_XL_ODR_52_HZ:   return 19230;    // 52 Hz -> ~19.2ms
    case LSM6D_XL_ODR_104_HZ:  return 9615;     // 104 Hz -> ~9.6ms
    case LSM6D_XL_ODR_208_HZ:  return 4807;     // 208 Hz -> ~4.8ms
    case LSM6D_XL_ODR_417_HZ:  return 2398;     // 417 Hz -> ~2.4ms
    case LSM6D_XL_ODR_833_HZ:  return 1200;     // 833 Hz -> ~1.2ms
    case LSM6D_XL_ODR_1667_HZ: return 600;      // 1667 Hz -> ~0.6ms
    default: return 9615; // Default to ~100 Hz
  }
}

static uint8_t prv_interval_us_to_odr_setting(uint32_t interval_us) {
  if (interval_us >= 80000) return LSM6D_XL_ODR_12_5_HZ;
  if (interval_us >= 38461) return LSM6D_XL_ODR_26_HZ;
  if (interval_us >= 19230) return LSM6D_XL_ODR_52_HZ;
  if (interval_us >= 9615)  return LSM6D_XL_ODR_104_HZ;
  if (interval_us >= 4807)  return LSM6D_XL_ODR_208_HZ;
  if (interval_us >= 2398)  return LSM6D_XL_ODR_417_HZ;
  if (interval_us >= 1200)  return LSM6D_XL_ODR_833_HZ;
  return LSM6D_XL_ODR_1667_HZ;
}

static bool prv_lsm6d_configure(void) {
  uint8_t ctrl_reg;
  
  // Configure accelerometer: set ODR and full-scale
  ctrl_reg = s_lsm6d_state.current_odr_setting | s_lsm6d_state.current_fs_setting;
  if (!prv_write_register(I2C_LSM6D, LSM6D_CTRL1_XL, ctrl_reg)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6D: Failed to configure CTRL1_XL");
    return false;
  }
  
  // Configure common settings in CTRL3_C
  // BDU=1 (Block Data Update), IF_INC=1 (Auto-increment register address)
  // H_LACTIVE=0 (Interrupt active high), PP_OD=0 (Push-pull output)
  if (!prv_write_register(I2C_LSM6D, LSM6D_CTRL3_C, 0x44)) { // BDU=1, IF_INC=1
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6D: Failed to configure CTRL3_C");
    return false;
  }
  
  return true;
}

static bool prv_lsm6d_init(void) {
  uint8_t result;
  
  // Check WHO_AM_I register to verify sensor is present
  if (!prv_read_register(I2C_LSM6D, LSM6D_WHO_AM_I, &result)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6D: Failed to read WHO_AM_I register");
    return false;
  }
  
  if (result != LSM6D_WHO_AM_I_VALUE) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6D: Wrong WHO_AM_I value: 0x%02x (expected 0x%02x)", 
            result, LSM6D_WHO_AM_I_VALUE);
    return false;
  }
  
  // Reset sensor to known state
  if (!prv_write_register(I2C_LSM6D, LSM6D_CTRL3_C, 0x01)) { // SW_RESET
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6D: Failed to reset sensor");
    return false;
  }
  
  // Wait for reset to complete
  delay_us(1000);
  
  // Configure the sensor
  if (!prv_lsm6d_configure()) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6D: Failed to configure sensor");
    return false;
  }
  
  s_lsm6d_state.initialized = true;
  PBL_LOG(LOG_LEVEL_DEBUG, "LSM6D: Initialization complete");
  return true;
}

// Implement imu.h methods

void imu_init(void) {
  bool rv;
  uint8_t result;
  
  // Initialize LSM6DSO accelerometer
  if (!prv_lsm6d_init()) {
    PBL_LOG(LOG_LEVEL_ERROR, "IMU: LSM6DSO initialization failed");
  }
  
  // MMC5603NJ magnetometer and BMP390 pressure sensors can be brought
  // down if they are up; but I think we will want to manage these elsewhere.

  // // Probe and power down other sensors to save power
  // rv = prv_read_register(I2C_MMC5603NJ, MMC5603_PRODUCT_ID, &result);
  // if (rv && result == MMC5603_PRODUCT_ID_VALUE) {
  //   prv_write_register(I2C_MMC5603NJ, MMC5603_CONTROL2, 0);
  // }

  // rv = prv_read_register(I2C_BMP390, BMP390_CHIP_ID, &result);
  // if (rv && result == BMP390_CHIP_ID_VALUE) {
  //   prv_write_register(I2C_BMP390, BMP390_PWR_CTRL, 0);
  // }
}

void imu_power_up(void) {
  if (!s_lsm6d_state.initialized) {
    prv_lsm6d_init();
  } else {
    // Re-configure in case settings changed
    prv_lsm6d_configure();
  }
}

void imu_power_down(void) {
  // Power down accelerometer
  prv_write_register(I2C_LSM6D, LSM6D_CTRL1_XL, LSM6D_XL_ODR_OFF);
}

// Implement accel.h methods.

// Accelerometer driver interface implementation
uint32_t accel_set_sampling_interval(uint32_t interval_us) {
  if (!s_lsm6d_state.initialized) {
    if (!prv_lsm6d_init()) {
      return s_lsm6d_state.sampling_interval_us;
    }
  }
  
  uint8_t new_odr = prv_interval_us_to_odr_setting(interval_us);
  uint32_t actual_interval = prv_odr_setting_to_interval_us(new_odr);
  
  s_lsm6d_state.current_odr_setting = new_odr;
  s_lsm6d_state.sampling_interval_us = actual_interval;
  
  // Update hardware configuration if sensor is active
  if (s_lsm6d_state.num_samples > 0) {
    prv_lsm6d_configure();
  }
  
  return actual_interval;
}

uint32_t accel_get_sampling_interval(void) {
  return s_lsm6d_state.sampling_interval_us;
}

static void prv_process_pending_interrupts(void) {
  if (!s_lsm6d_state.initialized || s_lsm6d_state.num_samples == 0) {
    s_interrupt_pending = false;
    return;
  }
  
  // Process available accelerometer data
  int max_samples = 32; // Reasonable batch size
  
  while (max_samples-- > 0) {
    uint8_t status;
    if (!prv_read_register(I2C_LSM6D, LSM6D_STATUS_REG, &status)) {
      break;
    }
    
    if (status & LSM6D_STATUS_XLDA) {
      AccelDriverSample sample;
      uint8_t raw_data[6];
      
      if (prv_read_multiple_registers(I2C_LSM6D, LSM6D_OUTX_L_XL, raw_data, 6)) {
        // Convert raw data to accelerometer sample
        int16_t x = (int16_t)((raw_data[1] << 8) | raw_data[0]);
        int16_t y = (int16_t)((raw_data[3] << 8) | raw_data[2]);
        int16_t z = (int16_t)((raw_data[5] << 8) | raw_data[4]);
        
        // Convert to milli-g (±2g full scale)
        sample.x = (x * 2000) / 32768;
        sample.y = (y * 2000) / 32768;
        sample.z = (z * 2000) / 32768;
        sample.timestamp_us = 0; // TODO: proper timestamping
        
        // Clear interrupt source
        uint8_t int_src;
        prv_read_register(I2C_LSM6D, LSM6D_ALL_INT_SRC, &int_src);
        
        // Deliver sample to apps
        accel_cb_new_sample(&sample);
      } else {
        break;
      }
    } else {
      break;
    }
  }
  
  s_interrupt_pending = false;
}

// Interrupt handler for accelerometer data ready
static void prv_accel_interrupt_handler(bool *should_context_switch) {
  if (!s_lsm6d_state.initialized) {
    return;
  }
  
  // Defer work to task context using standard pattern
  if (!s_interrupt_pending) {
    s_interrupt_pending = true;
    accel_offload_work_from_isr(prv_process_pending_interrupts, should_context_switch);
  }
}

static void prv_configure_exti_once(void) {
  if (!s_exti_configured) {
    exti_configure_pin(BOARD_CONFIG_ACCEL.accel_ints[0], ExtiTrigger_Rising, prv_accel_interrupt_handler);
    s_exti_configured = true;
  }
}

static void prv_configure_data_ready_interrupt(bool enable) {
  if (enable && !s_data_interrupt_enabled) {
    // Enable data-ready interrupt on INT1
    uint8_t int1_ctrl;
    prv_read_register(I2C_LSM6D, LSM6D_INT1_CTRL, &int1_ctrl);
    int1_ctrl |= 0x01; // Enable data-ready interrupt on INT1
    prv_write_register(I2C_LSM6D, LSM6D_INT1_CTRL, int1_ctrl);
    
    // Configure and enable GPIO interrupt
    prv_configure_exti_once();
    exti_enable(BOARD_CONFIG_ACCEL.accel_ints[0]);
    
    s_data_interrupt_enabled = true;
  } else if (!enable && s_data_interrupt_enabled) {
    // Disable data-ready interrupt
    uint8_t int1_ctrl;
    prv_read_register(I2C_LSM6D, LSM6D_INT1_CTRL, &int1_ctrl);
    int1_ctrl &= ~0x01; // Disable data-ready interrupt on INT1
    prv_write_register(I2C_LSM6D, LSM6D_INT1_CTRL, int1_ctrl);
    
    s_data_interrupt_enabled = false;
  }
}

void accel_set_num_samples(uint32_t num_samples) {
  s_lsm6d_state.num_samples = num_samples;
  
  if (!s_lsm6d_state.initialized) {
    if (!prv_lsm6d_init()) {
      PBL_LOG(LOG_LEVEL_ERROR, "LSM6D: Failed to initialize accelerometer");
      return;
    }
  }
  
  if (num_samples > 0) {
    // Enable accelerometer hardware and data-ready interrupts
    prv_lsm6d_configure();
    prv_configure_data_ready_interrupt(true);
  } else {
    // Disable interrupts and power down accelerometer
    prv_configure_data_ready_interrupt(false);
    imu_power_down();
  }
}

int accel_peek(AccelDriverSample *data) {
  // Simple implementation like BMA255 - just read current sensor data
  if (!s_lsm6d_state.initialized) {
    PBL_LOG(LOG_LEVEL_DEBUG, "LSM6D: accel_peek called but not initialized");
    return -1; // Not initialized
  }
  
  // If sensor is powered down (num_samples == 0), temporarily enable it
  bool was_powered_down = (s_lsm6d_state.num_samples == 0);
  if (was_powered_down) {
    // Temporarily enable sensor for peek
    prv_lsm6d_configure();
    delay_us(1000); // Brief delay for sensor to stabilize
  }
  
  uint8_t raw_data[6];
  
  // Read 6 bytes starting from OUTX_L_XL
  if (!prv_read_multiple_registers(I2C_LSM6D, LSM6D_OUTX_L_XL, raw_data, 6)) {
    PBL_LOG(LOG_LEVEL_ERROR, "LSM6D: Failed to read accelerometer data");
    
    // If we temporarily enabled, power back down
    if (was_powered_down) {
      imu_power_down();
    }
    return -1;
  }
  
  // Convert raw data to signed 16-bit values
  int16_t raw_x = (int16_t)((raw_data[1] << 8) | raw_data[0]);
  int16_t raw_y = (int16_t)((raw_data[3] << 8) | raw_data[2]);
  int16_t raw_z = (int16_t)((raw_data[5] << 8) | raw_data[4]);
  
  // Convert to milli-g (assuming 2G full scale)
  // For 2G full scale: 1 LSB = 0.061 mg
  data->x = (raw_x * 61) / 1000; // Convert to milli-g
  data->y = (raw_y * 61) / 1000;
  data->z = (raw_z * 61) / 1000;
  
  // Set timestamp (approximate)
  data->timestamp_us = 0; // TODO: Implement proper timestamping
  
  // If we temporarily enabled, power back down
  if (was_powered_down) {
    imu_power_down();
  }
  
  return 0; // Success
}

void accel_enable_shake_detection(bool on) {
  s_lsm6d_state.shake_detection_enabled = on;
  
  if (!s_lsm6d_state.initialized) {
    if (!prv_lsm6d_init()) {
      return;
    }
  }
  
  if (on) {
    // Configure wake-up threshold for shake detection
    prv_write_register(I2C_LSM6D, LSM6D_WAKE_UP_THS, 0x80 | 0x3F); // Enable, set threshold
    prv_write_register(I2C_LSM6D, LSM6D_WAKE_UP_DUR, 0x00); // Set duration
    
    // Route wake-up interrupt to INT1
    uint8_t int1_ctrl;
    prv_read_register(I2C_LSM6D, LSM6D_INT1_CTRL, &int1_ctrl);
    int1_ctrl |= 0x20; // Enable wake-up interrupt on INT1
    prv_write_register(I2C_LSM6D, LSM6D_INT1_CTRL, int1_ctrl);
    
    // Configure and enable GPIO interrupt
    prv_configure_exti_once();
    exti_enable(BOARD_CONFIG_ACCEL.accel_ints[0]);
  } else {
    // Disable wake-up interrupt
    uint8_t int1_ctrl;
    prv_read_register(I2C_LSM6D, LSM6D_INT1_CTRL, &int1_ctrl);
    int1_ctrl &= ~0x20; // Disable wake-up interrupt on INT1
    prv_write_register(I2C_LSM6D, LSM6D_INT1_CTRL, int1_ctrl);
  }
}

bool accel_get_shake_detection_enabled(void) {
  return s_lsm6d_state.shake_detection_enabled;
}

void accel_set_shake_sensitivity_high(bool sensitivity_high) {
  if (!s_lsm6d_state.initialized) {
    if (!prv_lsm6d_init()) {
      return;
    }
  }
  
  // Configure the wake-up threshold for shake detection
  uint8_t threshold_val;
  if (sensitivity_high) {
    threshold_val = 0x80 | 0x0F; // Enable + low threshold (high sensitivity)
  } else {
    threshold_val = 0x80 | 0x3F; // Enable + high threshold (normal sensitivity)
  }
  
  prv_write_register(I2C_LSM6D, LSM6D_WAKE_UP_THS, threshold_val);
}

void accel_enable_double_tap_detection(bool on) {
  s_lsm6d_state.double_tap_detection_enabled = on;
  
  if (!s_lsm6d_state.initialized) {
    if (!prv_lsm6d_init()) {
      return;
    }
  }
  
  if (on) {
    // Configure double tap detection
    prv_write_register(I2C_LSM6D, LSM6D_TAP_CFG0, 0x0E); // Enable X,Y,Z tap detection
    prv_write_register(I2C_LSM6D, LSM6D_TAP_CFG1, 0x8C); // Enable double tap
    prv_write_register(I2C_LSM6D, LSM6D_TAP_THS_6D, 0x8C); // Set tap threshold
    prv_write_register(I2C_LSM6D, LSM6D_TAP_DUR, 0x7F); // Set tap duration
    
    // Route double tap interrupt to INT1
    uint8_t int1_ctrl;
    prv_read_register(I2C_LSM6D, LSM6D_INT1_CTRL, &int1_ctrl);
    int1_ctrl |= 0x08; // Enable double tap interrupt on INT1
    prv_write_register(I2C_LSM6D, LSM6D_INT1_CTRL, int1_ctrl);
    
    // Configure and enable GPIO interrupt
    prv_configure_exti_once();
    exti_enable(BOARD_CONFIG_ACCEL.accel_ints[0]);
  } else {
    // Disable double tap interrupt
    uint8_t int1_ctrl;
    prv_read_register(I2C_LSM6D, LSM6D_INT1_CTRL, &int1_ctrl);
    int1_ctrl &= ~0x08; // Disable double tap interrupt on INT1
    prv_write_register(I2C_LSM6D, LSM6D_INT1_CTRL, int1_ctrl);
  }
}

bool accel_get_double_tap_detection_enabled(void) {
  return s_lsm6d_state.double_tap_detection_enabled;
}

