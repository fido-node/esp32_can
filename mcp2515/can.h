#ifndef CAN_H
#define CAN_H
#include <stdbool.h>

typedef struct CanFrame {

  uint8_t IsExt;

  uint32_t CanId;       /*!< Specifies the standard identifier.
                             This parameter must be a number between Min_Data = 0 and Max_Data = 0x7FF. */
  uint8_t DLC;         /*!< Specifies the length of the frame that will be received.
                             This parameter must be a number between Min_Data = 0 and Max_Data = 8. */
  uint8_t Data[8];      /*!< Contains the data to be received.
                             This parameter must be a number between Min_Data = 0 and Max_Data = 0xFF. */
} can_frame_t;


#endif