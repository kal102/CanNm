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
#ifndef CANNM_H
#define CANNM_H

/**===================================================================================================================*\
  @file CanNm.h

  @brief Can Network Management Module
  
  Implementation of Can Network Managment Module
\*====================================================================================================================*/

/*====================================================================================================================*\
    Include headers [SWS_CanNm_00245]
\*====================================================================================================================*/
#include "Std_Types.h"

/* [SWS_CanNm_00305] */
#include "ComStack_Types.h"

/* [SWS_CanNm_00309] */
#include "NmStack_Types.h"

//#include "CanNm_Cfg.h

/*====================================================================================================================*\
    Local macros Makra globalne
\*====================================================================================================================*/
#ifndef CANNM_CHANNEL_COUNT
#define CANNM_CHANNEL_COUNT 1
#endif

#ifndef CANNM_RXPDU_MAX_COUNT
#define CANNM_RXPDU_MAX_COUNT 128
#endif

/*====================================================================================================================*\
    Global types
\*====================================================================================================================*/
typedef struct {
	PduIdType	 RxPduId;
	PduInfoType* RxPduRef;
} CanNm_RxPdu;

typedef struct {
	PduIdType	 TxConfirmationPduId;
	PduInfoType* TxPduRef;
} CanNm_TxPdu;

typedef struct {
	PduIdType	 TxUserDataPduId;
	PduInfoType* TxUserDataPduRef;
} CanNm_UserDataTxPdu;

typedef enum {
	CANNM_PDU_BYTE_0 = 0x00,
	CANNM_PDU_BYTE_1 = 0x01,
	CANNM_PDU_OFF = 0xFF
} CanNm_PduBytePositionType;	//[SWS_CanNm_00074][SWS_CanNm_00075]

typedef struct {
	uint8 PnFilterMaskByteIndex;
	uint8 PnFilterMaskByteValue;
} CanNm_PnFilterMaskByte;

typedef struct {
	boolean						ActiveWakeupBitEnabled;
	boolean						AllNmMessagesKeepAwake;
	boolean 					BusLoadReductionActive;
	uint8						CarWakeUpBitPosition;
	boolean						CarWakeUpFilterEnabled;
	uint8						CarWakeUpFilterNodeId;
	boolean						CarWakeUpRxEnabled;
	float32						ImmediateNmCycleTime;
	uint8						ImmediateNmTransmissions;
	float32						MsgCycleOffset;
	float32						MsgCycleTime;
	float32						MsgReducedTime;
	float32						MsgTimeoutTime;
	boolean						NodeDetectionEnabled;
	uint8						NodeId;
	boolean						NodeIdEnabled;
	CanNm_PduBytePositionType	PduCbvPosition;
	CanNm_PduBytePositionType	PduNidPosition;
	boolean						PnEnabled;
	boolean						PnEraCalcEnabled;
	boolean						PnHandleMultipleNetworkRequests;
	float32						RemoteSleepIndTime;
	float32						RepeatMessageTime;
	boolean						RepeatMsgIndEnabled;
	CanNm_RxPdu*				RxPdu[CANNM_RXPDU_MAX_COUNT];
	float32						TimeoutTime;
	CanNm_TxPdu*				TxPdu;
	CanNm_UserDataTxPdu*		UserDataTxPdu;
	float32						WaitBusSleepTime;
	NetworkHandleType			ComMNetworkHandleRef;
	PduInfoType					PnEraRxNSduRef;
} CanNm_ChannelType;

typedef struct {
	const uint8 					PnInfoLength;
	const uint8 					PnInfoOffset;
	const CanNm_PnFilterMaskByte* 	PnFilterMaskByte;
} CanNm_PnInfo;

/** @brief CanNm_ConfigType [SWS_CanNm_00447]
 * 
 * This type shall contain at least all parameters that are post-build able according to chapter 10.
 */
typedef struct {
	boolean				BusLoadReductionEnabled;
	boolean				BusSynchronizationEnabled;
	CanNm_ChannelType*	ChannelConfig[CANNM_CHANNEL_COUNT];
	boolean				ComControlEnabled;
	boolean				ComUserDataSupport;
	boolean				CoordinationSyncSupport;
	boolean				DevErrorDetect;
	boolean				GlobalPnSupport;
	boolean				ImmediateRestartEnabled;
	boolean				ImmediateTxConfEnabled;				//[SWS_CanNm_00071]
	float32				MainFunctionPeriod;
	boolean				PassiveModeEnabled;
	boolean				PduRxIndicationEnabled;
	boolean				PnEiraCalcEnabled;
	CanNm_PnInfo*		PnInfo;
	float32				PnResetTime;
	boolean				RemoteSleepIndEnabled;
	boolean				StateChangeIndEnabled;
	boolean				UserDataEnabled;
	boolean				VersionInfoApi;
	PduInfoType*		PnEiraRxNSduRef;
} CanNm_ConfigType;

/*====================================================================================================================*\
    Global variables export
\*====================================================================================================================*/

/*====================================================================================================================*\
    Global functions declarations
\*====================================================================================================================*/

/*====================================================================================================================*\
    Global inline functions and function macros code
\*====================================================================================================================*/
void CanNm_Init(const CanNm_ConfigType* cannmConfigPtr);
void CanNm_DeInit(void);
Std_ReturnType CanNm_PassiveStartUp(NetworkHandleType nmChannelHandle);
Std_ReturnType CanNm_NetworkRequest(NetworkHandleType nmChannelHandle);
Std_ReturnType CanNm_NetworkRelease(NetworkHandleType nmChannelHandle);
Std_ReturnType CanNm_DisableCommunication(NetworkHandleType nmChannelHandle);
Std_ReturnType CanNm_EnableCommunication(NetworkHandleType nmChannelHandle);
Std_ReturnType CanNm_SetUserData(NetworkHandleType nmChannelHandle, const uint8* nmUserDataPtr);
Std_ReturnType CanNm_GetUserData(NetworkHandleType nmChannelHandle, uint8* nmUserDataPtr);
Std_ReturnType CanNm_Transmit(PduIdType TxPduId, const PduInfoType* PduInfoPtr);
Std_ReturnType CanNm_GetNodeIdentifier(NetworkHandleType nmChannelHandle, uint8*nmNodeIdPtr);
Std_ReturnType CanNm_GetLocalNodeIdentifier(NetworkHandleType nmChannelHandle, uint8* nmNodeIdPtr);
Std_ReturnType CanNm_RepeatMessageRequest(NetworkHandleType nmChannelHandle);
Std_ReturnType CanNm_GetPduData(NetworkHandleType nmChannelHandle, uint8* nmPduDataPtr);
Std_ReturnType CanNm_GetState(NetworkHandleType nmChannelHandle, Nm_StateType* nmStatePtr, Nm_ModeType* nmModePtr);
void CanNm_GetVersionInfo(Std_VersionInfoType* versioninfo);
Std_ReturnType CanNm_RequestBusSynchronization(NetworkHandleType nmChannelHandle);
Std_ReturnType CanNm_CheckRemoteSleepIndication(NetworkHandleType nmChannelHandle, boolean* nmRemoteSleepIndPtr);
Std_ReturnType CanNm_SetSleepReadyBit(NetworkHandleType nmChannelHandle,boolean nmSleepReadyBit);

void CanNm_TxConfirmation(PduIdType TxPduId, Std_ReturnType result);
void CanNm_RxIndication(PduIdType RxPduId, const PduInfoType* PduInfoPtr);
void CanNm_ConfirmPnAvailability(NetworkHandleType nmChannelHandle);
Std_ReturnType CanNm_TriggerTransmit(PduIdType TxPduId, PduInfoType* PduInfoPtr);

#endif /* CANNM_H */
