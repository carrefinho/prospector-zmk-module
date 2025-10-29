/*
 * Copyright (c) 2024 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */

#include <zephyr/types.h>
#include <zephyr/init.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/sys/byteorder.h>

#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zmk, CONFIG_ZMK_LOG_LEVEL);

#include <zmk/ble.h>

#include <zmk/events/split_central_status_changed.h>

enum psptr_peripheral_slot_state {
    PERIPHERAL_SLOT_STATE_OPEN,
    PERIPHERAL_SLOT_STATE_CONNECTING,
    PERIPHERAL_SLOT_STATE_CONNECTED,
};

struct psptr_peripheral_slot {
    enum psptr_peripheral_slot_state state;
    struct bt_conn *conn;
};

static struct psptr_peripheral_slot peripherals[ZMK_SPLIT_BLE_PERIPHERAL_COUNT];

static int psptr_peripheral_slot_index_for_conn(struct bt_conn *conn) {
    for (int i = 0; i < ZMK_SPLIT_BLE_PERIPHERAL_COUNT; i++) {
        if (peripherals[i].conn == conn) {
            return i;
        }
    }
    return -EINVAL;
}

static struct psptr_peripheral_slot *psptr_peripheral_slot_for_conn(struct bt_conn *conn) {
    int idx = psptr_peripheral_slot_index_for_conn(conn);
    if (idx < 0) {
        return NULL;
    }
    return &peripherals[idx];
}

static int release_psptr_peripheral_slot(int index) {
    if (index < 0 || index >= ZMK_SPLIT_BLE_PERIPHERAL_COUNT) {
        return -EINVAL;
    }

    struct psptr_peripheral_slot *slot = &peripherals[index];

    if (slot->state == PERIPHERAL_SLOT_STATE_OPEN) {
        return -EINVAL;
    }

    LOG_DBG("Releasing peripheral slot at %d", index);

    if (slot->conn != NULL) {
        slot->conn = NULL;
    }
    slot->state = PERIPHERAL_SLOT_STATE_OPEN;

    return 0;
}

static int reserve_psptr_peripheral_slot_for_conn(struct bt_conn *conn) {
#if IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_PREF_WEAK_BOND)
    for (int i = 0; i < ZMK_SPLIT_BLE_PERIPHERAL_COUNT; i++) {
        if (peripherals[i].state == PERIPHERAL_SLOT_STATE_OPEN) {
            // Be sure the slot is fully reinitialized.
            release_psptr_peripheral_slot(i);
            peripherals[i].conn = conn;
            peripherals[i].state = PERIPHERAL_SLOT_STATE_CONNECTED;
            return i;
        }
    }
#else
    int i = zmk_ble_put_peripheral_addr(bt_conn_get_dst(conn));
    if (i >= 0) {
        if (peripherals[i].state == PERIPHERAL_SLOT_STATE_OPEN) {
            // Be sure the slot is fully reinitialized.
            release_psptr_peripheral_slot(i);
            peripherals[i].conn = conn;
            peripherals[i].state = PERIPHERAL_SLOT_STATE_CONNECTED;
            return i;
        }
    }
#endif // IS_ENABLED(CONFIG_ZMK_SPLIT_BLE_PREF_WEAK_BOND)

    return -ENOMEM;
}

int release_psptr_peripheral_slot_for_conn(struct bt_conn *conn) {
    int idx = psptr_peripheral_slot_index_for_conn(conn);
    if (idx < 0) {
        return idx;
    }

    return release_psptr_peripheral_slot(idx);
}

static void split_central_process_connection(struct bt_conn *conn) {
    LOG_DBG("Current security for connection: %d", bt_conn_get_security(conn));

    struct psptr_peripheral_slot *slot = psptr_peripheral_slot_for_conn(conn);
    if (slot == NULL) {
        LOG_ERR("No peripheral state found for connection");
        return;
    }

    struct bt_conn_info info;

    bt_conn_get_info(conn, &info);

    LOG_DBG("New connection params: Interval: %d, Latency: %d, PHY: %d", info.le.interval,
            info.le.latency, info.le.phy->rx_phy);

    raise_zmk_split_central_status_changed((struct zmk_split_central_status_changed){
        .slot = psptr_peripheral_slot_index_for_conn(conn),
        .connected = true,
    });
}

static void split_central_connected(struct bt_conn *conn, uint8_t conn_err) {
    char addr_str[BT_ADDR_LE_STR_LEN];
    struct bt_conn_info info;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

    bt_conn_get_info(conn, &info);

    if (info.role != BT_CONN_ROLE_CENTRAL) {
        LOG_DBG("SKIPPING FOR ROLE %d", info.role);
        return;
    }

    if (conn_err) {
        LOG_ERR("Failed to connect to %s (%u)", addr_str, conn_err);
        release_psptr_peripheral_slot_for_conn(conn);
        return;
    }

    LOG_DBG("Connected: %s", addr_str);

    int slot_idx = reserve_psptr_peripheral_slot_for_conn(conn);
    if (slot_idx < 0) {
        LOG_ERR("Unable to reserve peripheral slot for connection (err %d)", slot_idx);
        return;
    }

    split_central_process_connection(conn);
}

static void split_central_disconnected(struct bt_conn *conn, uint8_t reason) {
    char addr_str[BT_ADDR_LE_STR_LEN];
    int err;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr_str, sizeof(addr_str));

    LOG_DBG("Disconnected: %s (reason %d)", addr_str, reason);

    raise_zmk_split_central_status_changed((struct zmk_split_central_status_changed){
        .slot = psptr_peripheral_slot_index_for_conn(conn),
        .connected = false,
    });

    // k_msleep(100);

    err = release_psptr_peripheral_slot_for_conn(conn);

    if (err < 0) {
        return;
    }
}

static struct bt_conn_cb conn_callbacks = {
    .connected = split_central_connected,
    .disconnected = split_central_disconnected,
};

static int zmk_split_bt_central_init(void) {
    bt_conn_cb_register(&conn_callbacks);
    return 0;
}

SYS_INIT(zmk_split_bt_central_init, APPLICATION, CONFIG_ZMK_BLE_INIT_PRIORITY);
