//PPC Version : V2.1.9.30
//PPL Source File Name : D:\Work\git_repo\App_Tracker\3_software\CD_ML307C-DC-CN_OpenCPU_Standard_1.0.4.2605181713_release\kernel\obj\kernel\common\ML307C\export\cm_export_func.ppp
//PPL Source File Name : kernel\\common\\ML307C\\export\\cm_export_func.c
typedef signed char int8_t ;
typedef signed short int int16_t ;
typedef signed int int32_t ;
typedef signed __int64 int64_t ;
typedef unsigned char uint8_t ;
typedef unsigned short int uint16_t ;
typedef unsigned int uint32_t ;
typedef unsigned __int64 uint64_t ;
typedef signed char int_least8_t ;
typedef signed short int int_least16_t ;
typedef signed int int_least32_t ;
typedef signed __int64 int_least64_t ;
typedef unsigned char uint_least8_t ;
typedef unsigned short int uint_least16_t ;
typedef unsigned int uint_least32_t ;
typedef unsigned __int64 uint_least64_t ;
typedef signed int int_fast8_t ;
typedef signed int int_fast16_t ;
typedef signed int int_fast32_t ;
typedef signed __int64 int_fast64_t ;
typedef unsigned int uint_fast8_t ;
typedef unsigned int uint_fast16_t ;
typedef unsigned int uint_fast32_t ;
typedef unsigned __int64 uint_fast64_t ;
typedef signed int intptr_t ;
typedef unsigned int uintptr_t ;
typedef signed long long intmax_t ;
typedef unsigned long long uintmax_t ;
typedef signed int ptrdiff_t ;
typedef unsigned int size_t ;
typedef unsigned short wchar_t ;
typedef long double max_align_t ;
typedef void ( *osThreadFunc_t ) ( void *argument ) ;
typedef void ( *osTimerFunc_t ) ( void *argument ) ;
typedef void *osThreadId_t ;
typedef void *osTimerId_t ;
typedef void *osEventFlagsId_t ;
typedef void *osMutexId_t ;
typedef void *osSemaphoreId_t ;
typedef void *osMemoryPoolId_t ;
typedef void *osMessageQueueId_t ;
typedef uint32_t TZ_ModuleId_t ;
typedef void ( *cm_uart_event_cb_t ) ( void *param , uint32_t evt ) ;
typedef unsigned int size_t ;
typedef unsigned int clock_t ;
typedef unsigned int time_t ;
typedef void ( *cm_rtc_alarm_cb ) ( void ) ;
typedef unsigned char BOOL ;
typedef unsigned char UINT8 ;
typedef unsigned short UINT16 ;
typedef unsigned long UINT32 ;
typedef char CHAR ;
typedef signed char INT8 ;
typedef signed short INT16 ;
typedef signed long INT32 ;
typedef unsigned char Bool ;
typedef UINT8 BYTE ;
typedef UINT8 UBYTE ;
typedef UINT16 UWORD ;
typedef UINT16 WORD ;
typedef INT16 SWORD ;
typedef UINT32 DWORD ;
typedef unsigned long long UINT64 ;
typedef void* VOID_PTR ;
typedef volatile UINT8 *V_UINT8_PTR ;
typedef volatile UINT16 *V_UINT16_PTR ;
typedef volatile UINT32 *V_UINT32_PTR ;
typedef unsigned int U32Bits ;
typedef BOOL BOOLEAN ;
typedef const char * SwVersion ;
//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 PMU_POR = 1 ,	 
 PMU_EMR ,	 
 PMU_WDTR = ( PMU_EMR+2 )	 
 } PMU_LastResetStatus;

typedef UINT8 UART_Activity ;
//ICAT EXPORTED STRUCT 
 typedef struct /* This is structure of the UART Configuration */ 
 {	 
 UART_OpMode opMode ; /* fifo mode , non fifo mode or DMA for basic interface*/	 
 UART_TriggerLevel triggerLevel ; /* the trigger level interrupt on 1 , 8 , 16 , 32 */	 
 UART_BaudRates baudRate ; /* the rate of the transmit and the receive up to 111520 ( default - 9600 ) .*/	 
 UART_WordLen numDataBits ; /* 5 , 6 , 7 , or 8 number of data bits in the UART data frame ( default - 8 ) . */	 
 UART_StopBits stopBits ; /* 1 , 1.500000 or 2 stop bits in the UART data frame ( default - 1 ) . */	 
 UART_ParityTBits parityBitType ; /* Even , Odd or no-parity bit type in the UART data frame ( default - Non ) . */	 
 UART_InterfaceType interfaceType ; /* number of interface that the UART driver supplies ( default - UART_IF_TYPE_L2 ) */	 
 BOOL modemSignal ; /* enable operate modem - TRUE , disable modem - FALSE */	 
 BOOL flowControl ; /* enable Auto flow Control - TRUE , disable Auto flow Control - FALSE */	 
 UINT8 sleepMode ; /* enable sleep mode - TRUE , more fine control - see UARTSleepMode enum */	 
 BOOL auto_baud ; /* enable auto_baud , auto-baud-rate detection within the UART ( default - FALSE ) */	 
 UART_SIRConfigure sirIrDA ;	 
 } UARTConfiguration;

//ICAT EXPORTED ENUM 
 typedef enum // change the order -1 to + 
 {	 
 UART_RC_OK = 1 , /* 1 - no errors */	 
	 
 UART_RC_PORT_NUM_ERROR = -100 , /* -100 - Error in the UART port number */	 
 UART_RC_NO_DATA_TO_READ , /* -99 - Eror no data to read from the FIFO UART */	 
 UART_RC_ILLEGAL_BAUD_RATE , /* -98 - Error in the UART Bayd Rate */	 
 UART_RC_UART_PARITY_BITS_ERROR , /* -97 - Error in parity bit */	 
 UART_RC_UART_ONE_STOP_BIT_ERROR , /* -96 - Error in one stop bit */	 
 UART_RC_ONE_HALF_OR_TWO_STOP_BIT_ERROR , /* -95 - Error in two stop bit */	 
 UART_RC_BAD_INTERFACE_TYPE , /* -94 - Error in the Interface Type */	 
 UART_RC_UART_NOT_AVAILABLE , /* -93 - Error in try to open UART that is open */	 
 UART_RC_NO_DATA_TO_WRITE , /* -92 - Error No data to writ the len = 0 */	 
 UART_RC_NOT_ALL_BYTE_WRITTEN , /* -91 - Error Not all the Byte write to the UART FIFO */	 
 UART_RC_ISR_ALREADY_BIND , /* -90 - Error try to bind ISR for Basic Interface */	 
 UART_RC_WRONG_ISR_UNBIND , /* -89 - Error in the UnBind ISR for Basic Interface */	 
 UART_RC_FIFO_NOT_EMPTY , /* -88 - Error , the UART FIFO not empty */	 
 UART_RC_UART_OPEN , /* -87 - Error try chance the configurr when the UART open */	 
 UART_RC_GPIO_ERR , /* -86 - Error in the Configure of the GPIO */	 
 UART_RC_IRDA_CONFIG_ERR , /* -85 - Illegal IrDA configuration */	 
 UART_RC_TX_DMA_ERR /* -84 - DMA TX Error */	 
 } UART_ReturnCode;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 LOG_DISABLE = 0x0 ,	 
 UART_LOG_ENABLE = 0x1 ,	 
 ACAT_LOG_ENABLE = 0x2	 
 } Log_ConfigE;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 MSG_DISABLE = 0x0 ,	 
 ACAT_MSG_ENABLE = 0x1	 
 } Msg_ConfigE;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 RTI_LOG_DISABLE = 0x0 ,	 
 RTI_DUMP_ENABLE = 0x1 ,	 
 RTI_TASK_ENABLE = 0x2 ,	 
 RTI_MIPS_ENABLE = 0x3	 
 } RTI_ConfigE;

//ICAT EXPORTED STRUCT 
 typedef struct {	 
 Log_ConfigE log_cfg ;	 
 Msg_ConfigE msg_cfg ;	 
 RTI_ConfigE rti_cfg ;	 
 } Log_ConfigS;

typedef void ( *UARTNotifyInterrupt ) ( UART_Port ) ;
typedef void ( *UsbLogPrint_t ) ( const char * , ... ) ;
typedef unsigned int size_t ;
typedef va_list __gnuc_va_list ;
typedef unsigned int size_t ;
typedef char CHAR ;
typedef unsigned char UCHAR ;
typedef int INT ;
typedef unsigned int UINT ;
typedef long LONG ;
typedef unsigned long ULONG ;
typedef short SHORT ;
typedef unsigned short USHORT ;
//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 OSA_TASK_READY ,	 
 OSA_TASK_COMPLETED ,	 
 OSA_TASK_TERMINATED ,	 
 OSA_TASK_SUSPENDED ,	 
 OSA_TASK_SLEEP ,	 
 OSA_TASK_QUEUE_SUSP ,	 
 OSA_TASK_SEMAPHORE_SUSP ,	 
 OSA_TASK_EVENT_FLAG ,	 
 OSA_TASK_BLOCK_MEMORY ,	 
 OSA_TASK_MUTEX_SUSP ,	 
 OSA_TASK_STATE_UNKNOWN ,	 
 } OSA_TASK_STATE;

//ICAT EXPORTED STRUCT 
 typedef struct OSA_TASK_STRUCT 
 {	 
 char *task_name ; /* Pointer to thread ' s name */	 
 unsigned int task_priority ; /* Priority of thread ( 0 -255 ) */	 
 unsigned long task_stack_def_val ; /* default vaule of thread */	 
 OSA_TASK_STATE task_state ; /* Thread ' s execution state */	 
 unsigned long task_stack_ptr ; /* Thread ' s stack pointer */	 
 unsigned long task_stack_start ; /* Stack starting address */	 
 unsigned long task_stack_end ; /* Stack ending address */	 
 unsigned long task_stack_size ; /* Stack size */	 
 unsigned long task_run_count ; /* Thread ' s run counter */	 
	 
 } OSA_TASK;

typedef void *OsaRefT ;
typedef UINT8 OSA_STATUS ;
typedef UINT8 OS_STATUS ;
typedef void* OS_HISR ;
typedef void* OSATaskRef ;
typedef void* OSAHISRRef ;
typedef void* OSASemaRef ;
typedef void* OSAMutexRef ;
typedef void* OSAMsgQRef ;
typedef void* OSAMailboxQRef ;
typedef void* OSAPoolRef ;
typedef void* OSATimerRef ;
typedef void* OSAFlagRef ;
typedef void* OSAPartitionPoolRef ;
typedef void* OSTaskRef ;
typedef void* OSSemaRef ;
typedef void* OSMutexRef ;
typedef void* OSMsgQRef ;
typedef void* OSMailboxQRef ;
typedef void* OSPoolRef ;
typedef void* OSTimerRef ;
typedef void* OSFlagRef ;
typedef UINT8 OS_STATUS ;
typedef OsaTimerStatusParamsT OSATimerStatus ;
typedef void* OSATaskRef ;
typedef void* OSAHISRRef ;
typedef void* OSAMsgQRef ;
typedef void* OSAMailboxQRef ;
typedef void* OSAPartitionPoolRef ;
typedef UINT8 OS_STATUS ;
typedef unsigned long UNSIGNED ;
typedef long SIGNED ;
typedef unsigned char DATA_ELEMENT ;
typedef DATA_ELEMENT OPTION ;
typedef DATA_ELEMENT BOOLEAN ;
typedef int STATUS ;
typedef unsigned char UNSIGNED_CHAR ;
typedef unsigned int UNSIGNED_INT ;
typedef int INT ;
typedef unsigned long * UNSIGNED_PTR ;
typedef unsigned char * BYTE_PTR ;
typedef void ( *CommandAddress ) ( void ) ;
typedef char* CommandProto ;
typedef const char * DiagDBVersion ;
//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 PROTOCOL_TYPE_0 = 0 ,	 
 MAX_PROTOCOL_TYPES	 
 } ProtocolType;

//ICAT EXPORTED STRUCT 
 typedef struct 
 {	 
 BOOL bEnabled ; // enable / disable the trace logging feature	 
 ProtocolType eProtocolType ; // protocol type for communication with ICAT , currently only protocol type 0 is supported	 
 UINT16 nMaxDataPerTrace ; // for each trace , what is the maximum data length to accompany the trace , in protocol type 0 , this is relevant only to DSP messages	 
 } DiagLoggerDefs;

typedef void ( *TIMER_CALLBACK_FUNCTION ) ( UINT8 ) ;
typedef void ( *ACC_TIMER_CALLBACK ) ( UINT32 ) ;
typedef int TIMER_STATUS ;
typedef int TIMER_ID ;
//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 PM_RC_OK = 0 ,	 
 PM_RC_FAIL , // General Failure	 
 PM_RC_ALREADY_EXISTS // Exit function since required target alrteady exists	 
 } PM_ReturnCodeE;

typedef void ( *PM_CallbackFuncDDRstateT ) ( BOOL b_DDR_ready ) ;
typedef unsigned long long UINT64 ;
typedef unsigned long TimeIn32KhzUnit ;
typedef void ( *TickCallbackPtr ) ( UINT32 ) ;
typedef TimeIn32KhzUnit ( *SuspendCallbackPtr ) ( void ) ;
typedef void ( *PrepareTimeCallbackPtr ) ( void ) ;
//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 GPIO_PIN_NOT_ASSIGNED = -1 ,	 
	 
 GPIO_PIN_0=0 , GPIO_PIN_1 , GPIO_PIN_2 , GPIO_PIN_3 , GPIO_PIN_4 , GPIO_PIN_5 , GPIO_PIN_6 , GPIO_PIN_7 ,	 
 GPIO_PIN_8 , GPIO_PIN_9 , GPIO_PIN_10 , GPIO_PIN_11 , GPIO_PIN_12 , GPIO_PIN_13 , GPIO_PIN_14 , GPIO_PIN_15 ,	 
 GPIO_PIN_16 , GPIO_PIN_17 , GPIO_PIN_18 , GPIO_PIN_19 , GPIO_PIN_20 , GPIO_PIN_21 , GPIO_PIN_22 , GPIO_PIN_23 ,	 
 GPIO_PIN_24 , GPIO_PIN_25 , GPIO_PIN_26 , GPIO_PIN_27 , GPIO_PIN_28 , GPIO_PIN_29 , GPIO_PIN_30 , GPIO_PIN_31 ,	 
 GPIO_PIN_32 ,	 
	 
	 
	 
 GPIO_MAX_AMOUNT_OF_PINS	 
	 
	 
	 
 } GPIO_PinNumbers;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 GPIO_RC_OK = 1 ,	 
	 
 GPIO_RC_INVALID_PORT_HANDLE = -100 ,	 
 GPIO_RC_NOT_OUTPUT_PORT ,	 
 GPIO_RC_NO_TIMER ,	 
 GPIO_RC_NO_FREE_HANDLE ,	 
 GPIO_RC_AMOUNT_OUT_OF_RANGE ,	 
 GPIO_RC_INCORRECT_PORT_SIZE ,	 
 GPIO_RC_PORT_NOT_ON_ONE_REG ,	 
 GPIO_RC_INVALID_PIN_NUM ,	 
 GPIO_RC_PIN_USED_IN_PORT ,	 
 GPIO_RC_PIN_NOT_FREE ,	 
 GPIO_RC_PIN_NOT_LOCKED ,	 
 GPIO_RC_NULL_POINTER ,	 
 GPIO_RC_PULLED_AND_OUTPUT ,	 
 GPIO_RC_INCORRECT_PORT_TYPE ,	 
 GPIO_RC_INCORRECT_TRANSITION_TYPE ,	 
 GPIO_RC_INCORRECT_DEBOUNCE ,	 
 GPIO_RC_INCORRECT_DIRECTION ,	 
 GPIO_RC_INCORRECT_INIT_VALUE	 
	 
 , GPIO_RC_INTC_ERROR ,	 
 GPIO_RC_PRM_ERROR	 
	 
 } GPIO_ReturnCode;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 GPIO_INPUT_PIN = 1 ,	 
 GPIO_OUTPUT_PIN	 
 } GPIO_PinDirection;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 GPIO_PIN_FREE_FOR_USE = 0 ,	 
 GPIO_PIN_USE_IN_PORT ,	 
 GPIO_PIN_USE_IN_INTERRUPT ,	 
 GPIO_PIN_USE_IN_PORT_WITH_INTERRUPT ,	 
 GPIO_PIN_LOCKED	 
 } GPIO_PinUsage;

//ICAT EXPORTED STRUCT 
 typedef struct 
 {	 
 GPIO_PinUsage pinUsage ;	 
 GPIO_PinDirection direction ;	 
 } GPIO_PinStatus;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 GPIO_INITIAL_VALUE_NO_CHANGE = 0 ,	 
 GPIO_INITIAL_VALUE_LOW ,	 
 GPIO_INITIAL_VALUE_HIGH	 
 } GPIO_BitInitialValue;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 GPIO_PULL_UP_DOWN_DISABLE = 0 ,	 
 GPIO_PULL_UP_ENABLE ,	 
 GPIO_PULL_DOWN_ENABLE	 
 } GPIO_PullUpDown;

//ICAT EXPORTED STRUCT 
 typedef struct 
 {	 
 GPIO_PinNumbers pinNumber ;	 
 GPIO_PinDirection direction ;	 
 GPIO_TransitionType transitionType ;	 
 GPIO_Debounce debounce ;	 
 GPIO_PullUpDown pullUpDown ;	 
 GPIO_BitInitialValue initialValue ;	 
 } GPIO_PinConfiguration;

typedef UINT8 GPIO_PortHandle ;
typedef void ( *GPIO_ISR ) ( void ) ;
typedef UINT32 INTC_InterruptPriorityTable [ MAX_INTERRUPT_CONTROLLER_SOURCES ] ;
typedef UINT32 INTC_InterruptInfo ;
typedef void ( *INTC_ISR ) ( INTC_InterruptInfo interruptInfo ) ;
typedef void ( *PMCNotifyEventFunc ) ( UINT64 eventRegs ) ;
typedef void ( *PMCGetStatusNotifyFunc ) ( UINT16 status ) ;
typedef void ( *PMCReadCallback ) ( UINT8 *dataBuffPtr , UINT16 dataSize , UINT16 userId ) ;
typedef void ( *PMCWriteCallback ) ( UINT16 dataBuffPtr ) ;
typedef void ( *PMCGetGPADCValueNotifyFunc ) ( PMC_adc_reg_t reg , UINT16 value ) ;
typedef void ( * ReadingCallback ) ( int ) ;
typedef void ( * LTETempReadingCallback ) ( unsigned short , unsigned short ) ;
typedef void ( * ReadingCallbackBoth ) ( BOOL , int , int ) ;
typedef union
 {
 UINT8 autoControl ;
 UINT8 autoControl2 ;
 UINT8 manControl ;
 } adcModeCntrl_t ;
typedef union
 {
 UINT64 all ;
 Registers_ts regs ;
 } PMCEvents ;
//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 SHD_POWER_DOWN ,	 
 SHD_RESET ,	 
 SHD_GHOST ,	 
 SHD_SW_ERROR /* EEHandler triggered the reset */	 
 } ShutDownType_te;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 RR_NORMAL_POWER_ON = 0x00 , // default , not combined with others	 
 RR_WATCH_DOG_TIMEOUT = 0x01 ,	 
 RR_SOFTWARE_GENERATED = 0x02 ,	 
 RR_CHARGING_BATTERY = 0x04 ,	 
 RR_LOW_BATTERY = 0x08 ,	 
 RR_ALARM_POWER_ON = 0x10 ,	 
 RR_EXT_POWER_ON = 0x20	 
 } 
 StartupReason_te;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 RE_RTC_ALARM = 0x01	 
 } StartupExtInd_te;

typedef BOOL ( *DiagPSisRunningFn ) ( void ) ;
typedef UINT8 u8_t ;
typedef UINT16 u16_t ;
typedef UINT32 u32_t ;
typedef UINT64 u64_t ;
typedef INT8 s8_t ;
typedef INT16 s16_t ;
typedef INT32 s32_t ;
typedef void* msg_t ;
typedef u32_t mem_ptr_t ;
typedef unsigned long long uint64_t ;
typedef union {
 ip4_addr_t ip4 ;
 ip6_addr_t ip6 ;
 } ipX_addr_t ;
typedef s8_t err_t ;
typedef unsigned int size_t ;
typedef unsigned short wchar_t ;
typedef int ( *__heapprt ) ( void * , char const * , ... ) ;
typedef size_t mem_size_t ;
typedef UINT32 sys_prot_t ;
typedef void * sys_mutex_t ;
typedef void * sys_sem_t ;
typedef void * sys_mbox_t ;
typedef void * sys_thread_t ;
typedef void ( *eventCallback ) ( void *pArgs , int evt ) ;
typedef void ( *printCallback ) ( char *printstr ) ;
typedef void ( *lwip_thread_fn ) ( void *pArgs ) ;
typedef err_t ( *lwip_run_entry ) ( void *pArgs ) ;
typedef void ( * socket_callback ) ( int s , int evt , u16_t len ) ;
typedef u32_t socklen_t ;
typedef void ( *cm_eloop_event_cb ) ( cm_eloop_event_handle_t event , void *cb_param ) ;
typedef void ( *cm_asocket_event_cb ) ( int sock , cm_asocket_event_e event , void *cb_param ) ;
typedef void ( *cm_async_dns_event_cb ) ( int req_id , cm_async_dns_event_e event , void *cb_param ,
 const char *host_name , const cm_async_dns_ip_addr_t *ip_addr ) ;
typedef void ( *cm_fota_result_callback ) ( cm_fota_error_e error ) ;
typedef void *cm_httpclient_handle_t ;
typedef void ( *cm_httpclient_event_callback_func ) ( cm_httpclient_handle_t client_handle , cm_httpclient_callback_event_e event , void *param ) ;
typedef uint32_t cm_keypad_map_t ;
typedef void ( *cm_key_event_callback_t ) ( cm_keypad_map_t key , cm_keypad_event_e event ) ;
typedef void ( *cm_lbs_callback ) ( cm_lbs_callback_event_e event , cm_lbs_location_rsp_t *location , void *cb_arg ) ;
typedef void ( *cm_pm_event_cb_t ) ( void ) ;
typedef void ( *cm_pm_exit_cb_t ) ( uint32_t reason ) ;
typedef void ( *cm_pm_powerkey_cb_t ) ( cm_powerkey_event_e event ) ;
typedef void ( *cm_audio_player_stream_cb ) ( cm_audio_player_stream_event_e event ) ;
typedef void ( *cm_audio_play_callback_t ) ( cm_audio_play_event_e event , void *param ) ;
typedef void ( *cm_wifiscan_callback_t ) ( cm_wifi_scan_info_t *param , void *user_param ) ;
typedef void ( *cm_virt_at_cb ) ( cm_virt_at_param_t *param ) ;
typedef void ( *cm_virt_at_urc_cb ) ( uint8_t *urc , int32_t urc_len ) ;
typedef void ( *cm_usb2com_recv_cb ) ( void *data , int32_t len ) ;
typedef void ( *cm_usb2com_status_cb ) ( int32_t evt ) ;
typedef void *cm_lwm2m_handle_t ;
typedef void ( *cm_lwm2m_event_cb_t ) ( int32_t event , cm_lwm2m_cb_param_t param ) ;
typedef void ( *cm_lwm2m_notify_cb_t ) ( int32_t mid , cm_lwm2m_cb_param_t param ) ;
typedef void ( *cm_lwm2m_read_cb_t ) ( int32_t mid , int32_t objid , int32_t insid , int32_t resid , cm_lwm2m_cb_param_t param ) ;
typedef void ( *cm_lwm2m_write_cb_t ) ( int32_t mid , int32_t objid , int32_t insid , int32_t resid , int32_t type , int32_t is_over , char *data , int32_t len , cm_lwm2m_cb_param_t param ) ;
typedef void ( *cm_lwm2m_execute_cb_t ) ( int32_t mid , int32_t objid , int32_t insid , int32_t resid , char *data , int32_t len , cm_lwm2m_cb_param_t param ) ;
typedef void ( *cm_lwm2m_observe_cb_t ) ( int32_t mid , int32_t observe , int32_t objid , int32_t insid , int32_t resid , cm_lwm2m_cb_param_t param ) ;
typedef void ( *cm_lwm2m_discover_cb_t ) ( int32_t mid , int32_t objid , cm_lwm2m_cb_param_t param ) ;
typedef void ( *cm_lwm2m_params_cb_t ) ( int32_t mid , int32_t objid , int32_t insid , int32_t resid , char *parameter , int32_t len , cm_lwm2m_cb_param_t param ) ;
typedef void ( *cm_ping_callback_func ) ( cm_ping_cb_type_e type , cm_ping_cb_result_e result , cm_ping_reply_t *resp , void *user_data ) ;
typedef void* ( *alloc_fn ) ( size_t size ) ;
typedef void ( *free_fn ) ( pmsg* msg ) ;
typedef signed short INT16 ;
typedef signed long INT32 ;
typedef unsigned char UINT8 ;
typedef unsigned short UINT16 ;
typedef unsigned long UINT32 ;
typedef unsigned char BOOL ;
typedef unsigned long long UINT64 ;
typedef int INT ;
//ICAT EXPORTED ENUM 
 typedef enum {	 
 HEAD_PHONE_INIT = 0 ,	 
 HEAD_PHONE_OUT , // 1	 
 HEAD_TYPE_UNKNOWN , // 2	 
 HEAD_PHONE_TYPE3 , // 3	 
 HEAD_PHONE_TYPE4 , // 4	 
 HEAD_PHONE_TYPE_C_REVERSE , // 5	 
 HEAD_PHONE_TYPE_C , // 6	 
	 
 HEAD_PHONE_ERROR = 0xFFFF	 
 } HEADPHONE_STATE;

//ICAT EXPORTED ENUM 
 typedef enum {	 
 HOOK_KEY_EVENT_NULL = 0 ,	 
 VOL_UP_PRESS , // 1	 
 VOL_DOWN_PRESS , // 2	 
 HOOK_KEY_PRESS , // 3	 
 VOL_UP_RELEASE , // 4	 
 VOL_DOWN_RELEASE , // 5	 
 HOOK_KEY_RELEASE , // 6	 
	 
 HEADPHONE_EVENT_ERROR = 0xFFFF	 
 } HEADPHONE_EVENT;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_ERR_NO = 0 , // No error	 
 AUDIOHAL_ERR_RESOURCE_RESET ,	 
 AUDIOHAL_ERR_RESOURCE_BUSY ,	 
 AUDIOHAL_ERR_RESOURCE_TIMEOUT ,	 
 AUDIOHAL_ERR_RESOURCE_NOT_ENABLED ,	 
 AUDIOHAL_ERR_BAD_PARAMETER ,	 
	 
 AUDIOHAL_ERR_UART_RX_OVERFLOW ,	 
 AUDIOHAL_ERR_UART_TX_OVERFLOW ,	 
 AUDIOHAL_ERR_UART_PARITY ,	 
 AUDIOHAL_ERR_UART_FRAMING ,	 
 AUDIOHAL_ERR_UART_BREAK_INT ,	 
	 
 AUDIOHAL_ERR_TIM_RTC_NOT_VALID ,	 
 AUDIOHAL_ERR_TIM_RTC_ALARM_NOT_ENABLED ,	 
 AUDIOHAL_ERR_TIM_RTC_ALARM_NOT_DISABLED ,	 
	 
 AUDIOHAL_ERR_COMMUNICATION_FAILED ,	 
	 
 /* Must be at the end */	 
 AUDIOHAL_ERR_QTY ,	 
	 
	 
 AUDIOHAL_ERR_ENUM_32_BIT = 0x7FFFFFFF // 32 bit enum compiling enforcement	 
 } AUDIOHAL_ERR_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_ITF_RECEIVER = 0 ,	 
 AUDIOHAL_ITF_EARPIECE ,	 
 AUDIOHAL_ITF_HEADPHONE = AUDIOHAL_ITF_EARPIECE ,	 
 AUDIOHAL_ITF_LOUDSPEAKER ,	 
 AUDIOHAL_ITF_LOUDSPEAKER_AND_HEADPHONE ,	 
 // AUDIOHAL_ITF_LOUDSPEAKER_AND_HEADPHONE = AUDIOHAL_ITF_LOUDSPEAKER_AND_EARPIECE ,	 
 AUDIOHAL_ITF_BLUETOOTH ,	 
 AUDIOHAL_ITF_FM ,	 
 AUDIOHAL_ITF_FM2SPK ,	 
 AUDIOHAL_ITF_TV ,	 
 AUDIOHAL_ITF_BLUETOOTH_WB ,	 
 AUDIOHAL_ITF_HS_LEFT ,	 
 AUDIOHAL_ITF_HS_RIGHT ,	 
 AUDIOHAL_ITF_VAD ,	 
 AUDIOHAL_ITF_FM2RCV ,	 
	 
 AUDIOHAL_ITF_QTY ,	 
 AUDIOHAL_ITF_NONE = 255 ,	 
 } AUDIOHAL_ITF_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_SPK_RECEIVER = 0 ,	 
 AUDIOHAL_SPK_EARPIECE ,	 
 AUDIOHAL_SPK_LOUDSPEAKER ,	 
 AUDIOHAL_SPK_LOUDSPEAKER_EARPIECE , // Output on both hands-free loud speaker and earpiece	 
	 
 AUDIOHAL_SPK_QTY ,	 
 AUDIOHAL_SPK_DISABLE = 255 ,	 
 } AUDIOHAL_SPK_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_SPEAKER_STEREO = 0 ,	 
 AUDIOHAL_SPEAKER_MONO_RIGHT ,	 
 AUDIOHAL_SPEAKER_MONO_LEFT ,	 
 AUDIOHAL_SPEAKER_STEREO_NA , // Output is mono only	 
	 
 AUDIOHAL_SPEAKER_QTY ,	 
 AUDIOHAL_SPEAKER_DISABLE = 255 ,	 
 } AUDIOHAL_SPEAKER_TYPE_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_MIC_RECEIVER = 0 ,	 
 AUDIOHAL_MIC_EARPIECE ,	 
 AUDIOHAL_MIC_LOUDSPEAKER ,	 
	 
 AUDIOHAL_MIC_QTY ,	 
 AUDIOHAL_MIC_DISABLE = 255 ,	 
 } AUDIOHAL_MIC_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_SPK_MUTE = 0 ,	 
 AUDIOHAL_SPK_VOL_1 ,	 
 AUDIOHAL_SPK_VOL_2 ,	 
 AUDIOHAL_SPK_VOL_3 ,	 
 AUDIOHAL_SPK_VOL_4 ,	 
 AUDIOHAL_SPK_VOL_5 ,	 
 AUDIOHAL_SPK_VOL_6 ,	 
 AUDIOHAL_SPK_VOL_7 ,	 
 AUDIOHAL_SPK_VOL_8 ,	 
 AUDIOHAL_SPK_VOL_9 ,	 
 AUDIOHAL_SPK_VOL_10 ,	 
 AUDIOHAL_SPK_VOL_11 , // 11	 
	 
 AUDIOHAL_SPK_VOL_QTY // 12	 
 } AUDIOHAL_SPK_LEVEL_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_MIC_MUTE = 0 ,	 
 AUDIOHAL_MIC_ENABLE ,	 
	 
 AUDIOHAL_MIC_VOL_QTY ,	 
 } AUDIOHAL_MIC_LEVEL_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_SIDE_MUTE = 0 ,	 
 AUDIOHAL_SIDE_VOL_1 ,	 
 AUDIOHAL_SIDE_VOL_2 ,	 
 AUDIOHAL_SIDE_VOL_3 ,	 
 AUDIOHAL_SIDE_VOL_4 ,	 
 AUDIOHAL_SIDE_VOL_5 ,	 
 AUDIOHAL_SIDE_VOL_6 ,	 
 AUDIOHAL_SIDE_VOL_7 ,	 
 AUDIOHAL_SIDE_VOL_TEST ,	 
 AUDIOHAL_SIDE_VOL_QTY ,	 
 } AUDIOHAL_SIDE_LEVEL_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_FREQ_8000HZ = 8000 ,	 
 AUDIOHAL_FREQ_11025HZ = 11025 ,	 
 AUDIOHAL_FREQ_12000HZ = 12000 ,	 
 AUDIOHAL_FREQ_16000HZ = 16000 ,	 
 AUDIOHAL_FREQ_22050HZ = 22050 ,	 
 AUDIOHAL_FREQ_24000HZ = 24000 ,	 
 AUDIOHAL_FREQ_32000HZ = 32000 ,	 
 AUDIOHAL_FREQ_44100HZ = 44100 ,	 
 AUDIOHAL_FREQ_48000HZ = 48000 ,	 
 AUDIOHAL_FREQ_64000HZ = 64000 ,	 
 AUDIOHAL_FREQ_88200HZ = 88200 ,	 
 AUDIOHAL_FREQ_96000HZ = 96000 ,	 
 } AUDIOHAL_FREQ_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_MONO = 1 ,	 
 AUDIOHAL_STEREO = 2 ,	 
 } AUDIOHAL_CH_NB_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_DTMF_0 = 0 ,	 
 AUDIOHAL_DTMF_1 ,	 
 AUDIOHAL_DTMF_2 ,	 
 AUDIOHAL_DTMF_3 ,	 
 AUDIOHAL_DTMF_4 ,	 
 AUDIOHAL_DTMF_5 ,	 
 AUDIOHAL_DTMF_6 ,	 
 AUDIOHAL_DTMF_7 ,	 
 AUDIOHAL_DTMF_8 ,	 
 AUDIOHAL_DTMF_9 ,	 
 AUDIOHAL_DTMF_A ,	 
 AUDIOHAL_DTMF_B ,	 
 AUDIOHAL_DTMF_C ,	 
 AUDIOHAL_DTMF_D ,	 
 AUDIOHAL_DTMF_S , // * key	 
 AUDIOHAL_DTMF_P , // # key	 
 AUDIOHAL_COMFORT_425 ,	 
 AUDIOHAL_COMFORT_950 ,	 
 AUDIOHAL_COMFORT_1400 ,	 
 AUDIOHAL_COMFORT_1800 ,	 
 } AUDIOHAL_TONE_TYPE_T;

//ICAT EXPORTED ENUM 
 typedef enum 
 {	 
 AUDIOHAL_TONE_0DB = 0 ,	 
 AUDIOHAL_TONE_M3DB , // -3 dB	 
 AUDIOHAL_TONE_M9DB , // -9 dB	 
 AUDIOHAL_TONE_M15DB , // -15 dB	 
 AUDIOHAL_TONE_QTY	 
 } AUDIOHAL_TONE_ATTENUATION_T;

//ICAT EXPORTED ENUM 
 typedef enum {	 
 AUDIOHAL_HEADPHONE_PLUG_OUT = 0 ,	 
 AUDIOHAL_HEADPHONE_PLUG_IN = 1	 
 } AUDIOHAL_HEADPHONE_PLUG_T;

//ICAT EXPORTED ENUM 
 typedef enum {	 
 AUDIOHAL_HEADPHONE_TYPE_NULL = 0 ,	 
 AUDIOHAL_HEADPHONE_TYPE_UNKNOWN = 1 ,	 
	 
 AUDIOHAL_HEADPHONE_TYPE_3 = 3 , // HEAD_PHONE_TYPE3	 
 AUDIOHAL_HEADPHONE_TYPE_4 = 4 // HEAD_PHONE_TYPE4	 
 } AUDIOHAL_HEADPHONE_TYPE_T;

//ICAT EXPORTED ENUM 
 typedef enum {	 
 AUDIOHAL_HEADPHONE_EVENT_NULL = 0 ,	 
 AUDIOHAL_HEADPHONE_EVENT_VOLUME_UP = 1 ,	 
 AUDIOHAL_HEADPHONE_EVENT_VOLUME_DOWN= 2 ,	 
 AUDIOHAL_HEADPHONE_EVENT_HOOK_KEY = 3 ,	 
	 
 AUDIOHAL_HEADPHONE_EVENT_TYPE_C_REVERSE = 7	 
	 
 } AUDIOHAL_HEADPHONE_EVENT_T;

//ICAT EXPORTED ENUM 
 typedef enum {	 
 AUDIOHAL_PORT_VOICE = 0 ,	 
 AUDIOHAL_PORT_SSPA = AUDIOHAL_PORT_VOICE ,	 
 AUDIOHAL_PORT_HW_SSP0 = 1 ,	 
 AUDIOHAL_PORT_SSP1 = AUDIOHAL_PORT_HW_SSP0 ,	 
 AUDIOHAL_PORT_HW_SSP1 = 2 ,	 
 AUDIOHAL_PORT_SSP2 = AUDIOHAL_PORT_HW_SSP1 ,	 
 AUDIOHAL_PORT_HW_SSP2 = 3 ,	 
 AUDIOHAL_PORT_SSP3 = AUDIOHAL_PORT_HW_SSP2 ,	 
	 
 AUDIOHAL_PORT_QTY	 
 } AUDIOHAL_DEVICE_PORT_T;

typedef void ( *AUDIOHAL_HANDLER_T ) ( void ) ;
typedef void ( *AUDIOHAL_HeadsetReport_T ) ( UINT32 plug , UINT32 type , UINT32 event ) ;
typedef void ( *AUDIOHAL_SpeakerPA_T ) ( UINT32 on ) ;
typedef void ( *AUDIOHAL_Codec_T ) ( UINT32 on ) ;
typedef void ( *AUDIOHAL_BT_CALLBACK_T ) ( UINT32 on ) ;
typedef void ( *AUDIOHAL_KWS_CB_T ) ( unsigned int key ) ;
typedef int ( *AUDIO_EXTRA_CB ) ( short* dataPtr , UINT16 dataSize ) ;
//ICAT EXPORTED STRUCT 
 typedef struct {	 
 UINT32 * startAddress ;	 
 UINT16 length ;	 
	 
 AUDIOHAL_FREQ_T sampleRate ;	 
 AUDIOHAL_CH_NB_T channelNb ;	 
 // BOOL voiceQuality ;	 
 // BOOL playSyncWithRecord ;	 
 INT voiceQuality ;	 
 INT playSyncWithRecord ;	 
	 
 AUDIOHAL_HANDLER_T halfHandler ;	 
 AUDIOHAL_HANDLER_T endHandler ;	 
 } AUDIOHAL_STREAM_T;

