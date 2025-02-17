#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/uart.h"
#include "rs485_parser.h"

static const char *TAG = "RS485_PARSER";

static uint8_t packet_buffer[256];
static int packet_index = 0;
static bool receiving_packet = false;

static char *packet_to_str(const uint8_t *packet, int len)
{
    static char buffer[512];
    int pos = 0;
    for (int i = 0; i < len && pos < sizeof(buffer) - 3; i++)
    {
        pos += snprintf(&buffer[pos], sizeof(buffer) - pos, "%02X ", packet[i]);
    }
    return buffer;
}

static bool is_checksum_valid(const uint8_t *packet, int len)
{
    if (len < 19)
    {
        ESP_LOGW(TAG, "Packet too short for checksum validation (len: %d)", len);
        return false;
    }

    uint8_t checksum = 0;
    for (int i = 2; i <= 17; i++)
    {
        checksum += packet[i];
    }
    checksum %= 256;

    if (checksum == packet[18])
    {
        return true;
    }

    ESP_LOGW(TAG, "Invalid checksum: Calculated 0x%02X, Received 0x%02X", checksum, packet[18]);
    return false;
}

typedef struct LightNode
{
    int light_id;
    struct LightNode *next;
} LightNode;

typedef struct OutletNode
{
    int outlet_id;
    struct OutletNode *next;
} OutletNode;

typedef struct RoomLights
{
    int room_id;
    LightNode *lights;
    struct RoomLights *next;
} RoomLights;

typedef struct RoomOutlets
{
    int room_id;
    OutletNode *outlets;
    struct RoomOutlets *next;
} RoomOutlets;

static RoomLights *discovered_lights = NULL;
static RoomOutlets *discovered_outlets = NULL;

static void add_discovered_device(RoomLights **room_list, int room_id, int device_id)
{
    RoomLights *room = *room_list;
    while (room)
    {
        if (room->room_id == room_id)
        {
            LightNode *device = room->lights;
            while (device)
            {
                if (device->light_id == device_id)
                {
                    return; // Already discovered
                }
                device = device->next;
            }
            LightNode *new_device = (LightNode *)malloc(sizeof(LightNode));
            new_device->light_id = device_id;
            new_device->next = room->lights;
            room->lights = new_device;
            return;
        }
        room = room->next;
    }
    RoomLights *new_room = (RoomLights *)malloc(sizeof(RoomLights));
    new_room->room_id = room_id;
    new_room->lights = (LightNode *)malloc(sizeof(LightNode));
    new_room->lights->light_id = device_id;
    new_room->lights->next = NULL;
    new_room->next = *room_list;
    *room_list = new_room;
}

static void parse_light_or_outlet_packet(RoomLights **room_list, const char *device_type, int room_id, uint8_t cmd, const uint8_t *payload)
{
    ESP_LOGI(TAG, "%s Packet - Room %d | Cmd: 0x%02X", device_type, room_id, cmd);
    bool all_on = true;

    for (int i = 0; i < 8; i++)
    {
        if (payload[i] == 0xFF)
        {
            add_discovered_device(room_list, room_id, i);
        }
        else
        {
            all_on = false;
        }
    }

    if (all_on)
    {
        ESP_LOGI(TAG, "Room %d - All %ss are ON, Skipping discovery...", room_id, device_type);
        return;
    }

    RoomLights *room = *room_list;
    while (room)
    {
        if (room->room_id == room_id)
        {
            LightNode *device = room->lights;
            while (device)
            {
                ESP_LOGI(TAG, "Room %d - %s %d is %s", room_id, device_type, device->light_id, payload[device->light_id] == 0xFF ? "ON" : "OFF");
                device = device->next;
            }
            break;
        }
        room = room->next;
    }
}

static void parse_heating_packet(int room_id, const uint8_t *payload)
{
    ESP_LOGI(TAG, "Heating Packet - Room %d", room_id);

    bool heating_on = (payload[0] >> 4) == 1;
    bool hot_water_on = (payload[0] & 0x0F) == 2;
    bool away_mode = (payload[1] & 0x0F) == 1;
    uint8_t target_temp = payload[2];
    uint8_t current_temp = payload[4];
    uint8_t hot_water_temp = payload[3];
    uint8_t heating_water_temp = payload[5];
    uint8_t boiler_error_code = payload[6];

    ESP_LOGI(TAG, "Heating: %s | Hot Water: %s | Away Mode: %s | Target Temp: %d°C | Current Temp: %d°C | Hot Water Temp: %d°C | Heating Water Temp: %d°C | Boiler Error Code: %02d",
             heating_on ? "ON" : "OFF",
             hot_water_on ? "ON" : "OFF",
             away_mode ? "ON" : "OFF",
             target_temp,
             current_temp,
             hot_water_temp,
             heating_water_temp,
             boiler_error_code);
}

static void parse_ac_packet(int room_id, const uint8_t *payload)
{
    ESP_LOGI(TAG, "AC Packet - Room %d", room_id);

    bool ac_on = (payload[0] >> 4) == 1;
    uint8_t mode = payload[1];
    uint8_t fan_speed = payload[2];
    uint8_t current_temp = payload[4];
    uint8_t target_temp = payload[5];

    const char *mode_str;
    switch (mode)
    {
    case 0x00:
        mode_str = "Cooling";
        break;
    case 0x01:
        mode_str = "Fan Only";
        break;
    case 0x02:
        mode_str = "Dry";
        break;
    case 0x03:
        mode_str = "Auto";
        break;
    default:
        mode_str = "Unknown";
        break;
    }

    const char *fan_str;
    switch (fan_speed)
    {
    case 0x00:
        fan_str = "Off";
        break;
    case 0x01:
        fan_str = "Low";
        break;
    case 0x02:
        fan_str = "Medium";
        break;
    case 0x03:
        fan_str = "High";
        break;
    default:
        fan_str = "Unknown";
        break;
    }

    ESP_LOGI(TAG, "AC: %s | Mode: %s | Fan Speed: %s | Current Temp: %d°C | Target Temp: %d°C",
             ac_on ? "ON" : "OFF",
             mode_str,
             fan_str,
             current_temp,
             target_temp);
}

static void parse_device_packet(uint16_t device, uint8_t cmd, const uint8_t *payload)
{
    uint8_t device_type = device >> 8;
    int room_id = device & 0xFF;

    switch (device_type)
    {
    case 0x0E:
        parse_light_or_outlet_packet(&discovered_lights, "Light", room_id, cmd, payload);
        break;
    case 0x3B:
        parse_light_or_outlet_packet((RoomLights **)&discovered_outlets, "Outlet", room_id, cmd, payload);
        break;
    case 0x36:
        parse_heating_packet(room_id, payload);
        break;
    case 0x39:
        parse_ac_packet(room_id, payload);
        break;
    default:
        ESP_LOGW(TAG, "Unknown Device 0x%02X - Room %d", device_type, room_id);
    }
}

static void parse_base_packet(int uart_num, const uint8_t *packet, int len)
{
    if (len < MIN_PACKET_SIZE)
    {
        ESP_LOGW(TAG, "Invalid Packet Length: %d", len);
        return;
    }

    if (!is_checksum_valid(packet, len))
    {
        ESP_LOGW(TAG, "Checksum validation failed. Dropping packet.");
        return;
    }

    uint8_t protocol = packet[3] >> 4;
    uint8_t seq_num = packet[3] & 0x0F;
    uint16_t dest = (packet[5] << 8) | packet[6];
    uint16_t src = (packet[7] << 8) | packet[8];
    uint8_t cmd = packet[9];
    const uint8_t *payload = &packet[10];

    ESP_LOGI(TAG, "Base Packet - UART%d | Protocol: %d | Seq: %d | Dest: 0x%04X | Src: 0x%04X | Cmd: 0x%02X",
             uart_num, protocol, seq_num, dest, src, cmd);

    if (dest == 0x0100)
    {
        parse_device_packet(src, cmd, payload);
    }
    else if (src == 0x0100)
    {
        parse_device_packet(dest, cmd, payload);
    }
    else
    {
        ESP_LOGW(TAG, "Unknown Packet: %s", packet_to_str(packet, len));
    }
}

void rs485_parser_process_byte(uint8_t byte)
{
    if (!receiving_packet)
    {
        if (packet_index == 0 && byte == PACKET_START_BYTE_1)
        {
            packet_buffer[packet_index++] = byte;
        }
        else if (packet_index == 1 && byte == PACKET_START_BYTE_2)
        {
            packet_buffer[packet_index++] = byte;
            receiving_packet = true;
        }
        else
        {
            packet_index = 0;
        }
    }
    else
    {
        packet_buffer[packet_index++] = byte;

        if (packet_index >= 2 && packet_buffer[packet_index - 2] == PACKET_END_BYTE_1 &&
            packet_buffer[packet_index - 1] == PACKET_END_BYTE_2)
        {

            if (packet_index >= MIN_PACKET_SIZE)
            {
                parse_rs485_packet(UART_NUM_1, packet_buffer, packet_index);
            }
            else
            {
                ESP_LOGW(TAG, "Packet too short: %d bytes", packet_index);
            }

            packet_index = 0;
            receiving_packet = false;
        }

        if (packet_index >= sizeof(packet_buffer))
        {
            ESP_LOGW(TAG, "Packet buffer overflow, resetting");
            packet_index = 0;
            receiving_packet = false;
        }
    }
}

void parse_rs485_packet(int uart_num, uint8_t *packet, int len)
{
    ESP_LOGI(TAG, "Parsed RS485 Packet - UART%d (Length: %d)", uart_num, len);
    parse_base_packet(uart_num, packet, len);
}