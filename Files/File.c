/**file.c********************************************************************************************
	Implementation of functions for support of the A/D conversion peripheral on MSP4305(6)xxx mcus
	TODO:  as per datasheet minimum sampling period for the temperature diode is 100 uS.  This is not
	currently handled and the measurement is probably wrong
***************************************************************************************************/
#include "../file/file.h"

//==Local Definitions===============================================================================
#define DEFINITION

//==Public Global Data==============================================================================
BoolType AdcSmplOV;							// >=1 sample was overwritten in the peripheral buffer
bool LBoolean;
BoolType AdcSbufOV;							// sensor buffer overwrote old value
UINT8 AdcSBufMaxDelay;					// track overflow of delayed samples for comb filter
const UINT16 AdcRefScaling[5] =		// select appropriate ADSbuUb based on reference selection
{
	384,			// 1.5 V reference buU/bit
	512,			// 2.0 V reference buU/bit
	640,			// 2.5 V reference buU/bit
	845,			// 3.3 V reference buU/bit
	1				// external reference buU/bit
};
UINT8 AdcSampleAvailableTsk;		// allow signalling of alternate task (for BIT)
UINT8 AdcSampleAvailableSig;

//==Public Function Prototypes======================================================================
void AdcInit( void );

//==Private Global Data=============================================================================
// TODO add numchannels variable compilation
const AdcConfigStruct * const adcConfig[NUM_ADC_VCHANNELS] =	// Configuration Settings
{
	(AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC0_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC1_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC2_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC3_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC4_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC5_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC6_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC7_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC8_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC9_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC10_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC11_LABEL)
	, (AdcConfigStruct * )& ADC_GET_CONFIG(ADC_VC12_LABEL)
};

AdcSDataElementType adcSDataBuffer[ADC_SDATA_BUF_LENGTH];

//==Private Function Prototypes====================================================================
//::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::::
//==Public Function Implementation==================================================================
/**AdcInit******************************************************************************************
	Initialize peripheral resources
	- Tclk = 5 MHz (MODOSC) - sample and hold @16 = 3.2 uS (for scale samples)
	- Tclk = 5 MHz (MODOSC) - sample and hold @512 = 102 uS uS (for health samples inc. Tdiode)
	- 12 bit conversions take 2.6 uS
	Use TimerA0.CCR1 to trigger sampling cycle @ 4 kHz (T=250 uS)
***************************************************************************************************/
//#pragma optimize=none	// needed to ensure compiled
void AdcInit( void )
{
  adcSDataBufhead = 0;			// Sensor data buffer head index

	for(n=0; n<(NUM_SDATA_BUF_CHANNELS+1); n++,pMctlx++)
	{
		__no_operation();	// TODO: optimizer squashes the next statement without a nop here
		*pMctlx = (UINT8)adcConfig[n]->adc12MCTLx;
	}

	ADC12MCTL8 |= ADC12EOS;
	ADC12IE = 0x01 << 8;	//interrupt when sequence finishes

	TA0IV = 0;
	TA0CTL = TASSEL__ACLK|ID__1|MC__STOP;					// ACLK/1=256 kHz;
	TA0CCTL0 = OUTMOD_3;												// PWM set@CCR1/reset@CCR0
	TA0CCTL1 = OUTMOD_3;												// PWM set@CCR1/reset@CCR0
	//TA0CCTL0 |= CCIE;												// TODO: only for debug
	//TA0CCTL1 |= CCIE;												// TODO: only for debug
	TA0CCR0 = FACLK/ADC_SAMPLE_RATE-1;					// 256 kHz/4000 = 64
  TA0CCR1 = TA0CCR0>>1;												// set output at 1/2 way point

  ADC12CTL0 |= (ADC12ON|ADC12ENC);
	TA0CTL |= MC__UP;														// turn on timer; count up to CCR0,reset,repeat
}

//==ISR Function Implementation=====================================================================
/**AdcSampleReadyISR********************************************************************************
	Copy sampled data to RamBuffers.  Expunge oldest sample on wrap
	- measured at 34 uS - Feb. 9, 2019 - SSM(2) RevA0
***************************************************************************************************/
//#pragma optimize=none	// needed to ensure compiled
//==Task Handler Implementation=====================================================================
//==Private Function Implementation=================================================================
/**AdcGetHealthData*********************************************************************************
	return a pointer to the health data buffer.  Current implementation is for a queue length of 1
***************************************************************************************************/
/**AdcGetSensorData*********************************************************************************
	return a pointer to the sensor data buffer "delay" samples old.
	Advance(and wrap) ring buffer pointer if necessary
	if advance requested but pointers are equal return null (empty)
***************************************************************************************************/
AdcSDataElementType* AdcGetSensorData(UINT8 delay, BoolType Advance)
{
	if (Advance)
	{
		if(adcSDataBuftail==adcSDataBufhead)		return NULL;	// return NULL if buffer empty
		adcSDataBuftail = ((adcSDataBuftail+1)&(ADC_SDATA_BUF_LENGTH-1));
	}
	return &adcSDataBuffer[((adcSDataBuftail-delay)&(ADC_SDATA_BUF_LENGTH-1))];
}
