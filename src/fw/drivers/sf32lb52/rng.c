#include "drivers/rng.h"

#include "drivers/periph_config.h"
#include "system/passert.h"
#include "kernel/util/sleep.h"
#include "board/board.h"

#include <mcu.h>

bool flag_inited = false;
RNG_HandleTypeDef   RngHandle;

bool rng_rand(uint32_t *rand_out) {

  PBL_ASSERTN(rand_out);
  HAL_StatusTypeDef   status;
  uint32_t            seed;

  /*##-1- Initialize RNG peripheral #######################################*/
  if(!flag_inited)
  {
      RngHandle.Instance = hwp_trng;
      status = HAL_RNG_Init(&RngHandle);
      if ( status != HAL_OK)
      {
          /* Initialization Error */
          //PBL_LOG(LOG_LEVEL_ERROR,"rng_rand init fail!");
          return false;
      }
      //PBL_LOG(LOG_LEVEL_INFO,"rng_rand first init ok!");
      flag_inited = true;
  }

  
  /*##-2- Generate random ###################################*/
  seed = 0;
  status = HAL_RNG_Generate(&RngHandle, &seed, 1);
  //PBL_LOG(LOG_LEVEL_INFO,"Generate Random value=%x, status=%d", (unsigned int)seed, (int)status);
  PBL_ASSERTN(seed != 0);
  *rand_out = seed;

  return true;
}
void example_rng(void)
{
  uint8_t i = 0;
  uint32_t value;
  for(i = 0; i < 8; i++)
  {
    rng_rand(&value);
    PBL_LOG(LOG_LEVEL_INFO,"get random value 0x%x;", (unsigned int)value);
  }

}