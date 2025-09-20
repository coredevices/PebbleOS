#pragma once

//! @addtogroup UI
//! @{
//!   @addtogroup Clicks
//!   \brief Dealing with button input
//!   @{

//! Button ID values
//! @see \ref click_recognizer_get_button_id()
typedef enum {
  //! Back button
  BUTTON_ID_BACK = 0,
  //! Up button
  BUTTON_ID_UP,
  //! Select (middle) button
  BUTTON_ID_SELECT,
  //! Down button
  BUTTON_ID_DOWN,
  //! Total number of buttons
  NUM_BUTTONS
} ButtonId;

//!   @} // end addtogroup Clicks
//! @} // end addtogroup UI
