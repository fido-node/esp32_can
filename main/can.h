#ifndef CAN_H
#define CAN_H
#include <stdbool.h>

typedef struct {
  uint32_t StdId;       /*!< Specifies the standard identifier.
                             This parameter must be a number between Min_Data = 0 and Max_Data = 0x7FF. */ 
  uint32_t ExtId;       /*!< Specifies the extended identifier.
                             This parameter must be a number between Min_Data = 0 and Max_Data = 0x1FFFFFFF. */ 

  uint32_t IDE;         /*!< Specifies the type of identifier for the message that will be received.
                             This parameter can be a value of @ref CAN_identifier_type */

  uint32_t RTR;         /*!< Specifies the type of frame for the received message.
                             This parameter can be a value of @ref CAN_remote_transmission_request */
  uint32_t DLC;         /*!< Specifies the length of the frame that will be received.
                             This parameter must be a number between Min_Data = 0 and Max_Data = 8. */
  uint8_t Data[8];      /*!< Contains the data to be received. 
                             This parameter must be a number between Min_Data = 0 and Max_Data = 0xFF. */
} can_frame_rx_t;

typedef struct {
  
  bool IsExt;

  long CanId;       /*!< Specifies the standard identifier.
                             This parameter must be a number between Min_Data = 0 and Max_Data = 0x7FF. */ 
  int DLC;         /*!< Specifies the length of the frame that will be received.
                             This parameter must be a number between Min_Data = 0 and Max_Data = 8. */
  uint8_t Data[8];      /*!< Contains the data to be received. 
                             This parameter must be a number between Min_Data = 0 and Max_Data = 0xFF. */
} can_frame_tx_t;


#endif