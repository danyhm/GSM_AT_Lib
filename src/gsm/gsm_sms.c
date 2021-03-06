/**	
 * \file            gsm_sms.c
 * \brief           SMS API
 */
 
/*
 * Copyright (c) 2018 Tilen Majerle
 *  
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge,
 * publish, distribute, sublicense, and/or sell copies of the Software, 
 * and to permit persons to whom the Software is furnished to do so, 
 * subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE
 * AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING 
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of GSM-AT library.
 *
 * Author:          Tilen MAJERLE <tilen@majerle.eu>
 */
#include "gsm/gsm_private.h"
#include "gsm/gsm_sms.h"
#include "gsm/gsm_mem.h"

#if GSM_CFG_SMS || __DOXYGEN__

#define GSM_SMS_OPERATION_IDX           0   /*!< Operation index for memory array (read, delete, list) */
#define GSM_SMS_SEND_IDX                1   /*!< Send index for memory array */
#define GSM_SMS_RECEIVE_IDX             2   /*!< Receive index for memory array */

#if !__DOXYGEN__
#define CHECK_ENABLED()                 if (!(check_enabled() == gsmOK)) { return gsmERRNOTENABLED; }
#endif /* !__DOXYGEN__ */

/**
 * \brief           Check if sms is enabled
 * \return          \ref gsmOK on success, member of \ref gsmr_t otherwise
 */
static gsmr_t
check_enabled(void) {
    gsmr_t res;
    GSM_CORE_PROTECT();                         /* Protect core */
    res = gsm.sms.enabled ? gsmOK : gsmERR;
    GSM_CORE_UNPROTECT();                       /* Unprotect core */
    return res;
}

/**
 * \brief           Check if SMS is available
 * \return          \ref gsmOK on success, member of \ref gsmr_t enumeration otherwise
 */
static gsmr_t
check_ready(void) {
    gsmr_t res;
    GSM_CORE_PROTECT();
    res = gsm.sms.ready ? gsmOK : gsmERR;
    GSM_CORE_UNPROTECT();
    return res;
}

/**
 * \brief           Check if input memory is available in modem
 * \param[in]       mem: Memory to test
 * \param[in]       can_curr: Flag indicates if \ref GSM_MEM_CURRENT option can be used
 * \return          \ref gsmOK on success, member of \ref gsmr_t otherwise
 */
static gsmr_t
check_sms_mem(gsm_mem_t mem, uint8_t can_curr) {
    gsmr_t res = gsmERRMEM;
    GSM_CORE_PROTECT();                         /* Protect core */
    if ((mem < GSM_MEM_END && gsm.sms.mem[GSM_SMS_OPERATION_IDX].mem_available & (1 << (uint32_t)mem)) ||
        (can_curr && mem == GSM_MEM_CURRENT)) {
        res = gsmOK;
    }
    GSM_CORE_UNPROTECT();                       /* Unprotect core */
    return res;
}

/**
 * \brief           Enable SMS functionality
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t otherwise
 */
gsmr_t
gsm_sms_enable(uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);                    /* Define variable for message */

    GSM_MSG_VAR_ALLOC(msg);                     /* Allocate memory for variable */
    GSM_MSG_VAR_REF(msg).cmd_def = GSM_CMD_SMS_ENABLE;
    GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CPMS_GET_OPT;

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, blocking, 60000);   /* Send message to producer queue */
}

/**
 * \brief           Disable SMS functionality
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t otherwise
 */
gsmr_t
gsm_sms_disable(uint32_t blocking) {
    GSM_CORE_PROTECT();                         /* Protect core */
    gsm.sms.enabled = 0;                        /* Clear enabled status */
    GSM_CORE_UNPROTECT();                       /* Unprotect core */
    return gsmOK;
}

/**
 * \brief           Send SMS text to phone number
 * \param[in]       num: String number
 * \param[in]       text: Text to send. Maximal `160` characters
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t otherwise
 */
gsmr_t
gsm_sms_send(const char* num, const char* text, uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);                    /* Define variable for message */

    GSM_ASSERT("num != NULL", num != NULL);     /* Assert input parameters */
    GSM_ASSERT("text != NULL && strlen(text) <= 160", 
        num != NULL && strlen(text) <= 160);    /* Assert input parameters */
    CHECK_ENABLED();                            /* Check if enabled */

    GSM_MSG_VAR_ALLOC(msg);                     /* Allocate memory for variable */
    GSM_MSG_VAR_REF(msg).cmd_def = GSM_CMD_CMGS;
    GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CMGF;
    GSM_MSG_VAR_REF(msg).msg.sms_send.num = num;
    GSM_MSG_VAR_REF(msg).msg.sms_send.text = text;
    GSM_MSG_VAR_REF(msg).msg.sms_send.format = 1;   /* Send as plain text */

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, blocking, 60000);   /* Send message to producer queue */
}

/**
 * \brief           Read SMS entry at specific memory and position
 * \param[in]       mem: Memory used to read message from
 * \param[in]       pos: Position number in memory to read
 * \param[out]      entry: Pointer to SMS entry structure to fill data to
 * \param[in]       update: Flag indicates update. Set to `1` to change `UNREAD` messages to `READ` or `0` to leave as is
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t otherwise
 */
gsmr_t
gsm_sms_read(gsm_mem_t mem, size_t pos, gsm_sms_entry_t* entry, uint8_t update, uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);                    /* Define variable for message */

    GSM_ASSERT("sms_entry != NULL", entry != NULL); /* Assert input parameters */
    CHECK_ENABLED();                            /* Check if enabled */
    GSM_ASSERT("mem", check_sms_mem(mem, 1) == gsmOK);  /* Assert input parameters */

    GSM_MSG_VAR_ALLOC(msg);                     /* Allocate memory for variable */

    memset(entry, 0x00, sizeof(*entry));        /* Reset data structure */

    entry->mem = mem;                           /* Set memory */
    entry->pos = pos;                           /* Set device position */
    GSM_MSG_VAR_REF(msg).cmd_def = GSM_CMD_CMGR;
    if (mem == GSM_MEM_CURRENT) {               /* Should be always false */
        GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CPMS_GET;    /* First get memory */
    } else {
        GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CPMS_SET;    /* First set memory */
    }
    GSM_MSG_VAR_REF(msg).msg.sms_read.mem = mem;
    GSM_MSG_VAR_REF(msg).msg.sms_read.pos = pos;
    GSM_MSG_VAR_REF(msg).msg.sms_read.entry = entry;
    GSM_MSG_VAR_REF(msg).msg.sms_read.update = update;
    GSM_MSG_VAR_REF(msg).msg.sms_read.format = 1;   /* Send as plain text */

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, blocking, 60000);   /* Send message to producer queue */
}

/**
 * \brief           Delete SMS entry at specific memory and position
 * \param[in]       mem: Memory used to read message from
 * \param[in]       pos: Position number in memory to read
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t otherwise
 */
gsmr_t
gsm_sms_delete(gsm_mem_t mem, size_t pos, uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);                    /* Define variable for message */

    CHECK_ENABLED();                            /* Check if enabled */
    GSM_ASSERT("mem", check_sms_mem(mem, 1) == gsmOK);  /* Assert input parameters */

    GSM_MSG_VAR_ALLOC(msg);                     /* Allocate memory for variable */
    GSM_MSG_VAR_REF(msg).cmd_def = GSM_CMD_CMGD;
    if (mem == GSM_MEM_CURRENT) {               /* Should be always false */
        GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CPMS_GET;    /* First get memory */
    } else {
        GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CPMS_SET;    /* First set memory */
    }
    GSM_MSG_VAR_REF(msg).msg.sms_delete.mem = mem;
    GSM_MSG_VAR_REF(msg).msg.sms_delete.pos = pos;

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, blocking, 60000);   /* Send message to producer queue */
}

/**
 * \brief           List SMS from SMS memory
 * \param[in]       mem: Memory to read entries from. Use \ref GSM_MEM_CURRENT to read from current memory
 * \param[in]       stat: SMS status to read, either `read`, `unread`, `sent`, `unsent` or `all`
 * \param[out]      entries: Pointer to array to save SMS entries
 * \param[in]       etr: Number of entries to read
 * \param[out]      er: Pointer to output variable to save number of entries in array
 * \param[in]       update: Flag indicates update. Set to `1` to change `UNREAD` messages to `READ` or `0` to leave as is
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t otherwise
 */
gsmr_t
gsm_sms_list(gsm_mem_t mem, gsm_sms_status_t stat, gsm_sms_entry_t* entries, size_t etr, size_t* er, uint8_t update, uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);                    /* Define variable for message */

    GSM_ASSERT("entires != NULL", entries != NULL); /* Assert input parameters */
    GSM_ASSERT("etr > 0", etr > 0);             /* Assert input parameters */
    CHECK_ENABLED();                            /* Check if enabled */
    GSM_ASSERT("mem", check_sms_mem(mem, 1) == gsmOK);  /* Assert input parameters */

    GSM_MSG_VAR_ALLOC(msg);                     /* Allocate memory for variable */

    if (er != NULL) {
        *er = 0;
    }
    memset(entries, 0x00, sizeof(*entries) * etr);  /* Reset data structure */
    GSM_MSG_VAR_REF(msg).cmd_def = GSM_CMD_CMGL;
    if (mem == GSM_MEM_CURRENT) {               /* Should be always false */
        GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CPMS_GET;    /* First get memory */
    } else {
        GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CPMS_SET;    /* First set memory */
    }
    GSM_MSG_VAR_REF(msg).msg.sms_list.mem = mem;
    GSM_MSG_VAR_REF(msg).msg.sms_list.status = stat;
    GSM_MSG_VAR_REF(msg).msg.sms_list.entries = entries;
    GSM_MSG_VAR_REF(msg).msg.sms_list.etr = etr;
    GSM_MSG_VAR_REF(msg).msg.sms_list.er = er;
    GSM_MSG_VAR_REF(msg).msg.sms_list.update = update;
    GSM_MSG_VAR_REF(msg).msg.sms_list.format = 1;   /* Send as plain text */

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, blocking, 60000);   /* Send message to producer queue */
}

/**
 * \brief           Set preferred storage for SMS
 * \param[in]       mem1: Preferred memory for read/delete SMS operations. Use \ref GSM_MEM_CURRENT to keep it as is
 * \param[in]       mem2: Preferred memory for sent/write SMS operations. Use \ref GSM_MEM_CURRENT to keep it as is
 * \param[in]       mem3: Preferred memory for received SMS entries. Use \ref GSM_MEM_CURRENT to keep it as is
 * \param[in]       blocking: Status whether command should be blocking or not
 * \return          \ref gsmOK on success, member of \ref gsmr_t otherwise
 */
gsmr_t
gsm_sms_set_preferred_storage(gsm_mem_t mem1, gsm_mem_t mem2, gsm_mem_t mem3, uint32_t blocking) {
    GSM_MSG_VAR_DEFINE(msg);                    /* Define variable for message */

    CHECK_ENABLED();                            /* Check if enabled */
    GSM_ASSERT("mem1", check_sms_mem(mem1, 1) == gsmOK);/* Assert input parameters */
    GSM_ASSERT("mem2", check_sms_mem(mem2, 1) == gsmOK);/* Assert input parameters */
    GSM_ASSERT("mem3", check_sms_mem(mem3, 1) == gsmOK);/* Assert input parameters */

    GSM_MSG_VAR_ALLOC(msg);                     /* Allocate memory for variable */
    GSM_MSG_VAR_REF(msg).cmd_def = GSM_CMD_CPMS_SET;

    /* In case any of memories is set to current, read current status first from device */
    if (mem1 == GSM_MEM_CURRENT || mem2 == GSM_MEM_CURRENT || mem3 == GSM_MEM_CURRENT) {
        GSM_MSG_VAR_REF(msg).cmd = GSM_CMD_CPMS_GET;
    }
    GSM_MSG_VAR_REF(msg).msg.sms_memory.mem[0] = mem1;
    GSM_MSG_VAR_REF(msg).msg.sms_memory.mem[1] = mem2;
    GSM_MSG_VAR_REF(msg).msg.sms_memory.mem[2] = mem3;

    return gsmi_send_msg_to_producer_mbox(&GSM_MSG_VAR_REF(msg), gsmi_initiate_cmd, blocking, 60000);   /* Send message to producer queue */
}

#endif /* GSM_CFG_SMS || __DOXYGEN__ */
