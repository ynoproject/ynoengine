## ynoproject protocol specifications



#### Basic Data Types:

| Name   | Description                     |
| ------ | ------------------------------- |
| int32  | 4-byte                          |
| uint32 | 4-byte                          |
| int16  | 2-byte                          |
| uint16  | 2-byte                          |
| byte   | 1-byte                          |
| bool | 1-byte |
| string | length (uint32) + following data |
| uuid | 16-byte |



#### S2C (Server to Client) Packets:

SYNC_PLAYERDATA:

| Data Field  | Type   |
| ----------- | ------ |
| host_id     | int32  |
| key         | string |
| uuid        | uuid   |
| rank        | int32  |
| account_bin | int32  |
| badge       | string |

GLOBAL_CHAT:

| Data Field     | Type   |
| -------------- | ------ |
| uuid           | uuid   |
| name           | string |
| sys            | string |
| rank           | int32  |
| account_bin    | int32  |
| badge          | string |
| map_id         | string |
| prev_map_id    | string |
| prev_locations | string |
| x              | int32  |
| y              | int32  |
| msg            | string |

PARTY_CHAT:
| Data Field | Type   |
| ---------- | ------ |
| uuid       | uuid   |
| msg        | string |

CONNECT:

| Data Field  | Type   |
| ----------- | ------ |
| player_id   | int32  |
| uuid        | uuid   |
| rank        | int32  |
| account_bin | int32  |
| badge       | string |

DISCONNECT:

| Data Field | Type  |
| ---------- | ----- |
| player_id  | int32 |

CHAT:

| Data Field | Type   |
| ---------- | ------ |
| player_id  | int32  |
| msg        | string |

MOVE::

| Data Field | Type  |
| ---------- | ----- |
| player_id  | int32 |
| x          | int32 |
| y          | int32 |

FACING

| Data Field | Type  |
| ---------- | ----- |
| player_id  | int32 |
| facing     | int32 |

SPEED:

| Data Field | Type  |
| ---------- | ----- |
| player_id  | int32 |
| speed      | int32 |

SPRITE:

| Data Field  | Type   |
| ----------- | ------ |
| player_id   | int32  |
| sprite_name | string |
| index       | int32  |

FLASH:

| Data Field | Type  |
| ---------- | ----- |
| player_id  | int32 |
| r          | int32 |
| g          | int32 |
| b          | int32 |
| p          | int32 |
| f          | int32 |

REPEATING_FLASH:

*Same as FLASH*

REMOVE_REPEATING_FLASH:
| Data Field | Type  |
| ---------- | ----- |
| player_id  | int32 |

TONE:

| Data Field | Type  |
| ---------- | ----- |
| player_id  | int32 |
| red        | int32 |
| green      | int32 |
| blue       | int32 |
| grey       | int32 |

SYSTEM_NAME:

| Data Field | Type   |
| ---------- | ------ |
| player_id  | int32  |
| sys_name   | string |

SOUND_EVENT:

| Data Field | Type   |
| ---------- | ------ |
| player_id  | int32  |
| name       | string |
| volume     | int32  |
| tempo      | int32  |
| balance    | int32  |

SHOW_PICTURE:

| Data Field            | Type   |
| --------------------- | ------ |
| player_id             | int32  |
| picture_id            | int32  |
| position_x            | int32  |
| position_y            | int32  |
| map_x                 | int32  |
| map_y                 | int32  |
| pan_x                 | int32  |
| pan_y                 | int32  |
| magnify               | int32  |
| top_trans             | int32  |
| bottom_trans          | int32  |
| red                   | int32  |
| green                 | int32  |
| blue                  | int32  |
| saturation            | int32  |
| effect_mode           | int32  |
| effect_power          | int32  |
| name                  | string |
| use_transparent_color | bool   |
| fixed_to_map          | bool   |

MOVE_PICTURE:

| Data Field   | Type  |
| ------------ | ----- |
| player_id    | int32 |
| picture_id   | int32 |
| position_x   | int32 |
| position_y   | int32 |
| map_x        | int32 |
| map_y        | int32 |
| pan_x        | int32 |
| pan_y        | int32 |
| magnify      | int32 |
| top_trans    | int32 |
| bottom_trans | int32 |
| red          | int32 |
| green        | int32 |
| blue         | int32 |
| saturation   | int32 |
| effect_mode  | int32 |
| effect_power | int32 |
| duration     | int32 |

ERASE_PICTURE:

| Data Field            | Type   |
| --------------------- | ------ |
| player_id             | int32  |
| picture_id            | int32  |

NAME:

| Data Field | Type   |
| ---------- | ------ |
| player_id  | int32  |
| name       | string |

SYNC_SWITCH:

| Data Field | Type  |
| ---------- | ----- |
| switch_id  | int32 |
| sync_type  | int32 |

SYNC_VARIABLE:

| Data Field | Type  |
| ---------- | ----- |
| var_id     | int32 |
| sync_type  | int32 |

SYNC_EVENT:

| Data Field   | Type  |
| ------------ | ----- |
| event_id     | int32 |
| trigger_type | int32 |

BADGE_UPDATE:

| Data Field   | Type  |
| ------------ | ----- |

#### C2S (Client to Server) Packets:

IDENTIFY:

| Data Field | Type  |
| ---------- | ----- |

MAIN_PLAYER_POS:

| Data Field | Type  |
| ---------- | ----- |
| x          | int32 |
| y          | int32 |

TELEPORT:

| Data Field | Type  |
| ---------- | ----- |
| x          | int32 |
| y          | int32 |

FACING:

| Data Field | Type  |
| ---------- | ----- |
| facing     | int32 |

SPEED:

| Data Field | Type  |
| ---------- | ----- |
| speed      | int32 |

SPRITE:

| Data Field  | Type   |
| ----------- | ------ |
| sprite_name | string |
| index       | int32  |

FLASH:

| Data Field | Type  |
| ---------- | ----- |
| r          | int32 |
| g          | int32 |
| b          | int32 |
| p          | int32 |
| f          | int32 |

REPEATING_FLASH:

*Same as FLASH*

REMOVE_REPEATING_FLASH:

*This packet has no parameters*

TONE:

| Data Field | Type  |
| ---------- | ----- |
| red        | int32 |
| green      | int32 |
| blue       | int32 |
| grey       | int32 |

NAME:

| Data Field | Type   |
| ---------- | ------ |
| name       | string |

SOUND_EVENT:

| Data Field | Type   |
| ---------- | ------ |
| name       | string |
| volume     | int32  |
| tempo      | int32  |
| balance    | int32  |

SYSTEM_NAME:

| Data Field | Type   |
| ---------- | ------ |
| sys_name   | string |

SHOW_PICTURE:

| Data Field            | Type   |
| --------------------- | ------ |
| picture_id            | int32  |
| position_x            | int32  |
| position_y            | int32  |
| map_x                 | int32  |
| map_y                 | int32  |
| pan_x                 | int32  |
| pan_y                 | int32  |
| magnify               | int32  |
| top_trans             | int32  |
| bottom_trans          | int32  |
| red                   | int32  |
| green                 | int32  |
| blue                  | int32  |
| saturation            | int32  |
| effect_mode           | int32  |
| effect_power          | int32  |
| name                  | string |
| use_transparent_color | bool   |
| fixed_to_map          | bool   |

MOVE_PICTURE:

| Data Field   | Type  |
| ------------ | ----- |
| picture_id   | int32 |
| position_x   | int32 |
| position_y   | int32 |
| map_x        | int32 |
| map_y        | int32 |
| pan_x        | int32 |
| pan_y        | int32 |
| magnify      | int32 |
| top_trans    | int32 |
| bottom_trans | int32 |
| red          | int32 |
| green        | int32 |
| blue         | int32 |
| saturation   | int32 |
| effect_mode  | int32 |
| effect_power | int32 |
| duration     | int32 |

ERASE_PICTURE:

| Data Field | Type  |
| ---------- | ----- |
| picture_id | int32 |

CHAT:

| Data Field | Type   |
| ---------- | ------ |
| msg        | string |

GLOBAL_CHAT:

| Data Field     | Type   |
| -------------- | ------ |
| msg            | string |
| enable_loc_bin | bool   |

PARTY_CHAT:

| Data Field | Type   |
| ---------- | ------ |
| msg        | string |

BAN_USER:

| Data Field | Type |
| ---------- | ---- |
| uuid       | uuid |

SYNC_SWITCH:

| Data Field | Type  |
| ---------- | ----- |
| switch_id  | int32 |
| value_bin  | int32 |

SYNC_VARIABLE:

| Data Field | Type  |
| ---------- | ----- |
| var_id     | int32 |
| value      | int32 |

SYNC_EVENT:

| Data Field | Type  |
| ---------- | ----- |
| event_id   | int32 |
| action_bin | int32 |

