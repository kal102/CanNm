/*
LICENSE

The MIT License (MIT)

Copyright (c) 2021 Lukasz Kalina

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

/** ==================================================================================================================*\
  @file CanNm.c

  @brief Can Network Management Module
  
  Implementation of Can Network Managment Module
\*====================================================================================================================*/

/*====================================================================================================================*\
    Include headers
\*====================================================================================================================*/
#include "string.h"
#include "Std_Types.h"	//[SWS_CanNm_00146]

/* [SWS_CanNm_00305] */
#include "ComStack_Types.h"

/* [SWS_CanNm_00307] */
//#include "Nm_Cbk.h"

/* [SWS_CanNm_00308] */
#include "Det.h"

/* [SWS_CanNm_00309] */
#include "NmStack_Types.h"

/* [SWS_CanNm_00312] */
#ifndef UNIT_TEST
#include "CanIf.h"
#endif

/* [SWS_CanNm_00326] */
//#include "PduR_CanNm.h"

#include "CanNm.h"
//#include "CanNm_Cbk.h"
//#include "CanNm_MemMap.h"
//#include "SchM_CanNm.h"

#include "fff.h"

/*====================================================================================================================*\
    Local macros
\*====================================================================================================================*/
/* Control Bit Vector */
#define REPEAT_MESSAGE_REQUEST 			0
#define NM_COORDINATOR_SLEEP_READY_BIT 	3
#define ACTIVE_WAKEUP_BIT				4
#define PARTIAL_NETWORK_INFORMATION_BIT	5

#define NO_PDU_RECEIVED -1

/*====================================================================================================================*\
    Local types
\*====================================================================================================================*/
typedef void (*CanNm_TimerCallback)(void* Timer, const uint8 channel);

typedef enum {
	CANNM_TIMER_STOPPED,
	CANNM_TIMER_STARTED
} CanNm_TimerState;

typedef struct {
	uint8						Channel;				
	CanNm_TimerCallback 		ExpiredCallback;
	CanNm_TimerState			State;
	float32						TimeLeft;
} CanNm_Timer;

typedef enum {
	CANNM_INIT,
	CANNM_UNINIT
} CanNm_InitStatusType;

typedef struct {
	uint8						Channel;
	Nm_ModeType					Mode;					//[SWS_CanNm_00092]
	Nm_StateType				State;					//[SWS_CanNm_00089]
	boolean						Requested;
	boolean						TxEnabled;
	sint8						RxLastPdu;
	CanNm_Timer					TimeoutTimer;			//NM-Timeout Timer, Tx Timeout Timer
	CanNm_Timer					MessageCycleTimer;
	CanNm_Timer					RepeatMessageTimer;
	CanNm_Timer					WaitBusSleepTimer;
	CanNm_Timer					RemoteSleepIndTimer;
	uint8						ImmediateTransmissions;
	boolean						BusLoadReduction;		//[SWS_CanNm_00238]
	boolean						RemoteSleepInd;
	boolean						RemoteSleepIndEnabled;
	boolean						NmPduFilterAlgorithm;
} CanNm_Internal_ChannelType;

typedef struct {
	CanNm_InitStatusType 		InitStatus;
	CanNm_Internal_ChannelType	Channels[CANNM_CHANNEL_COUNT];
} CanNm_InternalType;

/*====================================================================================================================*\
    Global variables
\*====================================================================================================================*/
CanNm_InternalType CanNm_Internal = {
		.InitStatus = CANNM_UNINIT
};

/*====================================================================================================================*\
    Local variables (static)
\*====================================================================================================================*/
static const CanNm_ConfigType* CanNm_ConfigPtr;

/*====================================================================================================================*\
    Local functions declarations
\*====================================================================================================================*/
/* Timer functions */
static inline void CanNm_Internal_TimerStart( CanNm_Timer* Timer, uint32 timeoutValue );
static inline void CanNm_Internal_TimerResume( CanNm_Timer* Timer );
static inline void CanNm_Internal_TimerStop( CanNm_Timer* Timer );
static inline void CanNm_Internal_TimerReset( CanNm_Timer* Timer, uint32 timeoutValue );
static inline void CanNm_Internal_TimerTick( CanNm_Timer* Timer, const uint8 channel, const float32 period );

static inline void CanNm_Internal_TimersInit( uint8 channel );
static inline void CanNm_Internal_TimeoutTimerExpiredCallback( void* Timer, const uint8 channel );
static inline void CanNm_Internal_MessageCycleTimerExpiredCallback( void* Timer, const uint8 channel );
static inline void CanNm_Internal_RepeatMessageTimerExpiredCallback( void* Timer, const uint8 channel );
static inline void CanNm_Internal_WaitBusSleepTimerExpiredCallback( void* Timer, const uint8 channel );
static inline void CanNm_Internal_RemoteSleepIndTimerExpiredCallback( void* Timer, const uint8 channel );

/* State Machine functions */
static inline void CanNm_Internal_BusSleep_to_BusSleep( const CanNm_ChannelType* ChannelConf,
 														CanNm_Internal_ChannelType* ChannelInternal );
static inline void 	 CanNm_Internal_BusSleep_to_RepeatMessage( const CanNm_ChannelType* ChannelConf,
 																CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_RepeatMessage_to_RepeatMessage( const CanNm_ChannelType* ChannelConf,
 																	CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_RepeatMessage_to_ReadySleep( const CanNm_ChannelType* ChannelConf,
 																CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_RepeatMessage_to_NormalOperation( const CanNm_ChannelType* ChannelConf,
 																	CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_NormalOperation_to_RepeatMessage( const CanNm_ChannelType* ChannelConf,
 																	CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_NormalOperation_to_NormalOperation( const CanNm_ChannelType* ChannelConf,
 																		CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_NormalOperation_to_ReadySleep( const CanNm_ChannelType* ChannelConf,
 																	CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_ReadySleep_to_NormalOperation( const CanNm_ChannelType* ChannelConf,
 																	CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_ReadySleep_to_RepeatMessage( const CanNm_ChannelType* ChannelConf,
 																CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_ReadySleep_to_PrepareBusSleep( const CanNm_ChannelType* ChannelConf,
 																	CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_PrepareBusSleep_to_RepeatMessage( const CanNm_ChannelType* ChannelConf,
 																	CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_PrepareBusSleep_to_BusSleep( const CanNm_ChannelType* ChannelConf,
 																CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_NetworkMode_to_NetworkMode( const CanNm_ChannelType* ChannelConf,
 																CanNm_Internal_ChannelType* ChannelInternal );

/* Additional functions */
static inline Std_ReturnType CanNm_Internal_TxDisable( CanNm_Internal_ChannelType* ChannelInternal );
static inline Std_ReturnType CanNm_Internal_TxEnable( CanNm_Internal_ChannelType* ChannelInternal );
static inline Std_ReturnType CanNm_Internal_TransmitMessage( const CanNm_ChannelType* ChannelConf,
 																CanNm_Internal_ChannelType* ChannelInternal );
static inline void CanNm_Internal_SetPduCbvBit( const CanNm_ChannelType* ChannelConf, const uint8 PduCbvBitPosition );
static inline void CanNm_Internal_ClearPduCbvBit( const CanNm_ChannelType* ChannelConf, const uint8 PduCbvBitPosition );
static inline void CanNm_Internal_ClearPduCbv( const CanNm_ChannelType* ChannelConf,
 												CanNm_Internal_ChannelType* ChannelInternal );
static inline uint8 CanNm_Internal_GetUserDataOffset( const CanNm_ChannelType* ChannelConf );
static inline uint8* CanNm_Internal_GetUserDataPtr( const CanNm_ChannelType* ChannelConf, uint8* MessageSduPtr );
static inline uint8 CanNm_Internal_GetUserDataLength( const CanNm_ChannelType* ChannelConf );

/*====================================================================================================================*\
	Global inline functions and function macros code
\*====================================================================================================================*/

#ifdef UNIT_TEST

DEFINE_FFF_GLOBALS;

/* Mandatory interfaces [SWS_CanNm_00324][SWS_CanNm_00093] */
FAKE_VALUE_FUNC(Std_ReturnType, Det_ReportRuntimeError, uint16, uint8, uint8, uint8);
FAKE_VOID_FUNC(Nm_BusSleepMode, const NetworkHandleType);
FAKE_VOID_FUNC(Nm_NetworkMode, const NetworkHandleType);
FAKE_VOID_FUNC(Nm_NetworkStartIndication, const NetworkHandleType);
FAKE_VOID_FUNC(Nm_PrepareBusSleepMode, const NetworkHandleType);

/* Optional interfaces [SWS_CanNm_00325] */
FAKE_VALUE_FUNC(Std_ReturnType, CanIf_Transmit, PduIdType, const PduInfoType*);
FAKE_VOID_FUNC(CanSM_TxTimeoutException, NetworkHandleType);
FAKE_VOID_FUNC(Det_ReportError, uint16, uint8, uint8, uint8);
FAKE_VOID_FUNC(Nm_CarWakeUpIndication, NetworkHandleType);
FAKE_VOID_FUNC(Nm_CoordReadyToSleepCancellation, NetworkHandleType);
FAKE_VOID_FUNC(Nm_CoordReadyToSleepIndication, NetworkHandleType);
FAKE_VOID_FUNC(Nm_PduRxIndication, NetworkHandleType);
FAKE_VOID_FUNC(Nm_RemoteSleepCancellation, NetworkHandleType);
FAKE_VOID_FUNC(Nm_RemoteSleepInd, NetworkHandleType);
FAKE_VOID_FUNC(Nm_RepeatMessageIndication, NetworkHandleType);
FAKE_VOID_FUNC(Nm_StateChangeNotification, NetworkHandleType, Nm_StateType, Nm_StateType);
FAKE_VOID_FUNC(Nm_TxTimeoutException, NetworkHandleType);
FAKE_VOID_FUNC(PduR_CanNmRxIndication, PduIdType, PduInfoType*);
FAKE_VOID_FUNC(PduR_CanNmTriggerTransmit, PduIdType, PduInfoType*);
FAKE_VOID_FUNC(PduR_CanNmTxConfirmation, PduIdType);

#endif //UNIT_TEST

/*====================================================================================================================*\
    Global functions code
\*====================================================================================================================*/

/** @brief CanNm_Init [SWS_CanNm_00208]
 * 
 * Initialize the CanNm module.
 */
void CanNm_Init(const CanNm_ConfigType* cannmConfigPtr)
{
    CanNm_ConfigPtr = cannmConfigPtr;	//[SWS_CanNm_00060]

	for (uint8 channel = 0; channel < CANNM_CHANNEL_COUNT; channel++) {
		const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[channel];
		CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[channel];

		ChannelInternal->Channel = channel;
		ChannelInternal->Mode = NM_MODE_BUS_SLEEP;														//[SWS_CanNm_00144]
		ChannelInternal->State = NM_STATE_BUS_SLEEP;													//[SWS_CanNm_00141][SWS_CanNm_00094]
		ChannelInternal->Requested = FALSE;																//[SWS_CanNm_00143]
		ChannelInternal->TxEnabled = FALSE;
		ChannelInternal->RxLastPdu = NO_PDU_RECEIVED;
		ChannelInternal->ImmediateTransmissions = 0;
		ChannelInternal->BusLoadReduction = FALSE;														//[SWS_CanNm_00023]
		ChannelInternal->RemoteSleepInd = FALSE;
		ChannelInternal->RemoteSleepIndEnabled = CanNm_ConfigPtr->RemoteSleepIndEnabled;
		ChannelInternal->NmPduFilterAlgorithm = FALSE;

		if (ChannelConf->NodeIdEnabled && ChannelConf->PduNidPosition != CANNM_PDU_OFF) {
			ChannelConf->TxPdu->TxPduRef->SduDataPtr[ChannelConf->PduNidPosition] = ChannelConf->NodeId;//[SWS_CanNm_00013]
		}

		CanNm_Internal_ClearPduCbv(ChannelConf, ChannelInternal);										//[SWS_CanNm_00085]

		uint8* destUserData = CanNm_Internal_GetUserDataPtr(ChannelConf, ChannelConf->UserDataTxPdu->TxUserDataPduRef->SduDataPtr);
		uint8 userDataLength = CanNm_Internal_GetUserDataLength(ChannelConf);
		memset(destUserData, 0xFF, userDataLength);														//[SWS_CanNm_00025]

		CanNm_Internal_TimersInit(channel);																//[SWS_CanNm_00061][SWS_CanNm_00033]
	}
	CanNm_Internal.InitStatus = CANNM_INIT;
}

/** @brief CanNm_DeInit [SWS_CanNm_91002]
 * 
 * De-initializes the CanNm module.
 */
void CanNm_DeInit(void)
{
    for (uint8 channel = 0; channel < CANNM_CHANNEL_COUNT; channel++) {
		CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[channel];

		if (ChannelInternal->State != NM_STATE_BUS_SLEEP) {
			return;
		}
		CanNm_Internal_TimersInit(channel);
		ChannelInternal->State = NM_STATE_UNINIT;
	}
	CanNm_Internal.InitStatus = CANNM_UNINIT;
}

/** @brief CanNm_PassiveStartUp [SWS_CanNm_00211]
 * 
 * Passive startup of the AUTOSAR CAN NM. It triggers the transition from Bus-Sleep Mode
 *  or Prepare Bus Sleep Mode to the Network Mode in Repeat Message State.
 */ 
Std_ReturnType CanNm_PassiveStartUp(NetworkHandleType nmChannelHandle)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];
	Std_ReturnType status = E_OK;

	if (CanNm_ConfigPtr->PassiveModeEnabled && ChannelInternal->Mode != NM_MODE_NETWORK) {				//[SWS_CanNm_00161]
        CanNm_Internal_BusSleep_to_RepeatMessage(ChannelConf, ChannelInternal);							//[SWS_CanNm_00128][SWS_CanNm_00314][SWS_CanNm_00315]
		status = E_OK;
	} else {
		status = E_NOT_OK;																				//[SWS_CanNm_00147]
	}
	return status;
}

/** @brief CanNm_NetworkRequest [SWS_CanNm_00213]
 * 
 * Request the network, since ECU needs to communicate on the bus.
 */
Std_ReturnType CanNm_NetworkRequest(NetworkHandleType nmChannelHandle)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	ChannelInternal->Requested = TRUE;	//[SWS_CanNm_00104][SWS_CanNm_00255]

	if (ChannelInternal->Mode == NM_MODE_BUS_SLEEP) {
		if (!CanNm_ConfigPtr->PassiveModeEnabled) {
			ChannelInternal->TxEnabled = TRUE;															//[SWS_CanNm_00072]
		}
		CanNm_Internal_BusSleep_to_RepeatMessage(ChannelConf, ChannelInternal);							//[SWS_CanNm_00129][SWS_CanNm_00314]
		if (ChannelConf->ActiveWakeupBitEnabled) {
			CanNm_Internal_SetPduCbvBit(ChannelConf, ACTIVE_WAKEUP_BIT);								//[SWS_CanNm_00401]
			if (ChannelConf->ImmediateNmTransmissions) {												//[SWS_CanNm_00005][SWS_CanNm_00334]
				ChannelInternal->ImmediateTransmissions = ChannelConf->ImmediateNmTransmissions;
				CanNm_Internal_MessageCycleTimerExpiredCallback(&ChannelInternal->MessageCycleTimer,
				 ChannelInternal->MessageCycleTimer.Channel);
			}
		}
	} else if (ChannelInternal->Mode == NM_MODE_PREPARE_BUS_SLEEP) {
		if (!CanNm_ConfigPtr->PassiveModeEnabled) {
			ChannelInternal->TxEnabled = TRUE;															//[SWS_CanNm_00072]
		}
		CanNm_Internal_PrepareBusSleep_to_RepeatMessage(ChannelConf, ChannelInternal);					//[SWS_CanNm_00123][SWS_CanNm_00315]
		if (ChannelConf->ActiveWakeupBitEnabled) {
			CanNm_Internal_SetPduCbvBit(ChannelConf, ACTIVE_WAKEUP_BIT);								//[SWS_CanNm_00401]
			if (CanNm_ConfigPtr->ImmediateRestartEnabled || ChannelConf->ImmediateNmTransmissions) {	//[SWS_CanNm_00005][SWS_CanNm_00122][SWS_CanNm_00334]
				ChannelInternal->ImmediateTransmissions = ChannelConf->ImmediateNmTransmissions;
				CanNm_Internal_MessageCycleTimerExpiredCallback(&ChannelInternal->MessageCycleTimer,
				 ChannelInternal->MessageCycleTimer.Channel);
			}
		}
	} else if (ChannelInternal->Mode == NM_MODE_NETWORK) {
		if (ChannelInternal->State == NM_STATE_READY_SLEEP) {
			if (ChannelConf->PnHandleMultipleNetworkRequests && ChannelConf->ImmediateNmTransmissions) {//[SWS_CanNm_00444][SWS_CanNm_00454]
				CanNm_Internal_ReadySleep_to_RepeatMessage(ChannelConf, ChannelInternal);
				ChannelInternal->ImmediateTransmissions = ChannelConf->ImmediateNmTransmissions;
				CanNm_Internal_MessageCycleTimerExpiredCallback(&ChannelInternal->MessageCycleTimer,
				 ChannelInternal->MessageCycleTimer.Channel);
			}
			else {
				CanNm_Internal_ReadySleep_to_NormalOperation(ChannelConf, ChannelInternal);				//[SWS_CanNm_00110]
				if (CanNm_ConfigPtr->RemoteSleepIndEnabled) {											//[SWS_CanNm_00149]
					CanNm_Internal_TimerStart(&ChannelInternal->RemoteSleepIndTimer, ChannelConf->RemoteSleepIndTime);
				}
			}
		} else if (ChannelInternal->State == NM_STATE_NORMAL_OPERATION) {
			if (ChannelConf->PnHandleMultipleNetworkRequests && ChannelConf->ImmediateNmTransmissions) {//[SWS_CanNm_00444][SWS_CanNm_00454]
				CanNm_Internal_NormalOperation_to_RepeatMessage(ChannelConf, ChannelInternal);
				ChannelInternal->ImmediateTransmissions = ChannelConf->ImmediateNmTransmissions;
				CanNm_Internal_MessageCycleTimerExpiredCallback(&ChannelInternal->MessageCycleTimer,
				 ChannelInternal->MessageCycleTimer.Channel);
			}
		} else if (ChannelInternal->State == NM_STATE_REPEAT_MESSAGE) {
			if (ChannelConf->PnHandleMultipleNetworkRequests && ChannelConf->ImmediateNmTransmissions) {//[SWS_CanNm_00444][SWS_CanNm_00454]
				CanNm_Internal_RepeatMessage_to_RepeatMessage(ChannelConf, ChannelInternal);
				ChannelInternal->ImmediateTransmissions = ChannelConf->ImmediateNmTransmissions;
				CanNm_Internal_MessageCycleTimerExpiredCallback(&ChannelInternal->MessageCycleTimer,
				 ChannelInternal->MessageCycleTimer.Channel);
			}		
		} else {
			//Nothing to be done
		}
	} else {
		//Nothing to be done
	}
	return E_OK;
}

/** @brief CanNm_NetworkRelease [SWS_CanNm_00214]
 * 
 * Release the network, since ECU doesn't have to communicate on the bus.
 */
Std_ReturnType CanNm_NetworkRelease(NetworkHandleType nmChannelHandle)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	ChannelInternal->Requested = FALSE;	//[SWS_CanNm_00105]

	if (ChannelInternal->Mode == NM_MODE_NETWORK) {
		if (ChannelInternal->State == NM_STATE_NORMAL_OPERATION) {
			CanNm_Internal_NormalOperation_to_ReadySleep(ChannelConf, ChannelInternal);			//[SWS_CanNm_00118]
		}
	}
	return E_OK;
}

/** @brief CanNm_DisableCommunication [SWS_CanNm_00215]
 * 
 * Disable the NM PDU transmission ability due to a ISO14229 Communication Control (28hex) service.
 */
Std_ReturnType CanNm_DisableCommunication(NetworkHandleType nmChannelHandle)
{
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	if (ChannelInternal->Mode == NM_MODE_NETWORK && !(CanNm_ConfigPtr->PassiveModeEnabled)) {	//[SWS_CanNm_00170]
		return CanNm_Internal_TxDisable(ChannelInternal);
	} else {																					//[SWS_CanNm_00172][SWS_CanNm_00298]
		return E_NOT_OK;
	}
}

/** @brief CanNm_EnableCommunication [SWS_CanNm_00216]
 * 
 * Enable the NM PDU transmission ability due to a ISO14229 Communication Control (28hex) service.
 */
Std_ReturnType CanNm_EnableCommunication(NetworkHandleType nmChannelHandle)
{
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	if (ChannelInternal->Mode == NM_MODE_NETWORK && !(CanNm_ConfigPtr->PassiveModeEnabled)) {	//[SWS_CanNm_00170]
		if (ChannelInternal->MessageCycleTimer.State == CANNM_TIMER_STOPPED) {					//[SWS_CanNm_00176]
			return CanNm_Internal_TxEnable(ChannelInternal);
		} else {																				//[SWS_CanNm_00177]
			return E_NOT_OK;
		}
	} else {																					//[SWS_CanNm_00295][SWS_CanNm_00297]
		return E_NOT_OK;
	}
}

/** @brief CanNm_SetUserData [SWS_CanNm_00217]
 * 
 * Set user data for NM PDUs transmitted next on the bus.
 */
Std_ReturnType CanNm_SetUserData(NetworkHandleType nmChannelHandle, const uint8* nmUserDataPtr)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	if (CanNm_ConfigPtr->UserDataEnabled && !CanNm_ConfigPtr->ComUserDataSupport) {				//[SWS_CanNm_00158][SWS_CanNm_00327]
		uint8* destUserData = CanNm_Internal_GetUserDataPtr(ChannelConf, ChannelConf->UserDataTxPdu->TxUserDataPduRef->SduDataPtr);
		uint8 userDataLength = CanNm_Internal_GetUserDataLength(ChannelConf);
		memcpy(destUserData, nmUserDataPtr, userDataLength);									//[SWS_CanNm_00159]	
		return E_OK;
	}
	else {
		return E_NOT_OK;
	}
}

/** @brief CanNm_GetUserData [SWS_CanNm_00218]
 * 
 * Get user data out of the most recently received NM PDU.
 */
Std_ReturnType CanNm_GetUserData(NetworkHandleType nmChannelHandle, uint8* nmUserDataPtr)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	if (CanNm_ConfigPtr->UserDataEnabled && ChannelInternal->RxLastPdu != NO_PDU_RECEIVED) {	//[SWS_CanNm_00158]
		uint8* srcUserData = CanNm_Internal_GetUserDataPtr(ChannelConf, ChannelConf->RxPdu[ChannelInternal->RxLastPdu]->RxPduRef->SduDataPtr);
		uint8 userDataLength = CanNm_Internal_GetUserDataLength(ChannelConf);
		memcpy(nmUserDataPtr, srcUserData, userDataLength);										//[SWS_CanNm_00160]
		return E_OK;
	} else {
		return E_NOT_OK;
	}
}

/** @brief CanNm_Transmit [SWS_CanNm_00331]
 * 
 * Requests transmission of a PDU.
 */
Std_ReturnType CanNm_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr)
{
	if (CanNm_ConfigPtr->ComUserDataSupport || CanNm_ConfigPtr->GlobalPnSupport) {				//[SWS_CanNm_00330]
		return CanIf_Transmit(TxPduId, PduInfoPtr);
	} else {
		return E_NOT_OK;
	}
}

/** @brief CanNm_GetNodeIdentifier [SWS_CanNm_00219]
 * 
 * Get node identifier out of the most recently received NM PDU.
 */
Std_ReturnType CanNm_GetNodeIdentifier(NetworkHandleType nmChannelHandle, uint8*nmNodeIdPtr)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	if (ChannelConf->PduNidPosition != CANNM_PDU_OFF) {
		if (ChannelInternal->RxLastPdu != NO_PDU_RECEIVED) {
			uint8 *pduNidPtr = ChannelConf->RxPdu[ChannelInternal->RxLastPdu]->RxPduRef->SduDataPtr;//[SWS_CanNm_00132]
			pduNidPtr += ChannelConf->PduNidPosition;
			*nmNodeIdPtr = *pduNidPtr;
			return E_OK;
		} else {
			return E_NOT_OK;
		}
	} else {
		return E_NOT_OK;
	}
}

/** @brief CanNm_GetNodeIdentifier [SWS_CanNm_00220]
 * 
 * Get node identifier configured for the local node.
 */
Std_ReturnType CanNm_GetLocalNodeIdentifier(NetworkHandleType nmChannelHandle, uint8* nmNodeIdPtr)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];

	*nmNodeIdPtr = ChannelConf->NodeId;	//[SWS_CanNm_00133]
	return E_OK;
}

/** @brief CanNm_RepeatMessageRequest [SWS_CanNm_00221]
 * 
 * Set Repeat Message Request Bit for NM PDUs transmitted next on the bus.
 */
Std_ReturnType CanNm_RepeatMessageRequest(NetworkHandleType nmChannelHandle)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	if (ChannelConf->PduCbvPosition != CANNM_PDU_OFF) {
		if (ChannelInternal->State == NM_STATE_READY_SLEEP) {
			if (ChannelConf->NodeDetectionEnabled) {								//[SWS_CanNm_00112]
				CanNm_Internal_SetPduCbvBit(ChannelConf, REPEAT_MESSAGE_REQUEST);	//[SWS_CanNm_00113]
				CanNm_Internal_ReadySleep_to_RepeatMessage(ChannelConf, ChannelInternal);
				return E_OK;
			} else {
				return E_NOT_OK;
			}
		} else if (ChannelInternal->State == NM_STATE_NORMAL_OPERATION) {
			if (ChannelConf->NodeDetectionEnabled) {								//[SWS_CanNm_00120]
				CanNm_Internal_SetPduCbvBit(ChannelConf, REPEAT_MESSAGE_REQUEST);	//[SWS_CanNm_00121]
				CanNm_Internal_NormalOperation_to_RepeatMessage(ChannelConf, ChannelInternal);
				return E_OK;
			} else {
				return E_NOT_OK;
			}
		} else {
			return E_NOT_OK;
		}
	} else {
		return E_NOT_OK;
	}
}

/** @brief CanNm_GetPduData [SWS_CanNm_00222]
 * 
 * Get the whole PDU data out of the most recently received NM PDU.
 */
Std_ReturnType CanNm_GetPduData(NetworkHandleType nmChannelHandle, uint8* nmPduDataPtr)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	if (ChannelConf->NodeDetectionEnabled || CanNm_ConfigPtr->UserDataEnabled || ChannelConf->NodeIdEnabled) {	//[SWS_CanNm_00138]
		if (ChannelInternal->RxLastPdu != NO_PDU_RECEIVED) {
			memcpy(nmPduDataPtr, ChannelConf->RxPdu[0]->RxPduRef->SduDataPtr, ChannelConf->RxPdu[0]->RxPduRef->SduLength);
			return E_OK;
		} else {
			return E_NOT_OK;
		}

	} else {
		return E_NOT_OK;
	}
}

/** @brief CanNm_GetState [SWS_CanNm_00223]
 * 
 * Returns the state and the mode of the network management.
 */
Std_ReturnType CanNm_GetState(NetworkHandleType nmChannelHandle, Nm_StateType* nmStatePtr, Nm_ModeType* nmModePtr)
{
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	*nmStatePtr = ChannelInternal->State;												//[SWS_CanNm_00091]
	*nmModePtr = ChannelInternal->Mode;
	return E_OK;    
}

/** @brief CanNm_GetVersionInfo [SWS_CanNm_00224]
 * 
 * This service returns the version information of this module.
 */
void CanNm_GetVersionInfo(Std_VersionInfoType* versioninfo)
{
	//TBD
}

/** @brief CanNm_RequestBusSynchronization [SWS_CanNm_00226]
 * 
 * Request bus synchronization.
 */
Std_ReturnType CanNm_RequestBusSynchronization(NetworkHandleType nmChannelHandle)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

    if (!CanNm_ConfigPtr->PassiveModeEnabled) {											//[SWS_CanNm_00130]
		if (ChannelInternal->Mode == NM_MODE_NETWORK && ChannelInternal->TxEnabled) {	//[SWS_CanNm_00181][SWS_CanNm_00187]
			CanNm_Internal_TransmitMessage(ChannelConf, ChannelInternal);
			return E_OK;			
		} else {
			return E_NOT_OK;
		}
	} else {
		return E_NOT_OK;
	}
}

/** @brief CanNm_CheckRemoteSleepInd [SWS_CanNm_00227]
 * 
 * Check if remote sleep indication takes place or not.
 */
Std_ReturnType CanNm_CheckRemoteSleepInd(NetworkHandleType nmChannelHandle, boolean* nmRemoteSleepIndPtr)
{
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	if (ChannelInternal->State != NM_STATE_BUS_SLEEP && ChannelInternal->State != NM_STATE_PREPARE_BUS_SLEEP 
		&& ChannelInternal->State != NM_STATE_REPEAT_MESSAGE) {							//[SWS_CanNm_00154]
		*nmRemoteSleepIndPtr = ChannelInternal->RemoteSleepInd;
		return E_OK;
	} else {
		return E_NOT_OK;
	}
}

/** @brief CanNm_SetSleepReadyBit [SWS_CanNm_00338]
 * 
 * Set the NM Coordinator Sleep Ready bit in the Control Bit Vector
 */
Std_ReturnType CanNm_SetSleepReadyBit(NetworkHandleType nmChannelHandle,boolean nmSleepReadyBit)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[nmChannelHandle];
    CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

	if (ChannelConf->PduCbvPosition != CANNM_PDU_OFF && CanNm_ConfigPtr->CoordinationSyncSupport) {	//[SWS_CanNm_00342]
		CanNm_Internal_SetPduCbvBit(ChannelConf, NM_COORDINATOR_SLEEP_READY_BIT);
		CanNm_Internal_TransmitMessage(ChannelConf, ChannelInternal);
		return E_OK;
	} else {
		return E_NOT_OK;
	}
}

/** @brief CanNm_TxConfirmation [SWS_CanNm_00228]
 * 
 * The lower layer communication interface module confirms the transmission of a PDU, or the failure to transmit a PDU.
 */
void CanNm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[TxPduId];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[TxPduId];

	if (result == E_OK) {
		CanNm_Internal_NetworkMode_to_NetworkMode(ChannelConf, ChannelInternal);			//[SWS_CanNm_00099]
	}
	if (CanNm_ConfigPtr->ComUserDataSupport) {
		PduR_CanNmRxIndication(TxPduId, ChannelConf->TxPdu->TxPduRef); 						//[SWS_CanNm_00329]
	}
}

/** @brief CanNm_TxConfirmation [SWS_CanNm_00231]
 * 
 * Indication of a received PDU from a lower layer communication interface module.
 */
void CanNm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[RxPduId];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[RxPduId];

	ChannelInternal->RxLastPdu = (ChannelInternal->RxLastPdu + 1) % (CANNM_RXPDU_MAX_COUNT);
	memcpy(ChannelConf->RxPdu[ChannelInternal->RxLastPdu]->RxPduRef->SduDataPtr, PduInfoPtr->SduDataPtr,
	 ChannelConf->RxPdu[ChannelInternal->RxLastPdu]->RxPduRef->SduLength);					//[SWS_CanNm_00035]

	boolean repeatMessageBitIndication = FALSE;
	if (ChannelConf->PduCbvPosition != CANNM_PDU_OFF && ChannelConf->NodeDetectionEnabled) {
		uint8 cbv = PduInfoPtr->SduDataPtr[ChannelConf->PduCbvPosition];
		repeatMessageBitIndication = cbv & (1 << REPEAT_MESSAGE_REQUEST);
	}

	if (ChannelInternal->Mode == NM_MODE_BUS_SLEEP) {
		CanNm_Internal_BusSleep_to_BusSleep(ChannelConf, ChannelInternal);
		Nm_NetworkStartIndication(RxPduId);													//[SWS_CanNm_00127]
	} else if (ChannelInternal->Mode == NM_MODE_PREPARE_BUS_SLEEP) {
		CanNm_Internal_PrepareBusSleep_to_RepeatMessage(ChannelConf, ChannelInternal);		//[SWS_CanNm_00124][SWS_CanNm_00315]
	} else if (ChannelInternal->Mode == NM_MODE_NETWORK) {
		CanNm_Internal_NetworkMode_to_NetworkMode(ChannelConf, ChannelInternal);  			//[SWS_CanNm_00098]
		if (repeatMessageBitIndication) {													//[SWS_CanNm_00119]
			if (ChannelInternal->State == NM_STATE_READY_SLEEP) {
				CanNm_Internal_ReadySleep_to_RepeatMessage(ChannelConf, ChannelInternal);	//[SWS_CanNm_00111]
			} else if (ChannelInternal->State == NM_STATE_NORMAL_OPERATION) {
				CanNm_Internal_NormalOperation_to_RepeatMessage(ChannelConf, ChannelInternal);
			} else {
				//Nothing to be done
			}
		}
		if (ChannelInternal->RemoteSleepInd) {
			ChannelInternal->RemoteSleepInd = FALSE;
			Nm_RemoteSleepCancellation(RxPduId);											//[SWS_CanNm_00151]
		} else if (ChannelInternal->RemoteSleepIndEnabled) {
			CanNm_Internal_TimerStart(&ChannelInternal->RemoteSleepIndTimer, ChannelConf->RemoteSleepIndTime);
		} else {
			//Nothing to be done
		}
	} else {
		//Nothing to be done	
	}

	if (ChannelInternal->BusLoadReduction) {
		CanNm_Internal_TimerStart(&ChannelInternal->MessageCycleTimer, ChannelConf->MsgReducedTime);	//[SWS_CanNm_00069]
	}

	if (CanNm_ConfigPtr->PduRxIndicationEnabled) {
		Nm_PduRxIndication(RxPduId);																	//[SWS_CanNm_00037]
	}
}

/** @brief CanNm_ConfirmPnAvailability [SWS_CanNm_00344]
 * 
 * Enables the PN filter functionality on the indicated NM channel.
 * Availability: The API is only available if CanNmGlobalPnSupport is TRUE.
 */
void CanNm_ConfirmPnAvailability(NetworkHandleType nmChannelHandle)
{
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[nmChannelHandle];

    if (CanNm_ConfigPtr->GlobalPnSupport) {
		ChannelInternal->NmPduFilterAlgorithm = TRUE;
	}
}

/** @brief CanNm_TriggerTransmit [SWS_CanNm_91001]
 * 
 * Within this API, the upper layer module (called module) shall check whether the 
 * available data fits into the buffer size reported by PduInfoPtr->SduLength.
 * If it fits, it shall copy its data into the buffer provided by PduInfoPtr->SduDataPtr 
 * and update the length of the actual copied data in PduInfoPtr->SduLength. 
 * If not, it returns E_NOT_OK without changing PduInfoPtr.
 */
Std_ReturnType CanNm_TriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr)
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[TxPduId];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[TxPduId];

	if (ChannelConf->TxPdu->TxPduRef->SduLength <= PduInfoPtr->SduLength) {
		memcpy(PduInfoPtr->SduDataPtr, ChannelConf->TxPdu->TxPduRef->SduDataPtr, ChannelConf->TxPdu->TxPduRef->SduLength);	//[SWS_CanNm_00351]
		PduInfoPtr->SduLength = ChannelConf->TxPdu->TxPduRef->SduLength;
		return E_OK;
	} else {
		return E_NOT_OK;
	}
}

/** @brief CanNm_MainFunction [SWS_CanNm_00234]
 * 
 * Main function of the CanNm.
 */
void CanNm_MainFunction(void)
{
	for (uint8 channel = 0; channel < CANNM_CHANNEL_COUNT; channel++) {
		CanNm_Internal_TimerTick(&CanNm_Internal.Channels[channel].TimeoutTimer, channel, CanNm_ConfigPtr->MainFunctionPeriod);			//[SWS_CanNm_00089]
		CanNm_Internal_TimerTick(&CanNm_Internal.Channels[channel].MessageCycleTimer, channel, CanNm_ConfigPtr->MainFunctionPeriod);
		CanNm_Internal_TimerTick(&CanNm_Internal.Channels[channel].RepeatMessageTimer, channel, CanNm_ConfigPtr->MainFunctionPeriod);
		CanNm_Internal_TimerTick(&CanNm_Internal.Channels[channel].WaitBusSleepTimer, channel, CanNm_ConfigPtr->MainFunctionPeriod);
		CanNm_Internal_TimerTick(&CanNm_Internal.Channels[channel].RepeatMessageTimer, channel, CanNm_ConfigPtr->MainFunctionPeriod);
	}
}

/*====================================================================================================================*\
    Local functions (static) code
\*====================================================================================================================*/

/*******************/
/* Timer functions */
/*******************/
static inline void CanNm_Internal_TimerStart( CanNm_Timer* Timer, uint32 timeoutValue )
{
	Timer->State = CANNM_TIMER_STARTED;
	Timer->TimeLeft = timeoutValue;	//[SWS_CanNm_00206]
}

static inline void CanNm_Internal_TimerResume( CanNm_Timer* Timer )
{
	Timer->State = CANNM_TIMER_STARTED;
}

static inline void CanNm_Internal_TimerStop( CanNm_Timer* Timer )
{
	Timer->State = CANNM_TIMER_STOPPED;
}

static inline void CanNm_Internal_TimerReset( CanNm_Timer* Timer, uint32 timeoutValue )
{
	Timer->State = CANNM_TIMER_STOPPED;
	Timer->TimeLeft = timeoutValue;
}

static inline void CanNm_Internal_TimerTick( CanNm_Timer* Timer, const uint8 channel, const float32 period )
{
	if (Timer->State == CANNM_TIMER_STARTED) {
		if (period >= Timer->TimeLeft) {
			CanNm_Internal_TimerStop(Timer);
			Timer->ExpiredCallback(Timer, channel);
		} else {
			Timer->TimeLeft -= period;
		}
	}
	else {
		//Nothing to be done
	}
}

static inline void CanNm_Internal_TimersInit( uint8 channel )
{
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[channel];

	ChannelInternal->TimeoutTimer.Channel = channel;
	ChannelInternal->TimeoutTimer.ExpiredCallback = CanNm_Internal_TimeoutTimerExpiredCallback;
	ChannelInternal->TimeoutTimer.State = CANNM_TIMER_STOPPED;
	ChannelInternal->TimeoutTimer.TimeLeft = 0;

	ChannelInternal->MessageCycleTimer.Channel = channel;
	ChannelInternal->MessageCycleTimer.ExpiredCallback = CanNm_Internal_MessageCycleTimerExpiredCallback;
	ChannelInternal->MessageCycleTimer.State = CANNM_TIMER_STOPPED;
	ChannelInternal->MessageCycleTimer.TimeLeft = 0;

	ChannelInternal->RepeatMessageTimer.Channel = channel;
	ChannelInternal->RepeatMessageTimer.ExpiredCallback = CanNm_Internal_RepeatMessageTimerExpiredCallback;
	ChannelInternal->RepeatMessageTimer.State = CANNM_TIMER_STOPPED;
	ChannelInternal->RepeatMessageTimer.TimeLeft = 0;

	ChannelInternal->WaitBusSleepTimer.Channel = channel;
	ChannelInternal->WaitBusSleepTimer.ExpiredCallback = CanNm_Internal_WaitBusSleepTimerExpiredCallback;
	ChannelInternal->WaitBusSleepTimer.State = CANNM_TIMER_STOPPED;
	ChannelInternal->WaitBusSleepTimer.TimeLeft = 0;

	ChannelInternal->RemoteSleepIndTimer.Channel = channel;
	ChannelInternal->RemoteSleepIndTimer.ExpiredCallback = CanNm_Internal_RemoteSleepIndTimerExpiredCallback;
	ChannelInternal->RemoteSleepIndTimer.State = CANNM_TIMER_STOPPED;
	ChannelInternal->RemoteSleepIndTimer.TimeLeft = 0;
}

static inline void CanNm_Internal_TimeoutTimerExpiredCallback( void* Timer, const uint8 channel )
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[channel];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[channel];

	if (ChannelInternal->State == NM_STATE_REPEAT_MESSAGE) {
		Nm_TxTimeoutException(ChannelInternal->Channel);
		CanNm_Internal_TimerStart(&ChannelInternal->TimeoutTimer, ChannelConf->TimeoutTime);
	} else if (ChannelInternal->State == NM_STATE_NORMAL_OPERATION) {
		Nm_TxTimeoutException(ChannelInternal->Channel);
		CanNm_Internal_NormalOperation_to_NormalOperation(ChannelConf, ChannelInternal);
	} else if (ChannelInternal->State == NM_STATE_READY_SLEEP) {
		if (ChannelConf->ActiveWakeupBitEnabled) {
			CanNm_Internal_ClearPduCbvBit(ChannelConf, ACTIVE_WAKEUP_BIT);		  					//[SWS_CanNm_00402]
		}
		CanNm_Internal_ReadySleep_to_PrepareBusSleep(ChannelConf, ChannelInternal);					//[SWS_CanNm_00109]
	} else {
		//Nothing to be done
	}	
}

static inline void CanNm_Internal_MessageCycleTimerExpiredCallback( void* Timer, const uint8 channel )
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[channel];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[channel];
	Std_ReturnType txStatus = E_OK;
	static Std_ReturnType lastTxStatus;

	if ((ChannelInternal->State == NM_STATE_REPEAT_MESSAGE) || (ChannelInternal->State == NM_STATE_NORMAL_OPERATION)) {
		txStatus = CanNm_Internal_TransmitMessage(ChannelConf, ChannelInternal);					//[SWS_CanNm_00032][SWS_CanNm_00087]
		if (ChannelInternal->ImmediateTransmissions) {
			if (txStatus == E_NOT_OK) {
				if (lastTxStatus == E_NOT_OK) {
					ChannelInternal->ImmediateTransmissions = 0;
					CanNm_Internal_TimerStart((CanNm_Timer*)Timer, ChannelConf->MsgCycleTime);		//[SWS_CanNm_00335]
				} else {
					CanNm_Internal_TimerStart((CanNm_Timer*)Timer, 1);								//[SWS_CanNm_00335]
				}
			} else {
				CanNm_Internal_TimerStart((CanNm_Timer*)Timer, ChannelConf->ImmediateNmCycleTime);	//[SWS_CanNm_00334]
				ChannelInternal->ImmediateTransmissions--;
			}
		} else {
			CanNm_Internal_TimerStart((CanNm_Timer*)Timer, ChannelConf->MsgCycleTime);				//[SWS_CanNm_00040]
		}
	}
	lastTxStatus = txStatus;
}

static inline void CanNm_Internal_RepeatMessageTimerExpiredCallback( void* Timer, const uint8 channel )
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[channel];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[channel];

	if (ChannelInternal->State == NM_STATE_REPEAT_MESSAGE) {
		if (ChannelInternal->Requested) {
			CanNm_Internal_RepeatMessage_to_NormalOperation(ChannelConf, ChannelInternal);			//[SWS_CanNm_00103]
		} else {
			CanNm_Internal_RepeatMessage_to_ReadySleep(ChannelConf, ChannelInternal);				//[SWS_CanNm_00106]
		}
	}
}

static inline void CanNm_Internal_WaitBusSleepTimerExpiredCallback( void* Timer, const uint8 channel )
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[channel];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[channel];

	if (ChannelInternal->Mode == NM_MODE_PREPARE_BUS_SLEEP) {
		CanNm_Internal_PrepareBusSleep_to_BusSleep(ChannelConf, ChannelInternal);					//[SWS_CanNm_00088]
	}
}

static inline void CanNm_Internal_RemoteSleepIndTimerExpiredCallback( void* Timer, const uint8 channel )
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[channel];
	CanNm_Internal_ChannelType* ChannelInternal = &CanNm_Internal.Channels[channel];

	ChannelInternal->RemoteSleepInd = TRUE;
	Nm_RemoteSleepInd(channel);
	CanNm_Internal_TimerStart(Timer, ChannelConf->RemoteSleepIndTime);								//[SWS_CanNm_00150]
}

/***************************/
/* State machine functions */
/***************************/
static inline void CanNm_Internal_BusSleep_to_BusSleep( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	Nm_NetworkStartIndication(ChannelInternal->Channel);
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_BUS_SLEEP, NM_STATE_BUS_SLEEP);
	}
}

static inline void CanNm_Internal_BusSleep_to_RepeatMessage( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	ChannelInternal->Mode = NM_MODE_NETWORK;
	ChannelInternal->State = NM_STATE_REPEAT_MESSAGE;
	ChannelInternal->BusLoadReduction = FALSE;														//[SWS_CanNm_00156]
	CanNm_Internal_TimerStart(&ChannelInternal->TimeoutTimer, ChannelConf->TimeoutTime);			//[SWS_CanNm_00096]
	CanNm_Internal_TimerStart(&ChannelInternal->RepeatMessageTimer, ChannelConf->RepeatMessageTime);//[SWS_CanNm_00102]
	CanNm_Internal_TimerStart(&ChannelInternal->MessageCycleTimer, ChannelConf->MsgCycleOffset);	//[SWS_CanNm_00100]
	Nm_NetworkMode(ChannelInternal->Channel);														//[SWS_CanNm_00097]
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_BUS_SLEEP, NM_STATE_REPEAT_MESSAGE);
	}
}

static inline void CanNm_Internal_RepeatMessage_to_RepeatMessage( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	CanNm_Internal_TimerStart(&ChannelInternal->TimeoutTimer, ChannelConf->TimeoutTime);			//[SWS_CanNm_00101]
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_REPEAT_MESSAGE, NM_STATE_REPEAT_MESSAGE);
	}
}

static inline void CanNm_Internal_RepeatMessage_to_ReadySleep( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	ChannelInternal->Mode = NM_MODE_NETWORK;
	ChannelInternal->State = NM_STATE_READY_SLEEP;
	ChannelInternal->TxEnabled = FALSE;																//[SWS_CanNm_00108]
	if (ChannelConf->NodeDetectionEnabled) {
		CanNm_Internal_ClearPduCbv(ChannelConf, ChannelInternal);									//[SWS_CanNm_00107]
	}
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_REPEAT_MESSAGE, NM_STATE_READY_SLEEP);
	}
}

static inline void CanNm_Internal_RepeatMessage_to_NormalOperation( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	ChannelInternal->Mode = NM_MODE_NETWORK;
	ChannelInternal->State = NM_STATE_NORMAL_OPERATION;
	if (ChannelConf->BusLoadReductionActive) {
		ChannelInternal->BusLoadReduction = TRUE;													//[SWS_CanNm_00157]
	}
	if (ChannelConf->NodeDetectionEnabled) {
		CanNm_Internal_ClearPduCbv(ChannelConf, ChannelInternal);									//[SWS_CanNm_00107]
	}
	if (CanNm_ConfigPtr->RemoteSleepIndEnabled) {													//[SWS_CanNm_00149]
		CanNm_Internal_TimerStart(&ChannelInternal->RemoteSleepIndTimer, ChannelConf->RemoteSleepIndTime);
	}
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_REPEAT_MESSAGE, NM_STATE_NORMAL_OPERATION);
	}
}

static inline void CanNm_Internal_NormalOperation_to_RepeatMessage( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	ChannelInternal->Mode = NM_MODE_NETWORK;
	ChannelInternal->State = NM_STATE_REPEAT_MESSAGE;
	ChannelInternal->BusLoadReduction = FALSE;														//[SWS_CanNm_00156]
	CanNm_Internal_TimerStart(&ChannelInternal->RepeatMessageTimer, ChannelConf->RepeatMessageTime);//[SWS_CanNm_00102]
	CanNm_Internal_TimerStart(&ChannelInternal->MessageCycleTimer, ChannelConf->MsgCycleOffset);	//[SWS_CanNm_00100]
	if (ChannelInternal->RemoteSleepInd) {
		ChannelInternal->RemoteSleepInd = FALSE;
		Nm_RemoteSleepCancellation(ChannelInternal->Channel);										//[SWS_CanNm_00151]
	}
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_NORMAL_OPERATION, NM_STATE_REPEAT_MESSAGE);
	}
}

static inline void CanNm_Internal_NormalOperation_to_NormalOperation( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	CanNm_Internal_TimerStart(&ChannelInternal->TimeoutTimer, ChannelConf->TimeoutTime);			//[SWS_CanNm_00117]
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_NORMAL_OPERATION, NM_STATE_NORMAL_OPERATION);
	}
}

static inline void CanNm_Internal_NormalOperation_to_ReadySleep( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	ChannelInternal->Mode = NM_MODE_NETWORK;
	ChannelInternal->State = NM_STATE_READY_SLEEP;
	ChannelInternal->TxEnabled = FALSE;																//[SWS_CanNm_00108]
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_NORMAL_OPERATION, NM_STATE_READY_SLEEP);
	}
}

static inline void CanNm_Internal_ReadySleep_to_NormalOperation( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	ChannelInternal->Mode = NM_MODE_NETWORK;
	ChannelInternal->State = NM_STATE_NORMAL_OPERATION;
	if (!CanNm_ConfigPtr->PassiveModeEnabled) {
		ChannelInternal->TxEnabled = TRUE;
	}
	if (ChannelConf->BusLoadReductionActive) {
		ChannelInternal->BusLoadReduction = TRUE;													//[SWS_CanNm_00157]
	}
	CanNm_Internal_TimerStart(&ChannelInternal->MessageCycleTimer, ChannelConf->MsgCycleOffset);	//[SWS_CanNm_00006][SWS_CanNm_00116]
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_READY_SLEEP, NM_STATE_NORMAL_OPERATION);
	}
}

static inline void CanNm_Internal_ReadySleep_to_RepeatMessage( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	ChannelInternal->Mode = NM_MODE_NETWORK;
	ChannelInternal->State = NM_STATE_REPEAT_MESSAGE;
	if (!CanNm_ConfigPtr->PassiveModeEnabled) {
		ChannelInternal->TxEnabled = TRUE;
	}
	ChannelInternal->BusLoadReduction = FALSE;														//[SWS_CanNm_00156]
	CanNm_Internal_TimerStart(&ChannelInternal->RepeatMessageTimer, ChannelConf->RepeatMessageTime);//[SWS_CanNm_00102]
	CanNm_Internal_TimerStart(&ChannelInternal->MessageCycleTimer, ChannelConf->MsgCycleOffset);	//[SWS_CanNm_00100]
	if (ChannelInternal->RemoteSleepInd) {
		ChannelInternal->RemoteSleepInd = FALSE;
		Nm_RemoteSleepCancellation(ChannelInternal->Channel);										//[SWS_CanNm_00151]
	}
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_READY_SLEEP, NM_STATE_REPEAT_MESSAGE);
	}
}

static inline void CanNm_Internal_ReadySleep_to_PrepareBusSleep( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal ) {
	ChannelInternal->Mode = NM_MODE_PREPARE_BUS_SLEEP;
	ChannelInternal->State = NM_STATE_PREPARE_BUS_SLEEP;
	CanNm_Internal_TimerStart(&ChannelInternal->WaitBusSleepTimer, ChannelConf->WaitBusSleepTime);	//[SWS_CanNm_00115]
	Nm_PrepareBusSleepMode(ChannelInternal->Channel);												//[SWS_CanNm_00114]
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_READY_SLEEP, NM_STATE_PREPARE_BUS_SLEEP);
	}
}

static inline void CanNm_Internal_PrepareBusSleep_to_RepeatMessage( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	ChannelInternal->Mode = NM_MODE_NETWORK;
	ChannelInternal->State = NM_STATE_REPEAT_MESSAGE;
	ChannelInternal->BusLoadReduction = FALSE;														//[SWS_CanNm_00156]
	CanNm_Internal_TimerStart(&ChannelInternal->TimeoutTimer, ChannelConf->TimeoutTime);			//[SWS_CanNm_00096]
	CanNm_Internal_TimerStart(&ChannelInternal->RepeatMessageTimer, ChannelConf->RepeatMessageTime);//[SWS_CanNm_00102]
	CanNm_Internal_TimerStart(&ChannelInternal->MessageCycleTimer, ChannelConf->MsgCycleOffset);	//[SWS_CanNm_00100]
	Nm_NetworkMode(ChannelInternal->Channel);														//[SWS_CanNm_00097]
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_PREPARE_BUS_SLEEP, NM_STATE_REPEAT_MESSAGE);
	}
}

static inline void CanNm_Internal_PrepareBusSleep_to_BusSleep( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	ChannelInternal->Mode = NM_MODE_BUS_SLEEP;
	ChannelInternal->State = NM_STATE_BUS_SLEEP;
	Nm_BusSleepMode(ChannelInternal->Channel);														//[SWS_CanNm_00126]
	if (CanNm_ConfigPtr->StateChangeIndEnabled) {
		Nm_StateChangeNotification(ChannelInternal->Channel, NM_STATE_PREPARE_BUS_SLEEP, NM_STATE_BUS_SLEEP);
	}
}

static inline void CanNm_Internal_NetworkMode_to_NetworkMode( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	CanNm_Internal_TimerStart(&ChannelInternal->TimeoutTimer, ChannelConf->TimeoutTime);			//[SWS_CanNm_00098][SWS_CanNm_00099]
}

/************************/
/* Additional functions */
/************************/
static inline Std_ReturnType CanNm_Internal_TxDisable( CanNm_Internal_ChannelType* ChannelInternal )
{
	ChannelInternal->TxEnabled = FALSE;
	if (CanNm_ConfigPtr->RemoteSleepIndEnabled) {
		ChannelInternal->RemoteSleepIndEnabled = FALSE;												//[SWS_CanNm_00175]
		CanNm_Internal_TimerStop(&ChannelInternal->RemoteSleepIndTimer);
	}								
	CanNm_Internal_TimerStop(&ChannelInternal->MessageCycleTimer);									//[SWS_CanNm_00051][SWS_CanNm_00173]
	CanNm_Internal_TimerStop(&ChannelInternal->TimeoutTimer);										//[SWS_CanNm_00174]
	return E_OK;
}

static inline Std_ReturnType CanNm_Internal_TxEnable( CanNm_Internal_ChannelType* ChannelInternal )
{
	const CanNm_ChannelType* ChannelConf = CanNm_ConfigPtr->ChannelConfig[ChannelInternal->Channel];

	if (!CanNm_ConfigPtr->PassiveModeEnabled) {
		ChannelInternal->TxEnabled = TRUE;															//[SWS_CanNm_00237]
		if (CanNm_ConfigPtr->RemoteSleepIndEnabled) {
			ChannelInternal->RemoteSleepIndEnabled = TRUE;											//[SWS_CanNm_00180]
			CanNm_Internal_TimerStart(&ChannelInternal->RemoteSleepIndTimer, ChannelConf->RemoteSleepIndTime);
		}											
		CanNm_Internal_TimerStart(&ChannelInternal->MessageCycleTimer, 1);							//[SWS_CanNm_00178]
		return E_OK;
	} else {
		return E_NOT_OK;
	}
}

static inline Std_ReturnType CanNm_Internal_TransmitMessage( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	if (ChannelInternal->TxEnabled) {
		return CanIf_Transmit(ChannelConf->TxPdu->TxConfirmationPduId, ChannelConf->TxPdu->TxPduRef);	//[SWS_CanNm_00032]
	} else {
		return E_OK;
	}
}

static inline void CanNm_Internal_SetPduCbvBit( const CanNm_ChannelType* ChannelConf, const uint8 PduCbvBitPosition )
{
	ChannelConf->TxPdu->TxPduRef->SduDataPtr[ChannelConf->PduCbvPosition] |= (1 << PduCbvBitPosition);
}

static inline void CanNm_Internal_ClearPduCbvBit( const CanNm_ChannelType* ChannelConf, const uint8 PduCbvBitPosition )
{
	ChannelConf->TxPdu->TxPduRef->SduDataPtr[ChannelConf->PduCbvPosition] &= ~(1 << PduCbvBitPosition);
}

static inline void CanNm_Internal_ClearPduCbv( const CanNm_ChannelType* ChannelConf, CanNm_Internal_ChannelType* ChannelInternal )
{
	if (ChannelConf->PduCbvPosition != CANNM_PDU_OFF) {
		ChannelConf->TxPdu->TxPduRef->SduDataPtr[ChannelConf->PduCbvPosition] = 0x00;
	}
}

static inline uint8 CanNm_Internal_GetUserDataOffset( const CanNm_ChannelType* ChannelConf )
{
	uint8 userDataPos = 0;
	userDataPos += (ChannelConf->PduNidPosition == CANNM_PDU_OFF) ? 0 : 1;
	userDataPos += (ChannelConf->PduCbvPosition == CANNM_PDU_OFF) ? 0 : 1;
	return userDataPos;
}

static inline uint8* CanNm_Internal_GetUserDataPtr( const CanNm_ChannelType* ChannelConf, uint8* MessageSduPtr )
{
	uint8 userDataOffset = CanNm_Internal_GetUserDataOffset(ChannelConf);
	return &MessageSduPtr[userDataOffset];
}

static inline uint8 CanNm_Internal_GetUserDataLength( const CanNm_ChannelType* ChannelConf )
{
	uint8 userDataOffset = CanNm_Internal_GetUserDataOffset(ChannelConf);
	return ChannelConf->UserDataTxPdu->TxUserDataPduRef->SduLength - userDataOffset;
}
