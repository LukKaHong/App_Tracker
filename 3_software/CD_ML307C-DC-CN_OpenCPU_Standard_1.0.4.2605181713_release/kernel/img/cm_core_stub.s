.arm
/*def osKernelLock*/
    .type osKernelLock, %function
    .func
    .global osKernelLock
    .syntax unified
    .thumb
    .section .text
osKernelLock:
    ldr.w pc ,[pc]
    .word 0x80238a45
    .endfunc
/*def osKernelUnlock*/
    .type osKernelUnlock, %function
    .func
    .global osKernelUnlock
    .syntax unified
    .thumb
    .section .text
osKernelUnlock:
    ldr.w pc ,[pc]
    .word 0x80238a6b
    .endfunc
/*def osKernelGetTickFreq*/
    .type osKernelGetTickFreq, %function
    .func
    .global osKernelGetTickFreq
    .syntax unified
    .thumb
    .section .text
osKernelGetTickFreq:
    ldr.w pc ,[pc]
    .word 0x80238a8b
    .endfunc
/*def osKernelGetTickCount*/
    .type osKernelGetTickCount, %function
    .func
    .global osKernelGetTickCount
    .syntax unified
    .thumb
    .section .text
osKernelGetTickCount:
    ldr.w pc ,[pc]
    .word 0x80238a9d
    .endfunc
/*def osThreadNew*/
    .type osThreadNew, %function
    .func
    .global osThreadNew
    .syntax unified
    .thumb
    .section .text
osThreadNew:
    ldr.w pc ,[pc]
    .word 0x80238aa3
    .endfunc
/*def osThreadGetName*/
    .type osThreadGetName, %function
    .func
    .global osThreadGetName
    .syntax unified
    .thumb
    .section .text
osThreadGetName:
    ldr.w pc ,[pc]
    .word 0x80238ae9
    .endfunc
/*def osThreadGetId*/
    .type osThreadGetId, %function
    .func
    .global osThreadGetId
    .syntax unified
    .thumb
    .section .text
osThreadGetId:
    ldr.w pc ,[pc]
    .word 0x80238af3
    .endfunc
/*def osThreadGetState*/
    .type osThreadGetState, %function
    .func
    .global osThreadGetState
    .syntax unified
    .thumb
    .section .text
osThreadGetState:
    ldr.w pc ,[pc]
    .word 0x80238b05
    .endfunc
/*def osThreadGetStackSize*/
    .type osThreadGetStackSize, %function
    .func
    .global osThreadGetStackSize
    .syntax unified
    .thumb
    .section .text
osThreadGetStackSize:
    ldr.w pc ,[pc]
    .word 0x80238b51
    .endfunc
/*def osThreadGetStackSpace*/
    .type osThreadGetStackSpace, %function
    .func
    .global osThreadGetStackSpace
    .syntax unified
    .thumb
    .section .text
osThreadGetStackSpace:
    ldr.w pc ,[pc]
    .word 0x80238b77
    .endfunc
/*def osThreadTerminate*/
    .type osThreadTerminate, %function
    .func
    .global osThreadTerminate
    .syntax unified
    .thumb
    .section .text
osThreadTerminate:
    ldr.w pc ,[pc]
    .word 0x80238c29
    .endfunc
/*def osThreadSetPriority*/
    .type osThreadSetPriority, %function
    .func
    .global osThreadSetPriority
    .syntax unified
    .thumb
    .section .text
osThreadSetPriority:
    ldr.w pc ,[pc]
    .word 0x80238ba5
    .endfunc
/*def osThreadGetPriority*/
    .type osThreadGetPriority, %function
    .func
    .global osThreadGetPriority
    .syntax unified
    .thumb
    .section .text
osThreadGetPriority:
    ldr.w pc ,[pc]
    .word 0x80238bc9
    .endfunc
/*def osThreadSuspend*/
    .type osThreadSuspend, %function
    .func
    .global osThreadSuspend
    .syntax unified
    .thumb
    .section .text
osThreadSuspend:
    ldr.w pc ,[pc]
    .word 0x80238bfb
    .endfunc
/*def osThreadResume*/
    .type osThreadResume, %function
    .func
    .global osThreadResume
    .syntax unified
    .thumb
    .section .text
osThreadResume:
    ldr.w pc ,[pc]
    .word 0x80238c11
    .endfunc
/*def osThreadYield*/
    .type osThreadYield, %function
    .func
    .global osThreadYield
    .syntax unified
    .thumb
    .section .text
osThreadYield:
    ldr.w pc ,[pc]
    .word 0x80238bef
    .endfunc
/*def osThreadGetCount*/
    .type osThreadGetCount, %function
    .func
    .global osThreadGetCount
    .syntax unified
    .thumb
    .section .text
osThreadGetCount:
    ldr.w pc ,[pc]
    .word 0x80238c3f
    .endfunc
/*def osDelay*/
    .type osDelay, %function
    .func
    .global osDelay
    .syntax unified
    .thumb
    .section .text
osDelay:
    ldr.w pc ,[pc]
    .word 0x80238c59
    .endfunc
/*def osTimerNew*/
    .type osTimerNew, %function
    .func
    .global osTimerNew
    .syntax unified
    .thumb
    .section .text
osTimerNew:
    ldr.w pc ,[pc]
    .word 0x80238d1d
    .endfunc
/*def osTimerStart*/
    .type osTimerStart, %function
    .func
    .global osTimerStart
    .syntax unified
    .thumb
    .section .text
osTimerStart:
    ldr.w pc ,[pc]
    .word 0x80238d79
    .endfunc
/*def osTimerStop*/
    .type osTimerStop, %function
    .func
    .global osTimerStop
    .syntax unified
    .thumb
    .section .text
osTimerStop:
    ldr.w pc ,[pc]
    .word 0x80238db5
    .endfunc
/*def osTimerIsRunning*/
    .type osTimerIsRunning, %function
    .func
    .global osTimerIsRunning
    .syntax unified
    .thumb
    .section .text
osTimerIsRunning:
    ldr.w pc ,[pc]
    .word 0x80238dcb
    .endfunc
/*def osTimerDelete*/
    .type osTimerDelete, %function
    .func
    .global osTimerDelete
    .syntax unified
    .thumb
    .section .text
osTimerDelete:
    ldr.w pc ,[pc]
    .word 0x80238de9
    .endfunc
/*def osEventFlagsNew*/
    .type osEventFlagsNew, %function
    .func
    .global osEventFlagsNew
    .syntax unified
    .thumb
    .section .text
osEventFlagsNew:
    ldr.w pc ,[pc]
    .word 0x80238c65
    .endfunc
/*def osEventFlagsSet*/
    .type osEventFlagsSet, %function
    .func
    .global osEventFlagsSet
    .syntax unified
    .thumb
    .section .text
osEventFlagsSet:
    ldr.w pc ,[pc]
    .word 0x80238c7b
    .endfunc
/*def osEventFlagsWait*/
    .type osEventFlagsWait, %function
    .func
    .global osEventFlagsWait
    .syntax unified
    .thumb
    .section .text
osEventFlagsWait:
    ldr.w pc ,[pc]
    .word 0x80238c9f
    .endfunc
/*def osEventFlagsDelete*/
    .type osEventFlagsDelete, %function
    .func
    .global osEventFlagsDelete
    .syntax unified
    .thumb
    .section .text
osEventFlagsDelete:
    ldr.w pc ,[pc]
    .word 0x80238cc9
    .endfunc
/*def osMutexNew*/
    .type osMutexNew, %function
    .func
    .global osMutexNew
    .syntax unified
    .thumb
    .section .text
osMutexNew:
    ldr.w pc ,[pc]
    .word 0x80238d0b
    .endfunc
/*def osMutexAcquire*/
    .type osMutexAcquire, %function
    .func
    .global osMutexAcquire
    .syntax unified
    .thumb
    .section .text
osMutexAcquire:
    ldr.w pc ,[pc]
    .word 0x80238cf5
    .endfunc
/*def osMutexRelease*/
    .type osMutexRelease, %function
    .func
    .global osMutexRelease
    .syntax unified
    .thumb
    .section .text
osMutexRelease:
    ldr.w pc ,[pc]
    .word 0x80238cdf
    .endfunc
/*def osMutexDelete*/
    .type osMutexDelete, %function
    .func
    .global osMutexDelete
    .syntax unified
    .thumb
    .section .text
osMutexDelete:
    ldr.w pc ,[pc]
    .word 0x80238e69
    .endfunc
/*def osSemaphoreNew*/
    .type osSemaphoreNew, %function
    .func
    .global osSemaphoreNew
    .syntax unified
    .thumb
    .section .text
osSemaphoreNew:
    ldr.w pc ,[pc]
    .word 0x80238e7f
    .endfunc
/*def osSemaphoreAcquire*/
    .type osSemaphoreAcquire, %function
    .func
    .global osSemaphoreAcquire
    .syntax unified
    .thumb
    .section .text
osSemaphoreAcquire:
    ldr.w pc ,[pc]
    .word 0x80238e95
    .endfunc
/*def osSemaphoreRelease*/
    .type osSemaphoreRelease, %function
    .func
    .global osSemaphoreRelease
    .syntax unified
    .thumb
    .section .text
osSemaphoreRelease:
    ldr.w pc ,[pc]
    .word 0x80238eab
    .endfunc
/*def osSemaphoreGetCount*/
    .type osSemaphoreGetCount, %function
    .func
    .global osSemaphoreGetCount
    .syntax unified
    .thumb
    .section .text
osSemaphoreGetCount:
    ldr.w pc ,[pc]
    .word 0x80238ec1
    .endfunc
/*def osSemaphoreDelete*/
    .type osSemaphoreDelete, %function
    .func
    .global osSemaphoreDelete
    .syntax unified
    .thumb
    .section .text
osSemaphoreDelete:
    ldr.w pc ,[pc]
    .word 0x80238ed7
    .endfunc
/*def osMessageQueueDelete*/
    .type osMessageQueueDelete, %function
    .func
    .global osMessageQueueDelete
    .syntax unified
    .thumb
    .section .text
osMessageQueueDelete:
    ldr.w pc ,[pc]
    .word 0x80238f5f
    .endfunc
/*def osMessageQueueGet*/
    .type osMessageQueueGet, %function
    .func
    .global osMessageQueueGet
    .syntax unified
    .thumb
    .section .text
osMessageQueueGet:
    ldr.w pc ,[pc]
    .word 0x80238f23
    .endfunc
/*def osMessageQueueNew*/
    .type osMessageQueueNew, %function
    .func
    .global osMessageQueueNew
    .syntax unified
    .thumb
    .section .text
osMessageQueueNew:
    ldr.w pc ,[pc]
    .word 0x80238eed
    .endfunc
/*def osMessageQueuePut*/
    .type osMessageQueuePut, %function
    .func
    .global osMessageQueuePut
    .syntax unified
    .thumb
    .section .text
osMessageQueuePut:
    ldr.w pc ,[pc]
    .word 0x80238f0b
    .endfunc
/*def cm_uart_open*/
    .type cm_uart_open, %function
    .func
    .global cm_uart_open
    .syntax unified
    .thumb
    .section .text
cm_uart_open:
    ldr.w pc ,[pc]
    .word 0x80263231
    .endfunc
/*def cm_uart_register_event*/
    .type cm_uart_register_event, %function
    .func
    .global cm_uart_register_event
    .syntax unified
    .thumb
    .section .text
cm_uart_register_event:
    ldr.w pc ,[pc]
    .word 0x80263347
    .endfunc
/*def cm_uart_close*/
    .type cm_uart_close, %function
    .func
    .global cm_uart_close
    .syntax unified
    .thumb
    .section .text
cm_uart_close:
    ldr.w pc ,[pc]
    .word 0x8026336d
    .endfunc
/*def cm_uart_write*/
    .type cm_uart_write, %function
    .func
    .global cm_uart_write
    .syntax unified
    .thumb
    .section .text
cm_uart_write:
    ldr.w pc ,[pc]
    .word 0x802633bd
    .endfunc
/*def cm_uart_read*/
    .type cm_uart_read, %function
    .func
    .global cm_uart_read
    .syntax unified
    .thumb
    .section .text
cm_uart_read:
    ldr.w pc ,[pc]
    .word 0x80263419
    .endfunc
/*def cm_uart_get_cfg*/
    .type cm_uart_get_cfg, %function
    .func
    .global cm_uart_get_cfg
    .syntax unified
    .thumb
    .section .text
cm_uart_get_cfg:
    ldr.w pc ,[pc]
    .word 0x802634a3
    .endfunc
/*def cm_uart_get_rxrb_data_len*/
    .type cm_uart_get_rxrb_data_len, %function
    .func
    .global cm_uart_get_rxrb_data_len
    .syntax unified
    .thumb
    .section .text
cm_uart_get_rxrb_data_len:
    ldr.w pc ,[pc]
    .word 0x802634eb
    .endfunc
/*def cm_uart_clean*/
    .type cm_uart_clean, %function
    .func
    .global cm_uart_clean
    .syntax unified
    .thumb
    .section .text
cm_uart_clean:
    ldr.w pc ,[pc]
    .word 0x802635e1
    .endfunc
/*def cm_uart_is_sending*/
    .type cm_uart_is_sending, %function
    .func
    .global cm_uart_is_sending
    .syntax unified
    .thumb
    .section .text
cm_uart_is_sending:
    ldr.w pc ,[pc]
    .word 0x80263637
    .endfunc
/*def cm_mem_get_heap_stats*/
    .type cm_mem_get_heap_stats, %function
    .func
    .global cm_mem_get_heap_stats
    .syntax unified
    .thumb
    .section .text
cm_mem_get_heap_stats:
    ldr.w pc ,[pc]
    .word 0x80262455
    .endfunc
/*def cm_malloc*/
    .type cm_malloc, %function
    .func
    .global cm_malloc
    .syntax unified
    .thumb
    .section .text
cm_malloc:
    ldr.w pc ,[pc]
    .word 0x8020e51d
    .endfunc
/*def cm_realloc*/
    .type cm_realloc, %function
    .func
    .global cm_realloc
    .syntax unified
    .thumb
    .section .text
cm_realloc:
    ldr.w pc ,[pc]
    .word 0x80262477
    .endfunc
/*def cm_calloc*/
    .type cm_calloc, %function
    .func
    .global cm_calloc
    .syntax unified
    .thumb
    .section .text
cm_calloc:
    ldr.w pc ,[pc]
    .word 0x8026247b
    .endfunc
/*def cm_free*/
    .type cm_free, %function
    .func
    .global cm_free
    .syntax unified
    .thumb
    .section .text
cm_free:
    ldr.w pc ,[pc]
    .word 0x8026247f
    .endfunc
/*def cm_sys_get_imei*/
    .type cm_sys_get_imei, %function
    .func
    .global cm_sys_get_imei
    .syntax unified
    .thumb
    .section .text
cm_sys_get_imei:
    ldr.w pc ,[pc]
    .word 0x80262499
    .endfunc
/*def cm_sys_get_sn*/
    .type cm_sys_get_sn, %function
    .func
    .global cm_sys_get_sn
    .syntax unified
    .thumb
    .section .text
cm_sys_get_sn:
    ldr.w pc ,[pc]
    .word 0x802624ad
    .endfunc
/*def cm_sys_get_cm_ver*/
    .type cm_sys_get_cm_ver, %function
    .func
    .global cm_sys_get_cm_ver
    .syntax unified
    .thumb
    .section .text
cm_sys_get_cm_ver:
    ldr.w pc ,[pc]
    .word 0x802624c1
    .endfunc
/*def cm_log_printf*/
    .type cm_log_printf, %function
    .func
    .global cm_log_printf
    .syntax unified
    .thumb
    .section .text
cm_log_printf:
    ldr.w pc ,[pc]
    .word 0x802624f3
    .endfunc
/*def cm_assert*/
    .type cm_assert, %function
    .func
    .global cm_assert
    .syntax unified
    .thumb
    .section .text
cm_assert:
    ldr.w pc ,[pc]
    .word 0x80262527
    .endfunc
/*def cm_fs_open*/
    .type cm_fs_open, %function
    .func
    .global cm_fs_open
    .syntax unified
    .thumb
    .section .text
cm_fs_open:
    ldr.w pc ,[pc]
    .word 0x80262559
    .endfunc
/*def cm_fs_close*/
    .type cm_fs_close, %function
    .func
    .global cm_fs_close
    .syntax unified
    .thumb
    .section .text
cm_fs_close:
    ldr.w pc ,[pc]
    .word 0x80262597
    .endfunc
/*def cm_fs_read*/
    .type cm_fs_read, %function
    .func
    .global cm_fs_read
    .syntax unified
    .thumb
    .section .text
cm_fs_read:
    ldr.w pc ,[pc]
    .word 0x802625a9
    .endfunc
/*def cm_fs_write*/
    .type cm_fs_write, %function
    .func
    .global cm_fs_write
    .syntax unified
    .thumb
    .section .text
cm_fs_write:
    ldr.w pc ,[pc]
    .word 0x802625c1
    .endfunc
/*def cm_fs_seek*/
    .type cm_fs_seek, %function
    .func
    .global cm_fs_seek
    .syntax unified
    .thumb
    .section .text
cm_fs_seek:
    ldr.w pc ,[pc]
    .word 0x802625dd
    .endfunc
/*def cm_fs_delete*/
    .type cm_fs_delete, %function
    .func
    .global cm_fs_delete
    .syntax unified
    .thumb
    .section .text
cm_fs_delete:
    ldr.w pc ,[pc]
    .word 0x802625ed
    .endfunc
/*def cm_fs_move*/
    .type cm_fs_move, %function
    .func
    .global cm_fs_move
    .syntax unified
    .thumb
    .section .text
cm_fs_move:
    ldr.w pc ,[pc]
    .word 0x802625f1
    .endfunc
/*def cm_fs_exist*/
    .type cm_fs_exist, %function
    .func
    .global cm_fs_exist
    .syntax unified
    .thumb
    .section .text
cm_fs_exist:
    ldr.w pc ,[pc]
    .word 0x802625f5
    .endfunc
/*def cm_fs_filesize*/
    .type cm_fs_filesize, %function
    .func
    .global cm_fs_filesize
    .syntax unified
    .thumb
    .section .text
cm_fs_filesize:
    ldr.w pc ,[pc]
    .word 0x80262617
    .endfunc
/*def cm_fs_getinfo*/
    .type cm_fs_getinfo, %function
    .func
    .global cm_fs_getinfo
    .syntax unified
    .thumb
    .section .text
cm_fs_getinfo:
    ldr.w pc ,[pc]
    .word 0x8026263d
    .endfunc
/*def cm_fs_findfirst*/
    .type cm_fs_findfirst, %function
    .func
    .global cm_fs_findfirst
    .syntax unified
    .thumb
    .section .text
cm_fs_findfirst:
    ldr.w pc ,[pc]
    .word 0x80262659
    .endfunc
/*def cm_fs_findnext*/
    .type cm_fs_findnext, %function
    .func
    .global cm_fs_findnext
    .syntax unified
    .thumb
    .section .text
cm_fs_findnext:
    ldr.w pc ,[pc]
    .word 0x8026269b
    .endfunc
/*def cm_rtc_set_timezone*/
    .type cm_rtc_set_timezone, %function
    .func
    .global cm_rtc_set_timezone
    .syntax unified
    .thumb
    .section .text
cm_rtc_set_timezone:
    ldr.w pc ,[pc]
    .word 0x802626e9
    .endfunc
/*def cm_rtc_get_timezone*/
    .type cm_rtc_get_timezone, %function
    .func
    .global cm_rtc_get_timezone
    .syntax unified
    .thumb
    .section .text
cm_rtc_get_timezone:
    ldr.w pc ,[pc]
    .word 0x80262707
    .endfunc
/*def cm_rtc_set_current_time*/
    .type cm_rtc_set_current_time, %function
    .func
    .global cm_rtc_set_current_time
    .syntax unified
    .thumb
    .section .text
cm_rtc_set_current_time:
    ldr.w pc ,[pc]
    .word 0x80262715
    .endfunc
/*def cm_rtc_get_current_time*/
    .type cm_rtc_get_current_time, %function
    .func
    .global cm_rtc_get_current_time
    .syntax unified
    .thumb
    .section .text
cm_rtc_get_current_time:
    ldr.w pc ,[pc]
    .word 0x8026273f
    .endfunc
/*def cm_rtc_register_alarm_cb*/
    .type cm_rtc_register_alarm_cb, %function
    .func
    .global cm_rtc_register_alarm_cb
    .syntax unified
    .thumb
    .section .text
cm_rtc_register_alarm_cb:
    ldr.w pc ,[pc]
    .word 0x8026275b
    .endfunc
/*def cm_rtc_enable_alarm*/
    .type cm_rtc_enable_alarm, %function
    .func
    .global cm_rtc_enable_alarm
    .syntax unified
    .thumb
    .section .text
cm_rtc_enable_alarm:
    ldr.w pc ,[pc]
    .word 0x8026275f
    .endfunc
/*def cm_rtc_set_alarm*/
    .type cm_rtc_set_alarm, %function
    .func
    .global cm_rtc_set_alarm
    .syntax unified
    .thumb
    .section .text
cm_rtc_set_alarm:
    ldr.w pc ,[pc]
    .word 0x80262763
    .endfunc
/*def cm_rtc_get_alarm*/
    .type cm_rtc_get_alarm, %function
    .func
    .global cm_rtc_get_alarm
    .syntax unified
    .thumb
    .section .text
cm_rtc_get_alarm:
    ldr.w pc ,[pc]
    .word 0x80262775
    .endfunc
/*def cm_sim_get_imsi*/
    .type cm_sim_get_imsi, %function
    .func
    .global cm_sim_get_imsi
    .syntax unified
    .thumb
    .section .text
cm_sim_get_imsi:
    ldr.w pc ,[pc]
    .word 0x80262489
    .endfunc
/*def cm_gpio_init*/
    .type cm_gpio_init, %function
    .func
    .global cm_gpio_init
    .syntax unified
    .thumb
    .section .text
cm_gpio_init:
    ldr.w pc ,[pc]
    .word 0x80263abd
    .endfunc
/*def cm_gpio_deinit*/
    .type cm_gpio_deinit, %function
    .func
    .global cm_gpio_deinit
    .syntax unified
    .thumb
    .section .text
cm_gpio_deinit:
    ldr.w pc ,[pc]
    .word 0x80263b49
    .endfunc
/*def cm_gpio_set_level*/
    .type cm_gpio_set_level, %function
    .func
    .global cm_gpio_set_level
    .syntax unified
    .thumb
    .section .text
cm_gpio_set_level:
    ldr.w pc ,[pc]
    .word 0x80263ba1
    .endfunc
/*def cm_gpio_get_level*/
    .type cm_gpio_get_level, %function
    .func
    .global cm_gpio_get_level
    .syntax unified
    .thumb
    .section .text
cm_gpio_get_level:
    ldr.w pc ,[pc]
    .word 0x80263bf3
    .endfunc
/*def cm_gpio_set_pull*/
    .type cm_gpio_set_pull, %function
    .func
    .global cm_gpio_set_pull
    .syntax unified
    .thumb
    .section .text
cm_gpio_set_pull:
    ldr.w pc ,[pc]
    .word 0x80263c2b
    .endfunc
/*def cm_gpio_get_pull*/
    .type cm_gpio_get_pull, %function
    .func
    .global cm_gpio_get_pull
    .syntax unified
    .thumb
    .section .text
cm_gpio_get_pull:
    ldr.w pc ,[pc]
    .word 0x80263c8b
    .endfunc
/*def cm_gpio_set_direction*/
    .type cm_gpio_set_direction, %function
    .func
    .global cm_gpio_set_direction
    .syntax unified
    .thumb
    .section .text
cm_gpio_set_direction:
    ldr.w pc ,[pc]
    .word 0x80263cbd
    .endfunc
/*def cm_gpio_get_direction*/
    .type cm_gpio_get_direction, %function
    .func
    .global cm_gpio_get_direction
    .syntax unified
    .thumb
    .section .text
cm_gpio_get_direction:
    ldr.w pc ,[pc]
    .word 0x80263d0f
    .endfunc
/*def cm_gpio_interrupt_register*/
    .type cm_gpio_interrupt_register, %function
    .func
    .global cm_gpio_interrupt_register
    .syntax unified
    .thumb
    .section .text
cm_gpio_interrupt_register:
    ldr.w pc ,[pc]
    .word 0x80263d47
    .endfunc
/*def cm_gpio_interrupt_enable*/
    .type cm_gpio_interrupt_enable, %function
    .func
    .global cm_gpio_interrupt_enable
    .syntax unified
    .thumb
    .section .text
cm_gpio_interrupt_enable:
    ldr.w pc ,[pc]
    .word 0x80263d9f
    .endfunc
/*def cm_gpio_interrupt_disable*/
    .type cm_gpio_interrupt_disable, %function
    .func
    .global cm_gpio_interrupt_disable
    .syntax unified
    .thumb
    .section .text
cm_gpio_interrupt_disable:
    ldr.w pc ,[pc]
    .word 0x802640ed
    .endfunc
/*def cm_gpio_ioctl*/
    .type cm_gpio_ioctl, %function
    .func
    .global cm_gpio_ioctl
    .syntax unified
    .thumb
    .section .text
cm_gpio_ioctl:
    ldr.w pc ,[pc]
    .word 0x80264165
    .endfunc
/*def cm_iomux_set_pin_func*/
    .type cm_iomux_set_pin_func, %function
    .func
    .global cm_iomux_set_pin_func
    .syntax unified
    .thumb
    .section .text
cm_iomux_set_pin_func:
    ldr.w pc ,[pc]
    .word 0x80262239
    .endfunc
/*def cm_iomux_get_pin_func*/
    .type cm_iomux_get_pin_func, %function
    .func
    .global cm_iomux_get_pin_func
    .syntax unified
    .thumb
    .section .text
cm_iomux_get_pin_func:
    ldr.w pc ,[pc]
    .word 0x80262305
    .endfunc
/*def cm_iomux_set_pin_cmd*/
    .type cm_iomux_set_pin_cmd, %function
    .func
    .global cm_iomux_set_pin_cmd
    .syntax unified
    .thumb
    .section .text
cm_iomux_set_pin_cmd:
    ldr.w pc ,[pc]
    .word 0x80262341
    .endfunc
/*def cm_pm_reboot*/
    .type cm_pm_reboot, %function
    .func
    .global cm_pm_reboot
    .syntax unified
    .thumb
    .section .text
cm_pm_reboot:
    ldr.w pc ,[pc]
    .word 0x8023ced9
    .endfunc
/*def cm_pm_poweroff*/
    .type cm_pm_poweroff, %function
    .func
    .global cm_pm_poweroff
    .syntax unified
    .thumb
    .section .text
cm_pm_poweroff:
    ldr.w pc ,[pc]
    .word 0x8023cedd
    .endfunc
/*def cm_pm_get_power_on_reason*/
    .type cm_pm_get_power_on_reason, %function
    .func
    .global cm_pm_get_power_on_reason
    .syntax unified
    .thumb
    .section .text
cm_pm_get_power_on_reason:
    ldr.w pc ,[pc]
    .word 0x8023cee1
    .endfunc
/*def cm_pm_work_lock*/
    .type cm_pm_work_lock, %function
    .func
    .global cm_pm_work_lock
    .syntax unified
    .thumb
    .section .text
cm_pm_work_lock:
    ldr.w pc ,[pc]
    .word 0x8023cf25
    .endfunc
/*def cm_pm_work_unlock*/
    .type cm_pm_work_unlock, %function
    .func
    .global cm_pm_work_unlock
    .syntax unified
    .thumb
    .section .text
cm_pm_work_unlock:
    ldr.w pc ,[pc]
    .word 0x8023cf2d
    .endfunc
/*def cm_pm_init*/
    .type cm_pm_init, %function
    .func
    .global cm_pm_init
    .syntax unified
    .thumb
    .section .text
cm_pm_init:
    ldr.w pc ,[pc]
    .word 0x8023cf1d
    .endfunc
/*def cm_pm_powerkey_regist_callback*/
    .type cm_pm_powerkey_regist_callback, %function
    .func
    .global cm_pm_powerkey_regist_callback
    .syntax unified
    .thumb
    .section .text
cm_pm_powerkey_regist_callback:
    ldr.w pc ,[pc]
    .word 0x8023cf35
    .endfunc
/*def cm_keypad_config*/
    .type cm_keypad_config, %function
    .func
    .global cm_keypad_config
    .syntax unified
    .thumb
    .section .text
cm_keypad_config:
    ldr.w pc ,[pc]
    .word 0x80264f87
    .endfunc
/*def cm_keypad_init*/
    .type cm_keypad_init, %function
    .func
    .global cm_keypad_init
    .syntax unified
    .thumb
    .section .text
cm_keypad_init:
    ldr.w pc ,[pc]
    .word 0x80264cf1
    .endfunc
/*def cm_keypad_register*/
    .type cm_keypad_register, %function
    .func
    .global cm_keypad_register
    .syntax unified
    .thumb
    .section .text
cm_keypad_register:
    ldr.w pc ,[pc]
    .word 0x80264d5b
    .endfunc
/*def cm_keypad_unregister*/
    .type cm_keypad_unregister, %function
    .func
    .global cm_keypad_unregister
    .syntax unified
    .thumb
    .section .text
cm_keypad_unregister:
    ldr.w pc ,[pc]
    .word 0x80264d9d
    .endfunc
/*def cm_keypad_deinit*/
    .type cm_keypad_deinit, %function
    .func
    .global cm_keypad_deinit
    .syntax unified
    .thumb
    .section .text
cm_keypad_deinit:
    ldr.w pc ,[pc]
    .word 0x80264dd5
    .endfunc
/*def cm_keypad_sleep*/
    .type cm_keypad_sleep, %function
    .func
    .global cm_keypad_sleep
    .syntax unified
    .thumb
    .section .text
cm_keypad_sleep:
    ldr.w pc ,[pc]
    .word 0x80264fe1
    .endfunc
/*def cm_pwm_set_clk*/
    .type cm_pwm_set_clk, %function
    .func
    .global cm_pwm_set_clk
    .syntax unified
    .thumb
    .section .text
cm_pwm_set_clk:
    ldr.w pc ,[pc]
    .word 0x80264971
    .endfunc
/*def cm_pwm_open_ns*/
    .type cm_pwm_open_ns, %function
    .func
    .global cm_pwm_open_ns
    .syntax unified
    .thumb
    .section .text
cm_pwm_open_ns:
    ldr.w pc ,[pc]
    .word 0x80264677
    .endfunc
/*def cm_pwm_close*/
    .type cm_pwm_close, %function
    .func
    .global cm_pwm_close
    .syntax unified
    .thumb
    .section .text
cm_pwm_close:
    ldr.w pc ,[pc]
    .word 0x802648e1
    .endfunc
/*def cm_lbs_init*/
    .type cm_lbs_init, %function
    .func
    .global cm_lbs_init
    .syntax unified
    .thumb
    .section .text
cm_lbs_init:
    ldr.w pc ,[pc]
    .word 0x802614a7
    .endfunc
/*def cm_lbs_deinit*/
    .type cm_lbs_deinit, %function
    .func
    .global cm_lbs_deinit
    .syntax unified
    .thumb
    .section .text
cm_lbs_deinit:
    ldr.w pc ,[pc]
    .word 0x8026148b
    .endfunc
/*def cm_lbs_location*/
    .type cm_lbs_location, %function
    .func
    .global cm_lbs_location
    .syntax unified
    .thumb
    .section .text
cm_lbs_location:
    ldr.w pc ,[pc]
    .word 0x80261501
    .endfunc
/*def cm_lbs_get_attr*/
    .type cm_lbs_get_attr, %function
    .func
    .global cm_lbs_get_attr
    .syntax unified
    .thumb
    .section .text
cm_lbs_get_attr:
    ldr.w pc ,[pc]
    .word 0x8026151d
    .endfunc
/*def cm_asocket_eloop*/
    .type cm_asocket_eloop, %function
    .func
    .global cm_asocket_eloop
    .syntax unified
    .thumb
    .section .text
cm_asocket_eloop:
    ldr.w pc ,[pc]
    .word 0x8025feb5
    .endfunc
/*def cm_asocket_open*/
    .type cm_asocket_open, %function
    .func
    .global cm_asocket_open
    .syntax unified
    .thumb
    .section .text
cm_asocket_open:
    ldr.w pc ,[pc]
    .word 0x8025feb9
    .endfunc
/*def cm_asocket_connect*/
    .type cm_asocket_connect, %function
    .func
    .global cm_asocket_connect
    .syntax unified
    .thumb
    .section .text
cm_asocket_connect:
    ldr.w pc ,[pc]
    .word 0x8025ff7f
    .endfunc
/*def cm_asocket_close*/
    .type cm_asocket_close, %function
    .func
    .global cm_asocket_close
    .syntax unified
    .thumb
    .section .text
cm_asocket_close:
    ldr.w pc ,[pc]
    .word 0x8025ffc1
    .endfunc
/*def cm_async_dns_set_priority*/
    .type cm_async_dns_set_priority, %function
    .func
    .global cm_async_dns_set_priority
    .syntax unified
    .thumb
    .section .text
cm_async_dns_set_priority:
    ldr.w pc ,[pc]
    .word 0x802392bb
    .endfunc
/*def cm_async_dns_eloop*/
    .type cm_async_dns_eloop, %function
    .func
    .global cm_async_dns_eloop
    .syntax unified
    .thumb
    .section .text
cm_async_dns_eloop:
    ldr.w pc ,[pc]
    .word 0x802392b7
    .endfunc
/*def cm_async_dns_request_ex*/
    .type cm_async_dns_request_ex, %function
    .func
    .global cm_async_dns_request_ex
    .syntax unified
    .thumb
    .section .text
cm_async_dns_request_ex:
    ldr.w pc ,[pc]
    .word 0x802392cd
    .endfunc
/*def cm_async_dns_get_type_by_priority*/
    .type cm_async_dns_get_type_by_priority, %function
    .func
    .global cm_async_dns_get_type_by_priority
    .syntax unified
    .thumb
    .section .text
cm_async_dns_get_type_by_priority:
    ldr.w pc ,[pc]
    .word 0x802392a7
    .endfunc
/*def cm_get_active_cid*/
    .type cm_get_active_cid, %function
    .func
    .global cm_get_active_cid
    .syntax unified
    .thumb
    .section .text
cm_get_active_cid:
    ldr.w pc ,[pc]
    .word 0x80234663
    .endfunc
/*def cm_eloop_create*/
    .type cm_eloop_create, %function
    .func
    .global cm_eloop_create
    .syntax unified
    .thumb
    .section .text
cm_eloop_create:
    ldr.w pc ,[pc]
    .word 0x802394c3
    .endfunc
/*def cm_eloop_delete*/
    .type cm_eloop_delete, %function
    .func
    .global cm_eloop_delete
    .syntax unified
    .thumb
    .section .text
cm_eloop_delete:
    ldr.w pc ,[pc]
    .word 0x802394fd
    .endfunc
/*def cm_eloop_wait_event*/
    .type cm_eloop_wait_event, %function
    .func
    .global cm_eloop_wait_event
    .syntax unified
    .thumb
    .section .text
cm_eloop_wait_event:
    ldr.w pc ,[pc]
    .word 0x8023952f
    .endfunc
/*def cm_eloop_register_event*/
    .type cm_eloop_register_event, %function
    .func
    .global cm_eloop_register_event
    .syntax unified
    .thumb
    .section .text
cm_eloop_register_event:
    ldr.w pc ,[pc]
    .word 0x802395af
    .endfunc
/*def cm_eloop_unregister_event*/
    .type cm_eloop_unregister_event, %function
    .func
    .global cm_eloop_unregister_event
    .syntax unified
    .thumb
    .section .text
cm_eloop_unregister_event:
    ldr.w pc ,[pc]
    .word 0x8023960d
    .endfunc
/*def cm_eloop_post_event*/
    .type cm_eloop_post_event, %function
    .func
    .global cm_eloop_post_event
    .syntax unified
    .thumb
    .section .text
cm_eloop_post_event:
    ldr.w pc ,[pc]
    .word 0x8023966d
    .endfunc
/*def cm_eloop_default*/
    .type cm_eloop_default, %function
    .func
    .global cm_eloop_default
    .syntax unified
    .thumb
    .section .text
cm_eloop_default:
    ldr.w pc ,[pc]
    .word 0x802394bf
    .endfunc
/*def cm_get_ipv4v6_local_addr*/
    .type cm_get_ipv4v6_local_addr, %function
    .func
    .global cm_get_ipv4v6_local_addr
    .syntax unified
    .thumb
    .section .text
cm_get_ipv4v6_local_addr:
    ldr.w pc ,[pc]
    .word 0x80233bdf
    .endfunc
/*def lwip_accept*/
    .type lwip_accept, %function
    .func
    .global lwip_accept
    .syntax unified
    .thumb
    .section .text
lwip_accept:
    ldr.w pc ,[pc]
    .word 0x801aecd9
    .endfunc
/*def lwip_bind*/
    .type lwip_bind, %function
    .func
    .global lwip_bind
    .syntax unified
    .thumb
    .section .text
lwip_bind:
    ldr.w pc ,[pc]
    .word 0x801aeed5
    .endfunc
/*def lwip_shutdown*/
    .type lwip_shutdown, %function
    .func
    .global lwip_shutdown
    .syntax unified
    .thumb
    .section .text
lwip_shutdown:
    ldr.w pc ,[pc]
    .word 0x801b1545
    .endfunc
/*def lwip_shutdown2*/
    .type lwip_shutdown2, %function
    .func
    .global lwip_shutdown2
    .syntax unified
    .thumb
    .section .text
lwip_shutdown2:
    ldr.w pc ,[pc]
    .word 0x801b15f1
    .endfunc
/*def lwip_getsockname*/
    .type lwip_getsockname, %function
    .func
    .global lwip_getsockname
    .syntax unified
    .thumb
    .section .text
lwip_getsockname:
    ldr.w pc ,[pc]
    .word 0x801b16c3
    .endfunc
/*def lwip_getpeername*/
    .type lwip_getpeername, %function
    .func
    .global lwip_getpeername
    .syntax unified
    .thumb
    .section .text
lwip_getpeername:
    ldr.w pc ,[pc]
    .word 0x801b16bd
    .endfunc
/*def lwip_setsockopt*/
    .type lwip_setsockopt, %function
    .func
    .global lwip_setsockopt
    .syntax unified
    .thumb
    .section .text
lwip_setsockopt:
    ldr.w pc ,[pc]
    .word 0x801b0cf5
    .endfunc
/*def lwip_getsockopt*/
    .type lwip_getsockopt, %function
    .func
    .global lwip_getsockopt
    .syntax unified
    .thumb
    .section .text
lwip_getsockopt:
    ldr.w pc ,[pc]
    .word 0x801b06bd
    .endfunc
/*def lwip_close*/
    .type lwip_close, %function
    .func
    .global lwip_close
    .syntax unified
    .thumb
    .section .text
lwip_close:
    ldr.w pc ,[pc]
    .word 0x801aefc7
    .endfunc
/*def lwip_close2*/
    .type lwip_close2, %function
    .func
    .global lwip_close2
    .syntax unified
    .thumb
    .section .text
lwip_close2:
    ldr.w pc ,[pc]
    .word 0x801b1495
    .endfunc
/*def lwip_connect*/
    .type lwip_connect, %function
    .func
    .global lwip_connect
    .syntax unified
    .thumb
    .section .text
lwip_connect:
    ldr.w pc ,[pc]
    .word 0x801af065
    .endfunc
/*def lwip_listen*/
    .type lwip_listen, %function
    .func
    .global lwip_listen
    .syntax unified
    .thumb
    .section .text
lwip_listen:
    ldr.w pc ,[pc]
    .word 0x801af167
    .endfunc
/*def lwip_recv*/
    .type lwip_recv, %function
    .func
    .global lwip_recv
    .syntax unified
    .thumb
    .section .text
lwip_recv:
    ldr.w pc ,[pc]
    .word 0x801af777
    .endfunc
/*def lwip_read*/
    .type lwip_read, %function
    .func
    .global lwip_read
    .syntax unified
    .thumb
    .section .text
lwip_read:
    ldr.w pc ,[pc]
    .word 0x801af5fd
    .endfunc
/*def lwip_recvfrom*/
    .type lwip_recvfrom, %function
    .func
    .global lwip_recvfrom
    .syntax unified
    .thumb
    .section .text
lwip_recvfrom:
    ldr.w pc ,[pc]
    .word 0x801af359
    .endfunc
/*def lwip_send*/
    .type lwip_send, %function
    .func
    .global lwip_send
    .syntax unified
    .thumb
    .section .text
lwip_send:
    ldr.w pc ,[pc]
    .word 0x801afb19
    .endfunc
/*def lwip_sendto*/
    .type lwip_sendto, %function
    .func
    .global lwip_sendto
    .syntax unified
    .thumb
    .section .text
lwip_sendto:
    ldr.w pc ,[pc]
    .word 0x801af957
    .endfunc
/*def lwip_socket*/
    .type lwip_socket, %function
    .func
    .global lwip_socket
    .syntax unified
    .thumb
    .section .text
lwip_socket:
    ldr.w pc ,[pc]
    .word 0x801afda3
    .endfunc
/*def lwip_socket_with_callback*/
    .type lwip_socket_with_callback, %function
    .func
    .global lwip_socket_with_callback
    .syntax unified
    .thumb
    .section .text
lwip_socket_with_callback:
    ldr.w pc ,[pc]
    .word 0x801b14b9
    .endfunc
/*def lwip_write*/
    .type lwip_write, %function
    .func
    .global lwip_write
    .syntax unified
    .thumb
    .section .text
lwip_write:
    ldr.w pc ,[pc]
    .word 0x801afe83
    .endfunc
/*def lwip_select*/
    .type lwip_select, %function
    .func
    .global lwip_select
    .syntax unified
    .thumb
    .section .text
lwip_select:
    ldr.w pc ,[pc]
    .word 0x801b00b1
    .endfunc
/*def lwip_ioctl*/
    .type lwip_ioctl, %function
    .func
    .global lwip_ioctl
    .syntax unified
    .thumb
    .section .text
lwip_ioctl:
    ldr.w pc ,[pc]
    .word 0x801b089f
    .endfunc
/*def lwip_fcntl*/
    .type lwip_fcntl, %function
    .func
    .global lwip_fcntl
    .syntax unified
    .thumb
    .section .text
lwip_fcntl:
    ldr.w pc ,[pc]
    .word 0x801b16c9
    .endfunc
/*def lwip_eventfd*/
    .type lwip_eventfd, %function
    .func
    .global lwip_eventfd
    .syntax unified
    .thumb
    .section .text
lwip_eventfd:
    ldr.w pc ,[pc]
    .word 0x801b14f9
    .endfunc
/*def lwip_getthreaderrno*/
    .type lwip_getthreaderrno, %function
    .func
    .global lwip_getthreaderrno
    .syntax unified
    .thumb
    .section .text
lwip_getthreaderrno:
    ldr.w pc ,[pc]
    .word 0x801b1711
    .endfunc
/*def lwip_getsockvalid*/
    .type lwip_getsockvalid, %function
    .func
    .global lwip_getsockvalid
    .syntax unified
    .thumb
    .section .text
lwip_getsockvalid:
    ldr.w pc ,[pc]
    .word 0x801b0f55
    .endfunc
/*def lwip_getsockerrno*/
    .type lwip_getsockerrno, %function
    .func
    .global lwip_getsockerrno
    .syntax unified
    .thumb
    .section .text
lwip_getsockerrno:
    ldr.w pc ,[pc]
    .word 0x801b0f67
    .endfunc
/*def lwip_getsocktype*/
    .type lwip_getsocktype, %function
    .func
    .global lwip_getsocktype
    .syntax unified
    .thumb
    .section .text
lwip_getsocktype:
    ldr.w pc ,[pc]
    .word 0x801b172f
    .endfunc
/*def lwip_getsocknetconn*/
    .type lwip_getsocknetconn, %function
    .func
    .global lwip_getsocknetconn
    .syntax unified
    .thumb
    .section .text
lwip_getsocknetconn:
    ldr.w pc ,[pc]
    .word 0x801aeabf
    .endfunc
/*def lwip_getsocktcppcb*/
    .type lwip_getsocktcppcb, %function
    .func
    .global lwip_getsocktcppcb
    .syntax unified
    .thumb
    .section .text
lwip_getsocktcppcb:
    ldr.w pc ,[pc]
    .word 0x801b1747
    .endfunc
/*def lwip_getsockudppcb*/
    .type lwip_getsockudppcb, %function
    .func
    .global lwip_getsockudppcb
    .syntax unified
    .thumb
    .section .text
lwip_getsockudppcb:
    ldr.w pc ,[pc]
    .word 0x801b1769
    .endfunc
/*def lwip_get_sock_localport*/
    .type lwip_get_sock_localport, %function
    .func
    .global lwip_get_sock_localport
    .syntax unified
    .thumb
    .section .text
lwip_get_sock_localport:
    ldr.w pc ,[pc]
    .word 0x801b178b
    .endfunc
/*def lwip_get_sock_remoteport*/
    .type lwip_get_sock_remoteport, %function
    .func
    .global lwip_get_sock_remoteport
    .syntax unified
    .thumb
    .section .text
lwip_get_sock_remoteport:
    ldr.w pc ,[pc]
    .word 0x801b17d7
    .endfunc
/*def lwip_get_sock_isipv6*/
    .type lwip_get_sock_isipv6, %function
    .func
    .global lwip_get_sock_isipv6
    .syntax unified
    .thumb
    .section .text
lwip_get_sock_isipv6:
    ldr.w pc ,[pc]
    .word 0x801b1829
    .endfunc
/*def lwip_get_sock_pdu_offset*/
    .type lwip_get_sock_pdu_offset, %function
    .func
    .global lwip_get_sock_pdu_offset
    .syntax unified
    .thumb
    .section .text
lwip_get_sock_pdu_offset:
    ldr.w pc ,[pc]
    .word 0x801b1847
    .endfunc
/*def lwip_trigger_process*/
    .type lwip_trigger_process, %function
    .func
    .global lwip_trigger_process
    .syntax unified
    .thumb
    .section .text
lwip_trigger_process:
    ldr.w pc ,[pc]
    .word 0x801b10c7
    .endfunc
/*def lwip_get_sock_localipXaddr*/
    .type lwip_get_sock_localipXaddr, %function
    .func
    .global lwip_get_sock_localipXaddr
    .syntax unified
    .thumb
    .section .text
lwip_get_sock_localipXaddr:
    ldr.w pc ,[pc]
    .word 0x801b17c3
    .endfunc
/*def lwip_get_sock_remoteipXaddr*/
    .type lwip_get_sock_remoteipXaddr, %function
    .func
    .global lwip_get_sock_remoteipXaddr
    .syntax unified
    .thumb
    .section .text
lwip_get_sock_remoteipXaddr:
    ldr.w pc ,[pc]
    .word 0x801b180f
    .endfunc
/*def lwip_get_sock_vlanid*/
    .type lwip_get_sock_vlanid, %function
    .func
    .global lwip_get_sock_vlanid
    .syntax unified
    .thumb
    .section .text
lwip_get_sock_vlanid:
    ldr.w pc ,[pc]
    .word 0x801b1885
    .endfunc
/*def readn*/
    .type readn, %function
    .func
    .global readn
    .syntax unified
    .thumb
    .section .text
readn:
    ldr.w pc ,[pc]
    .word 0x801b18ad
    .endfunc
/*def writen*/
    .type writen, %function
    .func
    .global writen
    .syntax unified
    .thumb
    .section .text
writen:
    ldr.w pc ,[pc]
    .word 0x801b18f5
    .endfunc
/*def readline*/
    .type readline, %function
    .func
    .global readline
    .syntax unified
    .thumb
    .section .text
readline:
    ldr.w pc ,[pc]
    .word 0x801b1961
    .endfunc
/*def lwip_readv*/
    .type lwip_readv, %function
    .func
    .global lwip_readv
    .syntax unified
    .thumb
    .section .text
lwip_readv:
    ldr.w pc ,[pc]
    .word 0x801b1499
    .endfunc
/*def lwip_writev*/
    .type lwip_writev, %function
    .func
    .global lwip_writev
    .syntax unified
    .thumb
    .section .text
lwip_writev:
    ldr.w pc ,[pc]
    .word 0x801b14d7
    .endfunc
/*def lwip_recvmsg*/
    .type lwip_recvmsg, %function
    .func
    .global lwip_recvmsg
    .syntax unified
    .thumb
    .section .text
lwip_recvmsg:
    ldr.w pc ,[pc]
    .word 0x801af60b
    .endfunc
/*def lwip_sendmsg*/
    .type lwip_sendmsg, %function
    .func
    .global lwip_sendmsg
    .syntax unified
    .thumb
    .section .text
lwip_sendmsg:
    ldr.w pc ,[pc]
    .word 0x801afbad
    .endfunc
/*def ipaddr_addr*/
    .type ipaddr_addr, %function
    .func
    .global ipaddr_addr
    .syntax unified
    .thumb
    .section .text
ipaddr_addr:
    ldr.w pc ,[pc]
    .word 0x801aa77d
    .endfunc
/*def ipaddr_aton*/
    .type ipaddr_aton, %function
    .func
    .global ipaddr_aton
    .syntax unified
    .thumb
    .section .text
ipaddr_aton:
    ldr.w pc ,[pc]
    .word 0x801aa66d
    .endfunc
/*def ipaddr_ntoa*/
    .type ipaddr_ntoa, %function
    .func
    .global ipaddr_ntoa
    .syntax unified
    .thumb
    .section .text
ipaddr_ntoa:
    ldr.w pc ,[pc]
    .word 0x801aa807
    .endfunc
/*def ip6addr_aton*/
    .type ip6addr_aton, %function
    .func
    .global ip6addr_aton
    .syntax unified
    .thumb
    .section .text
ip6addr_aton:
    ldr.w pc ,[pc]
    .word 0x801aa855
    .endfunc
/*def ip6addr_ntoa*/
    .type ip6addr_ntoa, %function
    .func
    .global ip6addr_ntoa
    .syntax unified
    .thumb
    .section .text
ip6addr_ntoa:
    ldr.w pc ,[pc]
    .word 0x801aaa3b
    .endfunc
/*def inet_ntop*/
    .type inet_ntop, %function
    .func
    .global inet_ntop
    .syntax unified
    .thumb
    .section .text
inet_ntop:
    ldr.w pc ,[pc]
    .word 0x801beb9d
    .endfunc
/*def inet_pton*/
    .type inet_pton, %function
    .func
    .global inet_pton
    .syntax unified
    .thumb
    .section .text
inet_pton:
    ldr.w pc ,[pc]
    .word 0x801bed4d
    .endfunc
/*def lwip_gethostbyname*/
    .type lwip_gethostbyname, %function
    .func
    .global lwip_gethostbyname
    .syntax unified
    .thumb
    .section .text
lwip_gethostbyname:
    ldr.w pc ,[pc]
    .word 0x801ae3d9
    .endfunc
/*def lwip_getaddrinfo*/
    .type lwip_getaddrinfo, %function
    .func
    .global lwip_getaddrinfo
    .syntax unified
    .thumb
    .section .text
lwip_getaddrinfo:
    ldr.w pc ,[pc]
    .word 0x801ae921
    .endfunc
/*def lwip_freeaddrinfo*/
    .type lwip_freeaddrinfo, %function
    .func
    .global lwip_freeaddrinfo
    .syntax unified
    .thumb
    .section .text
lwip_freeaddrinfo:
    ldr.w pc ,[pc]
    .word 0x801ae3e1
    .endfunc
/*def get_lwip_errno*/
    .type get_lwip_errno, %function
    .func
    .global get_lwip_errno
    .syntax unified
    .thumb
    .section .text
get_lwip_errno:
    ldr.w pc ,[pc]
    .word 0x80262533
    .endfunc
/*def cm_fota_set_url*/
    .type cm_fota_set_url, %function
    .func
    .global cm_fota_set_url
    .syntax unified
    .thumb
    .section .text
cm_fota_set_url:
    ldr.w pc ,[pc]
    .word 0x801999d9
    .endfunc
/*def cm_fota_read_config*/
    .type cm_fota_read_config, %function
    .func
    .global cm_fota_read_config
    .syntax unified
    .thumb
    .section .text
cm_fota_read_config:
    ldr.w pc ,[pc]
    .word 0x80199be7
    .endfunc
/*def cm_fota_exec_upgrade*/
    .type cm_fota_exec_upgrade, %function
    .func
    .global cm_fota_exec_upgrade
    .syntax unified
    .thumb
    .section .text
cm_fota_exec_upgrade:
    ldr.w pc ,[pc]
    .word 0x80199c87
    .endfunc
/*def cm_fota_set_reboot_time*/
    .type cm_fota_set_reboot_time, %function
    .func
    .global cm_fota_set_reboot_time
    .syntax unified
    .thumb
    .section .text
cm_fota_set_reboot_time:
    ldr.w pc ,[pc]
    .word 0x80199be3
    .endfunc
/*def cm_fota_res_callback_register*/
    .type cm_fota_res_callback_register, %function
    .func
    .global cm_fota_res_callback_register
    .syntax unified
    .thumb
    .section .text
cm_fota_res_callback_register:
    ldr.w pc ,[pc]
    .word 0x8019a28f
    .endfunc
/*def cm_ota_set_otasize*/
    .type cm_ota_set_otasize, %function
    .func
    .global cm_ota_set_otasize
    .syntax unified
    .thumb
    .section .text
cm_ota_set_otasize:
    ldr.w pc ,[pc]
    .word 0x8019a2e7
    .endfunc
/*def cm_ota_get_capacity*/
    .type cm_ota_get_capacity, %function
    .func
    .global cm_ota_get_capacity
    .syntax unified
    .thumb
    .section .text
cm_ota_get_capacity:
    ldr.w pc ,[pc]
    .word 0x8019a5ef
    .endfunc
/*def cm_ota_init*/
    .type cm_ota_init, %function
    .func
    .global cm_ota_init
    .syntax unified
    .thumb
    .section .text
cm_ota_init:
    ldr.w pc ,[pc]
    .word 0x80199c73
    .endfunc
/*def cm_ota_firmware_write*/
    .type cm_ota_firmware_write, %function
    .func
    .global cm_ota_firmware_write
    .syntax unified
    .thumb
    .section .text
cm_ota_firmware_write:
    ldr.w pc ,[pc]
    .word 0x8019a2cd
    .endfunc
/*def cm_ota_get_written_size*/
    .type cm_ota_get_written_size, %function
    .func
    .global cm_ota_get_written_size
    .syntax unified
    .thumb
    .section .text
cm_ota_get_written_size:
    ldr.w pc ,[pc]
    .word 0x8019a8bb
    .endfunc
/*def cm_ota_firmware_erase*/
    .type cm_ota_firmware_erase, %function
    .func
    .global cm_ota_firmware_erase
    .syntax unified
    .thumb
    .section .text
cm_ota_firmware_erase:
    ldr.w pc ,[pc]
    .word 0x8019a8c1
    .endfunc
/*def cm_ota_upgrade*/
    .type cm_ota_upgrade, %function
    .func
    .global cm_ota_upgrade
    .syntax unified
    .thumb
    .section .text
cm_ota_upgrade:
    ldr.w pc ,[pc]
    .word 0x80199c43
    .endfunc
/*def cm_httpclient_uri_encode*/
    .type cm_httpclient_uri_encode, %function
    .func
    .global cm_httpclient_uri_encode
    .syntax unified
    .thumb
    .section .text
cm_httpclient_uri_encode:
    ldr.w pc ,[pc]
    .word 0x80236b35
    .endfunc
/*def cm_httpclient_uri_encode_component*/
    .type cm_httpclient_uri_encode_component, %function
    .func
    .global cm_httpclient_uri_encode_component
    .syntax unified
    .thumb
    .section .text
cm_httpclient_uri_encode_component:
    ldr.w pc ,[pc]
    .word 0x80236b63
    .endfunc
/*def cm_httpclient_create*/
    .type cm_httpclient_create, %function
    .func
    .global cm_httpclient_create
    .syntax unified
    .thumb
    .section .text
cm_httpclient_create:
    ldr.w pc ,[pc]
    .word 0x80236bdb
    .endfunc
/*def cm_httpclient_delete*/
    .type cm_httpclient_delete, %function
    .func
    .global cm_httpclient_delete
    .syntax unified
    .thumb
    .section .text
cm_httpclient_delete:
    ldr.w pc ,[pc]
    .word 0x80236bad
    .endfunc
/*def cm_httpclient_is_busy*/
    .type cm_httpclient_is_busy, %function
    .func
    .global cm_httpclient_is_busy
    .syntax unified
    .thumb
    .section .text
cm_httpclient_is_busy:
    ldr.w pc ,[pc]
    .word 0x80236b95
    .endfunc
/*def cm_httpclient_set_cfg*/
    .type cm_httpclient_set_cfg, %function
    .func
    .global cm_httpclient_set_cfg
    .syntax unified
    .thumb
    .section .text
cm_httpclient_set_cfg:
    ldr.w pc ,[pc]
    .word 0x80236cb1
    .endfunc
/*def cm_httpclient_terminate*/
    .type cm_httpclient_terminate, %function
    .func
    .global cm_httpclient_terminate
    .syntax unified
    .thumb
    .section .text
cm_httpclient_terminate:
    ldr.w pc ,[pc]
    .word 0x80236d01
    .endfunc
/*def cm_httpclient_custom_header_set*/
    .type cm_httpclient_custom_header_set, %function
    .func
    .global cm_httpclient_custom_header_set
    .syntax unified
    .thumb
    .section .text
cm_httpclient_custom_header_set:
    ldr.w pc ,[pc]
    .word 0x80236d37
    .endfunc
/*def cm_httpclient_custom_header_free*/
    .type cm_httpclient_custom_header_free, %function
    .func
    .global cm_httpclient_custom_header_free
    .syntax unified
    .thumb
    .section .text
cm_httpclient_custom_header_free:
    ldr.w pc ,[pc]
    .word 0x80236d5d
    .endfunc
/*def cm_httpclient_specific_header_set*/
    .type cm_httpclient_specific_header_set, %function
    .func
    .global cm_httpclient_specific_header_set
    .syntax unified
    .thumb
    .section .text
cm_httpclient_specific_header_set:
    ldr.w pc ,[pc]
    .word 0x80236d89
    .endfunc
/*def cm_httpclient_specific_header_free*/
    .type cm_httpclient_specific_header_free, %function
    .func
    .global cm_httpclient_specific_header_free
    .syntax unified
    .thumb
    .section .text
cm_httpclient_specific_header_free:
    ldr.w pc ,[pc]
    .word 0x80236daf
    .endfunc
/*def cm_httpclient_get_response_code*/
    .type cm_httpclient_get_response_code, %function
    .func
    .global cm_httpclient_get_response_code
    .syntax unified
    .thumb
    .section .text
cm_httpclient_get_response_code:
    ldr.w pc ,[pc]
    .word 0x802370d7
    .endfunc
/*def cm_httpclient_parse_header*/
    .type cm_httpclient_parse_header, %function
    .func
    .global cm_httpclient_parse_header
    .syntax unified
    .thumb
    .section .text
cm_httpclient_parse_header:
    ldr.w pc ,[pc]
    .word 0x802370eb
    .endfunc
/*def cm_httpclient_sync_request*/
    .type cm_httpclient_sync_request, %function
    .func
    .global cm_httpclient_sync_request
    .syntax unified
    .thumb
    .section .text
cm_httpclient_sync_request:
    ldr.w pc ,[pc]
    .word 0x8023711f
    .endfunc
/*def cm_httpclient_sync_free_data*/
    .type cm_httpclient_sync_free_data, %function
    .func
    .global cm_httpclient_sync_free_data
    .syntax unified
    .thumb
    .section .text
cm_httpclient_sync_free_data:
    ldr.w pc ,[pc]
    .word 0x8023727d
    .endfunc
/*def cm_httpclient_request_start*/
    .type cm_httpclient_request_start, %function
    .func
    .global cm_httpclient_request_start
    .syntax unified
    .thumb
    .section .text
cm_httpclient_request_start:
    ldr.w pc ,[pc]
    .word 0x80236ddb
    .endfunc
/*def cm_httpclient_request_send*/
    .type cm_httpclient_request_send, %function
    .func
    .global cm_httpclient_request_send
    .syntax unified
    .thumb
    .section .text
cm_httpclient_request_send:
    ldr.w pc ,[pc]
    .word 0x80236e97
    .endfunc
/*def cm_httpclient_request_end*/
    .type cm_httpclient_request_end, %function
    .func
    .global cm_httpclient_request_end
    .syntax unified
    .thumb
    .section .text
cm_httpclient_request_end:
    ldr.w pc ,[pc]
    .word 0x8023706b
    .endfunc
/*def cm_i2c_open*/
    .type cm_i2c_open, %function
    .func
    .global cm_i2c_open
    .syntax unified
    .thumb
    .section .text
cm_i2c_open:
    ldr.w pc ,[pc]
    .word 0x80265265
    .endfunc
/*def cm_i2c_close*/
    .type cm_i2c_close, %function
    .func
    .global cm_i2c_close
    .syntax unified
    .thumb
    .section .text
cm_i2c_close:
    ldr.w pc ,[pc]
    .word 0x8026537d
    .endfunc
/*def cm_i2c_write*/
    .type cm_i2c_write, %function
    .func
    .global cm_i2c_write
    .syntax unified
    .thumb
    .section .text
cm_i2c_write:
    ldr.w pc ,[pc]
    .word 0x802653a7
    .endfunc
/*def cm_i2c_read*/
    .type cm_i2c_read, %function
    .func
    .global cm_i2c_read
    .syntax unified
    .thumb
    .section .text
cm_i2c_read:
    ldr.w pc ,[pc]
    .word 0x802653f5
    .endfunc
/*def cm_adc_read*/
    .type cm_adc_read, %function
    .func
    .global cm_adc_read
    .syntax unified
    .thumb
    .section .text
cm_adc_read:
    ldr.w pc ,[pc]
    .word 0x802651f9
    .endfunc
/*def cm_adc_vbat_read*/
    .type cm_adc_vbat_read, %function
    .func
    .global cm_adc_vbat_read
    .syntax unified
    .thumb
    .section .text
cm_adc_vbat_read:
    ldr.w pc ,[pc]
    .word 0x80265219
    .endfunc
/*def cm_spi_open*/
    .type cm_spi_open, %function
    .func
    .global cm_spi_open
    .syntax unified
    .thumb
    .section .text
cm_spi_open:
    ldr.w pc ,[pc]
    .word 0x802655bd
    .endfunc
/*def cm_spi_close*/
    .type cm_spi_close, %function
    .func
    .global cm_spi_close
    .syntax unified
    .thumb
    .section .text
cm_spi_close:
    ldr.w pc ,[pc]
    .word 0x80265777
    .endfunc
/*def cm_spi_write*/
    .type cm_spi_write, %function
    .func
    .global cm_spi_write
    .syntax unified
    .thumb
    .section .text
cm_spi_write:
    ldr.w pc ,[pc]
    .word 0x802657a7
    .endfunc
/*def cm_spi_read*/
    .type cm_spi_read, %function
    .func
    .global cm_spi_read
    .syntax unified
    .thumb
    .section .text
cm_spi_read:
    ldr.w pc ,[pc]
    .word 0x8026580b
    .endfunc
/*def cm_spi_write_then_read*/
    .type cm_spi_write_then_read, %function
    .func
    .global cm_spi_write_then_read
    .syntax unified
    .thumb
    .section .text
cm_spi_write_then_read:
    ldr.w pc ,[pc]
    .word 0x80265873
    .endfunc
/*def cm_ssl_setopt*/
    .type cm_ssl_setopt, %function
    .func
    .global cm_ssl_setopt
    .syntax unified
    .thumb
    .section .text
cm_ssl_setopt:
    ldr.w pc ,[pc]
    .word 0x802637b9
    .endfunc
/*def cm_ssl_close*/
    .type cm_ssl_close, %function
    .func
    .global cm_ssl_close
    .syntax unified
    .thumb
    .section .text
cm_ssl_close:
    ldr.w pc ,[pc]
    .word 0x8025f725
    .endfunc
/*def cm_ssl_write*/
    .type cm_ssl_write, %function
    .func
    .global cm_ssl_write
    .syntax unified
    .thumb
    .section .text
cm_ssl_write:
    ldr.w pc ,[pc]
    .word 0x8025f8e9
    .endfunc
/*def cm_ssl_read*/
    .type cm_ssl_read, %function
    .func
    .global cm_ssl_read
    .syntax unified
    .thumb
    .section .text
cm_ssl_read:
    ldr.w pc ,[pc]
    .word 0x8025f8f7
    .endfunc
/*def cm_ssl_check_pending*/
    .type cm_ssl_check_pending, %function
    .func
    .global cm_ssl_check_pending
    .syntax unified
    .thumb
    .section .text
cm_ssl_check_pending:
    ldr.w pc ,[pc]
    .word 0x8025f953
    .endfunc
/*def cm_ssl_get_bytes_avail*/
    .type cm_ssl_get_bytes_avail, %function
    .func
    .global cm_ssl_get_bytes_avail
    .syntax unified
    .thumb
    .section .text
cm_ssl_get_bytes_avail:
    ldr.w pc ,[pc]
    .word 0x8025f983
    .endfunc
/*def cm_ssl_conn_with_host*/
    .type cm_ssl_conn_with_host, %function
    .func
    .global cm_ssl_conn_with_host
    .syntax unified
    .thumb
    .section .text
cm_ssl_conn_with_host:
    ldr.w pc ,[pc]
    .word 0x8025f833
    .endfunc
/*def cm_ssl_list_cipher*/
    .type cm_ssl_list_cipher, %function
    .func
    .global cm_ssl_list_cipher
    .syntax unified
    .thumb
    .section .text
cm_ssl_list_cipher:
    ldr.w pc ,[pc]
    .word 0x8025f8e5
    .endfunc
/*def cm_cpuinfo_record_start*/
    .type cm_cpuinfo_record_start, %function
    .func
    .global cm_cpuinfo_record_start
    .syntax unified
    .thumb
    .section .text
cm_cpuinfo_record_start:
    ldr.w pc ,[pc]
    .word 0x80265079
    .endfunc
/*def cm_cpuinfo_record_read*/
    .type cm_cpuinfo_record_read, %function
    .func
    .global cm_cpuinfo_record_read
    .syntax unified
    .thumb
    .section .text
cm_cpuinfo_record_read:
    ldr.w pc ,[pc]
    .word 0x802650c3
    .endfunc
/*def cm_cpuinfo_record_stop*/
    .type cm_cpuinfo_record_stop, %function
    .func
    .global cm_cpuinfo_record_stop
    .syntax unified
    .thumb
    .section .text
cm_cpuinfo_record_stop:
    ldr.w pc ,[pc]
    .word 0x80265161
    .endfunc
/*def cm_cpuinfo_get_cpuid*/
    .type cm_cpuinfo_get_cpuid, %function
    .func
    .global cm_cpuinfo_get_cpuid
    .syntax unified
    .thumb
    .section .text
cm_cpuinfo_get_cpuid:
    ldr.w pc ,[pc]
    .word 0x80265177
    .endfunc
/*def cm_audio_play_set_cfg*/
    .type cm_audio_play_set_cfg, %function
    .func
    .global cm_audio_play_set_cfg
    .syntax unified
    .thumb
    .section .text
cm_audio_play_set_cfg:
    ldr.w pc ,[pc]
    .word 0x80261941
    .endfunc
/*def cm_audio_play_get_cfg*/
    .type cm_audio_play_get_cfg, %function
    .func
    .global cm_audio_play_get_cfg
    .syntax unified
    .thumb
    .section .text
cm_audio_play_get_cfg:
    ldr.w pc ,[pc]
    .word 0x80261957
    .endfunc
/*def cm_audio_play_file*/
    .type cm_audio_play_file, %function
    .func
    .global cm_audio_play_file
    .syntax unified
    .thumb
    .section .text
cm_audio_play_file:
    ldr.w pc ,[pc]
    .word 0x80261987
    .endfunc
/*def cm_audio_player_pause*/
    .type cm_audio_player_pause, %function
    .func
    .global cm_audio_player_pause
    .syntax unified
    .thumb
    .section .text
cm_audio_player_pause:
    ldr.w pc ,[pc]
    .word 0x80261a19
    .endfunc
/*def cm_audio_player_resume*/
    .type cm_audio_player_resume, %function
    .func
    .global cm_audio_player_resume
    .syntax unified
    .thumb
    .section .text
cm_audio_player_resume:
    ldr.w pc ,[pc]
    .word 0x80261a85
    .endfunc
/*def cm_audio_player_stop*/
    .type cm_audio_player_stop, %function
    .func
    .global cm_audio_player_stop
    .syntax unified
    .thumb
    .section .text
cm_audio_player_stop:
    ldr.w pc ,[pc]
    .word 0x80261af1
    .endfunc
/*def cm_audio_player_stream_open*/
    .type cm_audio_player_stream_open, %function
    .func
    .global cm_audio_player_stream_open
    .syntax unified
    .thumb
    .section .text
cm_audio_player_stream_open:
    ldr.w pc ,[pc]
    .word 0x80261bab
    .endfunc
/*def cm_audio_player_stream_push*/
    .type cm_audio_player_stream_push, %function
    .func
    .global cm_audio_player_stream_push
    .syntax unified
    .thumb
    .section .text
cm_audio_player_stream_push:
    ldr.w pc ,[pc]
    .word 0x80261e89
    .endfunc
/*def cm_audio_player_stream_close*/
    .type cm_audio_player_stream_close, %function
    .func
    .global cm_audio_player_stream_close
    .syntax unified
    .thumb
    .section .text
cm_audio_player_stream_close:
    ldr.w pc ,[pc]
    .word 0x80261b53
    .endfunc
/*def cm_audio_player_stream_clear_close*/
    .type cm_audio_player_stream_clear_close, %function
    .func
    .global cm_audio_player_stream_clear_close
    .syntax unified
    .thumb
    .section .text
cm_audio_player_stream_clear_close:
    ldr.w pc ,[pc]
    .word 0x80261f07
    .endfunc
/*def cm_audio_player_stream_cb_reg*/
    .type cm_audio_player_stream_cb_reg, %function
    .func
    .global cm_audio_player_stream_cb_reg
    .syntax unified
    .thumb
    .section .text
cm_audio_player_stream_cb_reg:
    ldr.w pc ,[pc]
    .word 0x80261b47
    .endfunc
/*def cm_wifiscan_cfg*/
    .type cm_wifiscan_cfg, %function
    .func
    .global cm_wifiscan_cfg
    .syntax unified
    .thumb
    .section .text
cm_wifiscan_cfg:
    ldr.w pc ,[pc]
    .word 0x8023cf63
    .endfunc
/*def cm_wifiscan_start*/
    .type cm_wifiscan_start, %function
    .func
    .global cm_wifiscan_start
    .syntax unified
    .thumb
    .section .text
cm_wifiscan_start:
    ldr.w pc ,[pc]
    .word 0x8023cfaf
    .endfunc
/*def cm_wifiscan_stop*/
    .type cm_wifiscan_stop, %function
    .func
    .global cm_wifiscan_stop
    .syntax unified
    .thumb
    .section .text
cm_wifiscan_stop:
    ldr.w pc ,[pc]
    .word 0x8023cfc7
    .endfunc
/*def cm_wifiscan_query*/
    .type cm_wifiscan_query, %function
    .func
    .global cm_wifiscan_query
    .syntax unified
    .thumb
    .section .text
cm_wifiscan_query:
    ldr.w pc ,[pc]
    .word 0x8023cfd1
    .endfunc
/*def cm_virt_at_send_async*/
    .type cm_virt_at_send_async, %function
    .func
    .global cm_virt_at_send_async
    .syntax unified
    .thumb
    .section .text
cm_virt_at_send_async:
    ldr.w pc ,[pc]
    .word 0x802671a5
    .endfunc
/*def cm_virt_at_send_sync*/
    .type cm_virt_at_send_sync, %function
    .func
    .global cm_virt_at_send_sync
    .syntax unified
    .thumb
    .section .text
cm_virt_at_send_sync:
    ldr.w pc ,[pc]
    .word 0x80267273
    .endfunc
/*def cm_virt_at_urc_cb_reg*/
    .type cm_virt_at_urc_cb_reg, %function
    .func
    .global cm_virt_at_urc_cb_reg
    .syntax unified
    .thumb
    .section .text
cm_virt_at_urc_cb_reg:
    ldr.w pc ,[pc]
    .word 0x802677ed
    .endfunc
/*def cm_virt_at_urc_cb_dereg*/
    .type cm_virt_at_urc_cb_dereg, %function
    .func
    .global cm_virt_at_urc_cb_dereg
    .syntax unified
    .thumb
    .section .text
cm_virt_at_urc_cb_dereg:
    ldr.w pc ,[pc]
    .word 0x802678b3
    .endfunc
/*def cm_sms_send_txt*/
    .type cm_sms_send_txt, %function
    .func
    .global cm_sms_send_txt
    .syntax unified
    .thumb
    .section .text
cm_sms_send_txt:
    ldr.w pc ,[pc]
    .word 0x80265c43
    .endfunc
/*def cm_sms_send_pdu*/
    .type cm_sms_send_pdu, %function
    .func
    .global cm_sms_send_pdu
    .syntax unified
    .thumb
    .section .text
cm_sms_send_pdu:
    ldr.w pc ,[pc]
    .word 0x80265ded
    .endfunc
/*def cm_modem_info_cell*/
    .type cm_modem_info_cell, %function
    .func
    .global cm_modem_info_cell
    .syntax unified
    .thumb
    .section .text
cm_modem_info_cell:
    ldr.w pc ,[pc]
    .word 0x80267a65
    .endfunc
/*def cm_modem_info_radio*/
    .type cm_modem_info_radio, %function
    .func
    .global cm_modem_info_radio
    .syntax unified
    .thumb
    .section .text
cm_modem_info_radio:
    ldr.w pc ,[pc]
    .word 0x80267b5b
    .endfunc
/*def cm_usb2com_register_recv_cb*/
    .type cm_usb2com_register_recv_cb, %function
    .func
    .global cm_usb2com_register_recv_cb
    .syntax unified
    .thumb
    .section .text
cm_usb2com_register_recv_cb:
    ldr.w pc ,[pc]
    .word 0x8023d13d
    .endfunc
/*def cm_usb2com_send_data*/
    .type cm_usb2com_send_data, %function
    .func
    .global cm_usb2com_send_data
    .syntax unified
    .thumb
    .section .text
cm_usb2com_send_data:
    ldr.w pc ,[pc]
    .word 0x8023d143
    .endfunc
/*def cm_usb2com_register_status_cb*/
    .type cm_usb2com_register_status_cb, %function
    .func
    .global cm_usb2com_register_status_cb
    .syntax unified
    .thumb
    .section .text
cm_usb2com_register_status_cb:
    ldr.w pc ,[pc]
    .word 0x8023d17b
    .endfunc
/*def cm_usb2com_get_status*/
    .type cm_usb2com_get_status, %function
    .func
    .global cm_usb2com_get_status
    .syntax unified
    .thumb
    .section .text
cm_usb2com_get_status:
    ldr.w pc ,[pc]
    .word 0x8023d181
    .endfunc
/*def cm_lcd_write_cmd*/
    .type cm_lcd_write_cmd, %function
    .func
    .global cm_lcd_write_cmd
    .syntax unified
    .thumb
    .section .text
cm_lcd_write_cmd:
    ldr.w pc ,[pc]
    .word 0x802663b9
    .endfunc
/*def cm_lcd_write_data*/
    .type cm_lcd_write_data, %function
    .func
    .global cm_lcd_write_data
    .syntax unified
    .thumb
    .section .text
cm_lcd_write_data:
    ldr.w pc ,[pc]
    .word 0x802663ab
    .endfunc
/*def cm_lcd_write_buf*/
    .type cm_lcd_write_buf, %function
    .func
    .global cm_lcd_write_buf
    .syntax unified
    .thumb
    .section .text
cm_lcd_write_buf:
    ldr.w pc ,[pc]
    .word 0x802664f9
    .endfunc
/*def cm_lcd_init*/
    .type cm_lcd_init, %function
    .func
    .global cm_lcd_init
    .syntax unified
    .thumb
    .section .text
cm_lcd_init:
    ldr.w pc ,[pc]
    .word 0x8026641f
    .endfunc
/*def cm_lcd_deinit*/
    .type cm_lcd_deinit, %function
    .func
    .global cm_lcd_deinit
    .syntax unified
    .thumb
    .section .text
cm_lcd_deinit:
    ldr.w pc ,[pc]
    .word 0x802664b3
    .endfunc
/*def cm_lcd_read_id*/
    .type cm_lcd_read_id, %function
    .func
    .global cm_lcd_read_id
    .syntax unified
    .thumb
    .section .text
cm_lcd_read_id:
    ldr.w pc ,[pc]
    .word 0x802664d3
    .endfunc
/*def cm_lwm2m_create*/
    .type cm_lwm2m_create, %function
    .func
    .global cm_lwm2m_create
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_create:
    ldr.w pc ,[pc]
    .word 0x80262899
    .endfunc
/*def cm_lwm2m_delete*/
    .type cm_lwm2m_delete, %function
    .func
    .global cm_lwm2m_delete
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_delete:
    ldr.w pc ,[pc]
    .word 0x80262953
    .endfunc
/*def cm_lwm2m_add_obj*/
    .type cm_lwm2m_add_obj, %function
    .func
    .global cm_lwm2m_add_obj
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_add_obj:
    ldr.w pc ,[pc]
    .word 0x802629b5
    .endfunc
/*def cm_lwm2m_del_obj*/
    .type cm_lwm2m_del_obj, %function
    .func
    .global cm_lwm2m_del_obj
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_del_obj:
    ldr.w pc ,[pc]
    .word 0x80262a2b
    .endfunc
/*def cm_lwm2m_discover*/
    .type cm_lwm2m_discover, %function
    .func
    .global cm_lwm2m_discover
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_discover:
    ldr.w pc ,[pc]
    .word 0x80262a61
    .endfunc
/*def cm_lwm2m_open*/
    .type cm_lwm2m_open, %function
    .func
    .global cm_lwm2m_open
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_open:
    ldr.w pc ,[pc]
    .word 0x80262aff
    .endfunc
/*def cm_lwm2m_update*/
    .type cm_lwm2m_update, %function
    .func
    .global cm_lwm2m_update
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_update:
    ldr.w pc ,[pc]
    .word 0x80262b35
    .endfunc
/*def cm_lwm2m_close*/
    .type cm_lwm2m_close, %function
    .func
    .global cm_lwm2m_close
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_close:
    ldr.w pc ,[pc]
    .word 0x80262b6b
    .endfunc
/*def cm_lwm2m_notify_packing*/
    .type cm_lwm2m_notify_packing, %function
    .func
    .global cm_lwm2m_notify_packing
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_notify_packing:
    ldr.w pc ,[pc]
    .word 0x80262cf1
    .endfunc
/*def cm_lwm2m_notify*/
    .type cm_lwm2m_notify, %function
    .func
    .global cm_lwm2m_notify
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_notify:
    ldr.w pc ,[pc]
    .word 0x80262d95
    .endfunc
/*def cm_lwm2m_read_rsp*/
    .type cm_lwm2m_read_rsp, %function
    .func
    .global cm_lwm2m_read_rsp
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_read_rsp:
    ldr.w pc ,[pc]
    .word 0x80262e8d
    .endfunc
/*def cm_lwm2m_write_rsp*/
    .type cm_lwm2m_write_rsp, %function
    .func
    .global cm_lwm2m_write_rsp
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_write_rsp:
    ldr.w pc ,[pc]
    .word 0x80262ef9
    .endfunc
/*def cm_lwm2m_execute_rsp*/
    .type cm_lwm2m_execute_rsp, %function
    .func
    .global cm_lwm2m_execute_rsp
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_execute_rsp:
    ldr.w pc ,[pc]
    .word 0x80262f31
    .endfunc
/*def cm_lwm2m_param_rsp*/
    .type cm_lwm2m_param_rsp, %function
    .func
    .global cm_lwm2m_param_rsp
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_param_rsp:
    ldr.w pc ,[pc]
    .word 0x80262f69
    .endfunc
/*def cm_lwm2m_observe_rsp*/
    .type cm_lwm2m_observe_rsp, %function
    .func
    .global cm_lwm2m_observe_rsp
    .syntax unified
    .thumb
    .section .text
cm_lwm2m_observe_rsp:
    ldr.w pc ,[pc]
    .word 0x80262fa1
    .endfunc
/*def cm_ping_init*/
    .type cm_ping_init, %function
    .func
    .global cm_ping_init
    .syntax unified
    .thumb
    .section .text
cm_ping_init:
    ldr.w pc ,[pc]
    .word 0x80263a27
    .endfunc
/*def cm_ping_start*/
    .type cm_ping_start, %function
    .func
    .global cm_ping_start
    .syntax unified
    .thumb
    .section .text
cm_ping_start:
    ldr.w pc ,[pc]
    .word 0x80263a9b
    .endfunc
/*def AudioHAL_AifBindCodec_CB*/
    .type AudioHAL_AifBindCodec_CB, %function
    .func
    .global AudioHAL_AifBindCodec_CB
    .syntax unified
    .thumb
    .section .text
AudioHAL_AifBindCodec_CB:
    ldr.w pc ,[pc]
    .word 0x80228399
    .endfunc
/*def vpathSetMode*/
    .type vpathSetMode, %function
    .func
    .global vpathSetMode
    .syntax unified
    .thumb
    .section .text
vpathSetMode:
    ldr.w pc ,[pc]
    .word 0x8025725b
    .endfunc
/*def cm_onewire_init*/
    .type cm_onewire_init, %function
    .func
    .global cm_onewire_init
    .syntax unified
    .thumb
    .section .text
cm_onewire_init:
    ldr.w pc ,[pc]
    .word 0x80265b5d
    .endfunc
/*def cm_onewire_setup_reset*/
    .type cm_onewire_setup_reset, %function
    .func
    .global cm_onewire_setup_reset
    .syntax unified
    .thumb
    .section .text
cm_onewire_setup_reset:
    ldr.w pc ,[pc]
    .word 0x80265b61
    .endfunc
/*def cm_onewire_write_byte*/
    .type cm_onewire_write_byte, %function
    .func
    .global cm_onewire_write_byte
    .syntax unified
    .thumb
    .section .text
cm_onewire_write_byte:
    ldr.w pc ,[pc]
    .word 0x80265b65
    .endfunc
/*def cm_onewire_read_byte*/
    .type cm_onewire_read_byte, %function
    .func
    .global cm_onewire_read_byte
    .syntax unified
    .thumb
    .section .text
cm_onewire_read_byte:
    ldr.w pc ,[pc]
    .word 0x80265b69
    .endfunc
/*def cm_onewire_write_data*/
    .type cm_onewire_write_data, %function
    .func
    .global cm_onewire_write_data
    .syntax unified
    .thumb
    .section .text
cm_onewire_write_data:
    ldr.w pc ,[pc]
    .word 0x80265b6d
    .endfunc
/*def cm_onewire_read_data*/
    .type cm_onewire_read_data, %function
    .func
    .global cm_onewire_read_data
    .syntax unified
    .thumb
    .section .text
cm_onewire_read_data:
    ldr.w pc ,[pc]
    .word 0x80265b71
    .endfunc
