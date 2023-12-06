/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "bleprph.h"
#include "services/ans/ble_svc_ans.h"

#include "vm.h"

static char decode_buffer[256] = { 5, 0, 't', 'e', 's'};


static void decode_incoming(size_t len) {
    if (len >= 256) {
        MODLOG_DFLT(ERROR, "decode buffer len too large: %u\n", len);
        assert(0);
    }
    if (len < sizeof(uint16_t)) {
        MODLOG_DFLT(ERROR, "decode buffer too small: %u\n", len);
        assert(0);
    }
    MODLOG_DFLT(INFO, "Decoding %u bytes\n", len);

    uint16_t offset = *(uint16_t *)decode_buffer;
    MODLOG_DFLT(INFO, "Offset %u bytes\n", offset);

    const char *data = &decode_buffer[sizeof(uint16_t)];
    uint16_t data_len = len - sizeof(uint16_t);
    if (offset + data_len > PROG_MEM) {
        MODLOG_DFLT(ERROR, "Offset + data overflows prog mem: %u + %u > %u\n", offset, data_len, PROG_MEM);
        assert(0);
    }
    memcpy(program + offset, data, data_len);
    memset(decode_buffer, 0, 256);
}

static void runstate_updated() {

}

static const ble_uuid128_t service_uuid =
     BLE_UUID128_INIT(0xfe, 0x13, 0x7d, 0xc0, 0x39, 0xef, 0x47, 0x37,
                      0x94, 0x0a, 0x16, 0xc5, 0xbd, 0x5e, 0x07, 0x7e);


//static const ble_uuid16_t gatt_svr_svc_generic_uuid = BLE_UUID16_INIT(GATT_SVR_SVC_GENERIC_ATTR);

/* A characteristic that can be subscribed to */
static uint16_t program_handle;
static const ble_uuid128_t program_uuid =
    BLE_UUID128_INIT(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02);


static uint8_t run_state;
static uint16_t run_state_handle;
static const ble_uuid128_t run_state_uuid =
    BLE_UUID128_INIT(0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01);

static int
gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt,
                void *arg);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /*** Service ***/
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &service_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                .uuid = &run_state_uuid.u,
                .access_cb = gatt_svc_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .val_handle = &run_state_handle,
            },
            {
                .uuid = &program_uuid.u,
                .access_cb = gatt_svc_access,
                .flags = BLE_GATT_CHR_F_WRITE,
                .val_handle = &program_handle,
            },
            {
                0, /* No more characteristics in this service. */
            }
        },
    },

    {
        0, /* No more services. */
    },
};

static int
gatt_svr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
               void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

static int
gatt_svc_access(uint16_t conn_handle, uint16_t attr_handle,
                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (attr_handle == run_state_handle) {
            rc = os_mbuf_append(ctxt->om,
                                &run_state,
                                sizeof(run_state));
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
        }
        goto unknown;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        if (attr_handle == run_state_handle) {
            rc = gatt_svr_write(ctxt->om,
                                sizeof(run_state),
                                sizeof(run_state),
                                &run_state, NULL);
            ble_gatts_chr_updated(attr_handle);
            runstate_updated();
            return rc;
        } else if (attr_handle == program_handle) {
            uint16_t buf_len = 0;
            rc = gatt_svr_write(ctxt->om, 2, 256, &decode_buffer, &buf_len);
            decode_incoming(buf_len);
            return rc;
        }
        goto unknown;

    default:
        goto unknown;
    }

unknown:
    /* Unknown characteristic/descriptor;
     * The NimBLE host should not have called this function;
     */
    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                    "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    default:
        assert(0);
        break;
    }
}

int
gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    //ble_svc_ans_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }
    decode_incoming(5);

    return 0;
}
