Import("env")

env.Append(
    CFLAGS=[
        "-DCONFIG_BT_ENABLED=y",
        "-DCONFIG_BTDM_CTRL_MODE_BLE_ONLY=y",
        "-DCONFIG_BTDM_CTRL_MODE_BR_EDR_ONLY=n",
        "-DCONFIG_BTDM_CTRL_MODE_BTDM=n",
        "-DCONFIG_BT_BLUEDROID_ENABLED=y",
        "-DCONFIG_BT_NIMBLE_ENABLED=n"
    ]
) 