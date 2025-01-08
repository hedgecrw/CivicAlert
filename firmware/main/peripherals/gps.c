#include <freertos/FreeRTOS.h>
#include <driver/uart.h>
#include "gps.h"

#define UBX_MSG_SYNC1_OFFSET           0
#define UBX_MSG_SYNC2_OFFSET           1
#define UBX_MSG_CLASS_OFFSET           2
#define UBX_MSG_ID_OFFSET              3
#define UBX_MSG_LEN_OFFSET             4
#define UBX_MSG_PAYLOAD_OFFSET         6
#define UBX_MAX_PAYLOAD_SIZE           255
#define UBX_MSG_CHKSUM_LEN             2
#define UBX_PACKET_OVERHEAD            (UBX_MSG_PAYLOAD_OFFSET + UBX_MSG_CHKSUM_LEN)
#define UBX_MAX_PACKET_SIZE            (UBX_MAX_PAYLOAD_SIZE + UBX_PACKET_OVERHEAD)

#define UBX_SYNC1_CHAR                 0xB5
#define UBX_SYNC2_CHAR                 0x62
#define UBX_SYNC                       UBX_SYNC1_CHAR, UBX_SYNC2_CHAR

#define UBX_CFG_VALGET_MSG             0x06, 0x8B
#define UBX_CFG_VALSET_MSG             0x06, 0x8A
#define UBX_CFG_ACK_MSG                0x05, 0x01
#define UBX_MON_VER_MSG                0x0A, 0x04
#define UBX_NAV_PVT_MSG                0x01, 0x07
#define UBX_TIM_TM2_MSG                0x0D, 0x03

#define UBX_MON_VER_CHKSUM             0x0E, 0x34
#define UBX_CFG_VALGET_BEGIN           0x00, 0x01, 0x00, 0x00
#define UBX_CFG_VALSET_BEGIN           0x00, 0x05, 0x00, 0x00
#define UBX_GET_LNA_DATA               0x57, 0x00, 0xA3, 0x20
#define UBX_GET_LNA_CHKSUM             0xB4, 0x5A
#define UBX_SET_LNA_DATA               UBX_GET_LNA_DATA, 0x00
#define UBX_SET_LNA_CHKSUM             0xB8, 0x2D
#define UBX_GET_INTERFACE_CFG_DATA     0x03, 0x00, 0x51, 0x10, 0x06, 0x00, 0x64, 0x10, 0x01, 0x00, 0x73, 0x10, \
                                       0x02, 0x00, 0x73, 0x10, 0x01, 0x00, 0x74, 0x10, 0x02, 0x00, 0x74, 0x10
#define UBX_GET_INTERFACE_CFG_CHKSUM   0xA0, 0x76
#define UBX_SET_INTERFACE_CFG_DATA     0x03, 0x00, 0x51, 0x10, 0x00, 0x06, 0x00, 0x64, 0x10, 0x00, 0x01, 0x00, \
                                       0x73, 0x10, 0x01, 0x02, 0x00, 0x73, 0x10, 0x00, 0x01, 0x00, 0x74, 0x10, \
                                       0x01, 0x02, 0x00, 0x74, 0x10, 0x00
#define UBX_SET_INTERFACE_CFG_CHKSUM   0xAB, 0xC0
#define UBX_GET_GNSS_CFG_DATA          0x1F, 0x00, 0x31, 0x10, 0x01, 0x00, 0x31, 0x10, 0x04, 0x00, 0x31, 0x10, \
                                       0x20, 0x00, 0x31, 0x10, 0x05, 0x00, 0x31, 0x10, 0x21, 0x00, 0x31, 0x10, \
                                       0x07, 0x00, 0x31, 0x10, 0x09, 0x00, 0x31, 0x10, 0x22, 0x00, 0x31, 0x10, \
                                       0x0F, 0x00, 0x31, 0x10, 0x28, 0x00, 0x31, 0x10
#define UBX_GET_GNSS_CFG_CHKSUM        0x60, 0xB6
#define UBX_SET_GNSS_CFG_DATA          0x1F, 0x00, 0x31, 0x10, 0x01, 0x01, 0x00, 0x31, 0x10, 0x01, 0x04, 0x00, \
                                       0x31, 0x10, 0x01, 0x20, 0x00, 0x31, 0x10, 0x01, 0x05, 0x00, 0x31, 0x10, \
                                       0x01, 0x21, 0x00, 0x31, 0x10, 0x01, 0x07, 0x00, 0x31, 0x10, 0x01, 0x09, \
                                       0x00, 0x31, 0x10, 0x01, 0x22, 0x00, 0x31, 0x10, 0x01, 0x0F, 0x00, 0x31, \
                                       0x10, 0x01, 0x28, 0x00, 0x31, 0x10, 0x01
#define UBX_SET_GNSS_CFG_CHKSUM        0x79, 0x94
#define UBX_GET_L5_CFG_DATA            0x01, 0x00, 0x32, 0x10
#define UBX_GET_L5_CFG_CHKSUM          0xDD, 0x10
#define UBX_SET_L5_CFG_DATA            UBX_GET_L5_CFG_DATA, 0x01
#define UBX_SET_L5_CFG_CHKSUM          0xE2, 0x0D
#define UBX_GET_MSG_CFG_DATA           0x07, 0x00, 0x91, 0x20, 0x79, 0x01, 0x91, 0x20
#define UBX_GET_MSG_CFG_CHKSUM         0x81, 0x9F
#define UBX_SET_MSG_CFG_DATA           0x07, 0x00, 0x91, 0x20, 0x01, 0x79, 0x01, 0x91, 0x20, 0x01
#define UBX_SET_MSG_CFG_CHKSUM         0x88, 0xC0
#define UBX_GET_GEN_CFG_DATA           0x21, 0x00, 0x11, 0x20, 0x11, 0x00, 0x11, 0x20, 0x13, 0x00, 0x11, 0x10, \
                                       0x1C, 0x00, 0x11, 0x20, 0x03, 0x00, 0x36, 0x10, 0x05, 0x00, 0x36, 0x10, \
                                       0x04, 0x00, 0x36, 0x10, 0x07, 0x00, 0x05, 0x10, 0x01, 0x00, 0x21, 0x30, \
                                       0x02, 0x00, 0x21, 0x30, 0x03, 0x00, 0x21, 0x20, 0x0C, 0x00, 0x05, 0x20
#define UBX_GET_GEN_CFG_CHKSUM         0xEF, 0xCA
#define UBX_SET_GEN_CFG_DATA           0x21, 0x00, 0x11, 0x20, 0x02, 0x11, 0x00, 0x11, 0x20, 0x03, 0x13, 0x00, \
                                       0x11, 0x10, 0x01, 0x1C, 0x00, 0x11, 0x20, 0x03, 0x03, 0x00, 0x36, 0x10, \
                                       0x00, 0x05, 0x00, 0x36, 0x10, 0x00, 0x04, 0x00, 0x36, 0x10, 0x01, 0x07, \
                                       0x00, 0x05, 0x10, 0x00, 0x01, 0x00, 0x21, 0x30, 0xF4, 0x01, 0x02, 0x00, \
                                       0x21, 0x30, 0x02, 0x00, 0x03, 0x00, 0x21, 0x20, 0x01, 0x0C, 0x00, 0x05, \
                                       0x20, 0x01
#define UBX_SET_GEN_CFG_CHKSUM         0x03, 0x64

#define FIX_TYPE_2D                    0x02
#define FIX_TYPE_3D                    0x03

#define LNA_MSG_GAIN_OFFSET     4

// UBX received packet state
typedef enum
{
   UBX_PACKET_INIT_STATE,
   UBX_PACKET_SYNC_STATE,
   UBX_PACKET_CLASS_STATE,
   UBX_PACKET_ID_STATE,
   UBX_PACKET_LEN1_STATE,
   UBX_PACKET_LEN2_STATE,
   UBX_PACKET_PAYLOAD_STATE,
   UBX_PACKET_CK_A_STATE,
   UBX_PACKET_CK_B_STATE
} ubx_packet_state_t;

// UBX packet type
typedef enum
{
   UBX_MSG_UNKNOWN,
   UBX_MON_VER,
   UBX_NAV_PVT,
   UBX_TIM_TM2,
   UBX_CFG_VALGET,
   UBX_ACK_ACK
} ubx_message_type_t;

// LNA gain settings
typedef enum
{
   LNA_GAIN_NORMAL = 0,
   LNA_GAIN_LOW = 1,
   LNA_GAIN_BYPASS = 2
} lna_gain_t;

// Relevant UBX message structures
#pragma pack(push, 1)
typedef struct {
   uint32_t iTOW;
   uint16_t year;
   uint8_t month, day, hour, min, sec;
   union {
      uint8_t valid;
      struct {
         uint8_t validDate: 1;
         uint8_t validTime: 1;
         uint8_t fullyResolved: 1;
         uint8_t validMag: 1;
      };
   };
   uint32_t tAcc;
   int32_t nano;
   uint8_t fixType;
   union {
      uint8_t flags;
      struct {
         uint8_t gnssFixOK: 1;
         uint8_t diffSoln: 1;
         uint8_t psmState: 3;
         uint8_t headVehValid: 1;
         uint8_t carrSoln: 2;
      };
   };
   union {
      uint8_t flags2;
      struct {
         uint8_t confirmedAvai: 1;
         uint8_t confirmedDate: 1;
         uint8_t confirmedTime: 1;
      };
   };
   uint8_t numSV;
   int32_t lon, lat, height, hMSL;
   uint32_t hAcc, vAcc;
   int32_t velN, velE, velD, gSpeed, headMot;
   uint32_t sAcc, headAcc;
   uint16_t pDOP;
   uint8_t reserved1[6];
   int32_t headVeh;
   union {
      uint16_t flags3;
      struct {
         uint8_t invalidLlh: 1;
         uint8_t lastCorrectionAge: 4;
         uint8_t unused: 8;
         uint8_t authTime: 1;
         uint8_t nmaFixStatus: 1;
      };
   };
} __attribute__((packed)) ubx_nav_pvt_t;

typedef struct {
   uint8_t ch;
   union {
      uint8_t flags;
      struct {
         uint8_t mode: 1;
         uint8_t run: 1;
         uint8_t newFallingEdge: 1;
         uint8_t timeBase: 2;
         uint8_t utc: 1;
         uint8_t time: 1;
         uint8_t newRisingEdge: 1;
      };
   };
   uint16_t count, wnR, wnF;
   uint32_t towMsR, towSubMsR, towMsF, towSubMsF;
   int32_t accEst;
} __attribute__((packed)) ubx_tim_tm2_t;
#pragma pack(pop)

// Global state variables
static bool initial_fix_found;
static ubx_packet_state_t ubx_packet_state;
static uint8_t gps_rx_buffer[UBX_MAX_PACKET_SIZE];
static uint16_t gps_rx_idx, gps_rx_len;
static uint8_t gps_rx_ck_a, gps_rx_ck_b;
static ubx_nav_pvt_t ubx_nav_pvt_message;
static ubx_tim_tm2_t ubx_tim_tm2_message;
static float lat_degrees, lon_degrees, height_meters;
static volatile double requested_timestamp;

// Time conversion helper function
inline static double tm2_to_gps_timestamp(uint16_t week_number, uint32_t tow_ms, uint32_t tow_sub_ms)
{
   return ((double)week_number * 604800.0) + ((double)tow_ms * 0.001) + ((double)tow_sub_ms * 0.000000001);
}


// Full UBX message processing function
static ubx_message_type_t gps_process_message(uint8_t* msg, uint16_t len)
{
   // Identify the type of message received and copy it to the appropriate global location
   if ((msg[UBX_MSG_CLASS_OFFSET] == 0x01) && (msg[UBX_MSG_ID_OFFSET] == 0x07) && (len == sizeof(ubx_nav_pvt_t)))
   {
      memcpy(&ubx_nav_pvt_message, msg, len);
      if (ubx_nav_pvt_message.gnssFixOK && ((ubx_nav_pvt_message.fixType == FIX_TYPE_3D) || ((ubx_nav_pvt_message.fixType == FIX_TYPE_2D) && !initial_fix_found)))
      {
         initial_fix_found |= (ubx_nav_pvt_message.fixType == FIX_TYPE_3D);
         lat_degrees = (float)ubx_nav_pvt_message.lat * 1.0e-7f;
         lon_degrees = (float)ubx_nav_pvt_message.lon * 1.0e-7f;
         height_meters = (float)ubx_nav_pvt_message.height * 1.0e-3f;
      }
      return UBX_NAV_PVT;
   }
   else if ((msg[UBX_MSG_CLASS_OFFSET] == 0x0D) && (msg[UBX_MSG_ID_OFFSET] == 0x03) && (len == sizeof(ubx_tim_tm2_t)))
   {
      memcpy(&ubx_tim_tm2_message, msg, len);
      if (ubx_tim_tm2_message.time)
      {
         if (ubx_tim_tm2_message.newRisingEdge)
            requested_timestamp = tm2_to_gps_timestamp(ubx_tim_tm2_message.wnR, ubx_tim_tm2_message.towMsR, ubx_tim_tm2_message.towSubMsR);
         else if (ubx_tim_tm2_message.newFallingEdge)
            requested_timestamp = tm2_to_gps_timestamp(ubx_tim_tm2_message.wnF, ubx_tim_tm2_message.towMsF, ubx_tim_tm2_message.towSubMsF);
      }
      return UBX_TIM_TM2;
   }
   else if ((msg[UBX_MSG_CLASS_OFFSET] == 0x06) && (msg[UBX_MSG_ID_OFFSET] == 0x8B))
      return UBX_CFG_VALGET;
   else if ((msg[UBX_MSG_CLASS_OFFSET] == 0x0A) && (msg[UBX_MSG_ID_OFFSET] == 0x04))
      return UBX_MON_VER;
   else if ((msg[UBX_MSG_CLASS_OFFSET] == 0x05) && ((msg[UBX_MSG_ID_OFFSET] == 0x00) || (msg[UBX_MSG_ID_OFFSET] == 0x01)))
      return UBX_ACK_ACK;
   return UBX_MSG_UNKNOWN;
}

// UBX message processing state machine
static ubx_message_type_t gps_process_next_char(void)
{
   // Process incoming UBX bytes character-by-character
   static uint8_t rx_byte;
   if (uart_read_bytes(UART_NUM_1, &rx_byte, 1, pdMS_TO_TICKS(1000)) > 0)
   {
      switch(ubx_packet_state)
      {
         case UBX_PACKET_INIT_STATE:
            if (rx_byte == UBX_SYNC1_CHAR)
            {
               ubx_packet_state = UBX_PACKET_SYNC_STATE;
               gps_rx_idx = gps_rx_ck_a = gps_rx_ck_b = 0;
                  gps_rx_buffer[gps_rx_idx++] = rx_byte;
            }
            break;
         case UBX_PACKET_SYNC_STATE:
            if (rx_byte == UBX_SYNC2_CHAR)
            {
               gps_rx_buffer[gps_rx_idx++] = rx_byte;
               ubx_packet_state = UBX_PACKET_CLASS_STATE;
            }
            else
               ubx_packet_state = UBX_PACKET_INIT_STATE;
            break;
         case UBX_PACKET_CLASS_STATE:
            gps_rx_buffer[gps_rx_idx++] = rx_byte;
            gps_rx_ck_a += rx_byte;
            gps_rx_ck_b += gps_rx_ck_a;
            ubx_packet_state = UBX_PACKET_ID_STATE;
            break;
         case UBX_PACKET_ID_STATE:
            gps_rx_buffer[gps_rx_idx++] = rx_byte;
            gps_rx_ck_a += rx_byte;
            gps_rx_ck_b += gps_rx_ck_a;
            ubx_packet_state = UBX_PACKET_LEN1_STATE;
            break;
         case UBX_PACKET_LEN1_STATE:
            gps_rx_buffer[gps_rx_idx++] = rx_byte;
            gps_rx_ck_a += rx_byte;
            gps_rx_ck_b += gps_rx_ck_a;
            gps_rx_len = rx_byte;
            ubx_packet_state = UBX_PACKET_LEN2_STATE;
            break;
         case UBX_PACKET_LEN2_STATE:
            gps_rx_buffer[gps_rx_idx++] = rx_byte;
            gps_rx_ck_a += rx_byte;
            gps_rx_ck_b += gps_rx_ck_a;
            gps_rx_len |= (rx_byte << 8);
            if (gps_rx_len > 0)
               ubx_packet_state = UBX_PACKET_PAYLOAD_STATE;
            else
               ubx_packet_state = UBX_PACKET_CK_A_STATE;
            break;
         case UBX_PACKET_PAYLOAD_STATE:
            if ((gps_rx_idx - UBX_MSG_PAYLOAD_OFFSET) < UBX_MAX_PAYLOAD_SIZE)
            {
               gps_rx_buffer[gps_rx_idx++] = rx_byte;
               gps_rx_ck_a += rx_byte;
               gps_rx_ck_b += gps_rx_ck_a;
            }
            if ((gps_rx_idx - UBX_MSG_PAYLOAD_OFFSET) == gps_rx_len)
               ubx_packet_state = UBX_PACKET_CK_A_STATE;
            break;
         case UBX_PACKET_CK_A_STATE:
            if (gps_rx_ck_a != rx_byte)
               ubx_packet_state = UBX_PACKET_INIT_STATE;
            else
            {
               gps_rx_buffer[gps_rx_idx++] = rx_byte;
               ubx_packet_state = UBX_PACKET_CK_B_STATE;
            }
            break;
         case UBX_PACKET_CK_B_STATE:
            if (gps_rx_ck_b == rx_byte)
            {
               gps_rx_buffer[gps_rx_idx++] = rx_byte;
               return gps_process_message(gps_rx_buffer, gps_rx_idx);
            }
            ubx_packet_state = UBX_PACKET_INIT_STATE;
            break;
         default:
            ubx_packet_state = UBX_PACKET_INIT_STATE;
      }
   }
   return UBX_MSG_UNKNOWN;
}

static void gps_reset(void)
{
   // Hold the reset pin low for ~1ms to reset the GPS module
   gpio_set_level(GPS_RESET_PIN, 0);
   vTaskDelay(pdMS_TO_TICKS(1100));
   gpio_set_level(GPS_RESET_PIN, 1);
   initial_fix_found = false;
}

static void gps_wait_until_ready(void)
{
   // Test the communication interface by polling the UBX-MON-VER message
   uart_flush(UART_NUM_1);
   ubx_packet_state = UBX_PACKET_INIT_STATE;
   const uint8_t ubx_mon_ver[] = {UBX_SYNC, UBX_MON_VER_MSG, 0, 0, UBX_MON_VER_CHKSUM};
   uart_write_bytes(UART_NUM_1, ubx_mon_ver, sizeof(ubx_mon_ver));
   while (gps_process_next_char() != UBX_MON_VER);
}

static lna_gain_t gps_get_lna_gain(void)
{
   // Poll to check that the LNA gain is set correctly
   const uint8_t ubx_valget_lna[] = {UBX_SYNC, UBX_CFG_VALGET_MSG, 0x08, 0x00, UBX_CFG_VALGET_BEGIN, UBX_GET_LNA_DATA, UBX_GET_LNA_CHKSUM};
   uart_write_bytes(UART_NUM_1, ubx_valget_lna, sizeof(ubx_valget_lna));
   while (gps_process_next_char() != UBX_CFG_VALGET);
   return (lna_gain_t)gps_rx_buffer[UBX_MSG_PAYLOAD_OFFSET + LNA_MSG_GAIN_OFFSET];
}

static void gps_set_lna_gain(void)
{
   // Set the LNA gain to NORMAL mode
   const uint8_t ubx_valset_lna[] = {UBX_SYNC, UBX_CFG_VALSET_MSG, 0x09, 0x00, UBX_CFG_VALSET_BEGIN, UBX_SET_LNA_DATA, UBX_SET_LNA_CHKSUM};
   uart_write_bytes(UART_NUM_1, ubx_valset_lna, sizeof(ubx_valset_lna));
   while (gps_process_next_char() != UBX_ACK_ACK);
}

static void gps_verify_or_set_interface_config(void)
{
   // Ensure that only UBX communications take place only over the UART interface
   // CFG-I2C-ENABLED=0, CFG-SPI-ENABLED=0, CFG-UART1INPROT-UBX=1, CFG-UART1OUTPROT-UBX=1, CFG-UART1INPROT-NMEA=0, CFG-UART1OUTPROT-NMEA=0
   const uint8_t ubx_interface_cfg_get[] = {UBX_SYNC, UBX_CFG_VALGET_MSG, 0x1C, 0x00, UBX_CFG_VALGET_BEGIN, UBX_GET_INTERFACE_CFG_DATA, UBX_GET_INTERFACE_CFG_CHKSUM};
   uart_write_bytes(UART_NUM_1, ubx_interface_cfg_get, sizeof(ubx_interface_cfg_get));
   while (gps_process_next_char() != UBX_CFG_VALGET);
   // TODO: Verify that config value matches the expected value
   //    If not, send:
   if (false)
   {
      const uint8_t ubx_interface_cfg_set[] = {UBX_SYNC, UBX_CFG_VALSET_MSG, 0x22, 0x00, UBX_CFG_VALSET_BEGIN, UBX_SET_INTERFACE_CFG_DATA, UBX_SET_INTERFACE_CFG_CHKSUM};
      uart_write_bytes(UART_NUM_1, ubx_interface_cfg_set, sizeof(ubx_interface_cfg_set));
      while (gps_process_next_char() != UBX_ACK_ACK);
   }
}

static void gps_verify_or_set_gnss_config(void)
{
   // Ensure that the expected set of GNSS signals are enabled
   // CFG-SIGNAL-GPS_ENA=1, CFG-SIGNAL-GPS_L1CA_ENA=1, CFG-SIGNAL-GPS_L5_ENA=1, CFG-SIGNAL-SBAS_ENA=1, CFG-SIGNAL-SBAS_L1CA_ENA=1
   // CFG-SIGNAL-GAL_ENA=1, CFG-SIGNAL-GAL_E1_ENA=1, CFG-SIGNAL-GAL_E5A_ENA=1, CFG-SIGNAL-BDS_ENA=1, CFG-SIGNAL-BDS_B1C_ENA=1, CFG-SIGNAL-BDS_B2A_ENA=1
   const uint8_t ubx_gnss_cfg_get[] = {UBX_SYNC, UBX_CFG_VALGET_MSG, 0x30, 0x00, UBX_CFG_VALGET_BEGIN, UBX_GET_GNSS_CFG_DATA, UBX_GET_GNSS_CFG_CHKSUM};
   uart_write_bytes(UART_NUM_1, ubx_gnss_cfg_get, sizeof(ubx_gnss_cfg_get));
   while (gps_process_next_char() != UBX_CFG_VALGET);
   // TODO: Verify that config value matches the expected value
   //    If not, send:
   if (false)
   {
      const uint8_t ubx_gnss_cfg_set[] = {UBX_SYNC, UBX_CFG_VALSET_MSG, 0x3B, 0x00, UBX_CFG_VALSET_BEGIN, UBX_SET_GNSS_CFG_DATA, UBX_SET_GNSS_CFG_CHKSUM};
      uart_write_bytes(UART_NUM_1, ubx_gnss_cfg_set, sizeof(ubx_gnss_cfg_set));
      while (gps_process_next_char() != UBX_ACK_ACK);
      vTaskDelay(pdMS_TO_TICKS(500));
   }
}

static bool gps_verify_l5_signals_available(void)
{
   // Ensure that the currently marked as "non-operational" L5 signals are available
   const uint8_t ubx_l5_cfg_get[] = {UBX_SYNC, UBX_CFG_VALGET_MSG, 0x08, 0x00, UBX_CFG_VALGET_BEGIN, UBX_GET_L5_CFG_DATA, UBX_GET_L5_CFG_CHKSUM};
   uart_write_bytes(UART_NUM_1, ubx_l5_cfg_get, sizeof(ubx_l5_cfg_get));
   while (gps_process_next_char() != UBX_CFG_VALGET);
   bool l5_signals_available = false; // TODO: Test whether available
   if (!l5_signals_available)
   {
      const uint8_t ubx_l5_cfg_set[] = {UBX_SYNC, UBX_CFG_VALSET_MSG, 0x09, 0x00, UBX_CFG_VALSET_BEGIN, UBX_SET_L5_CFG_DATA, UBX_SET_L5_CFG_CHKSUM};
      uart_write_bytes(UART_NUM_1, ubx_l5_cfg_set, sizeof(ubx_l5_cfg_set));
      while (gps_process_next_char() != UBX_ACK_ACK);
      // TODO: If the above does not work, datasheet says to send: B5 62 06 8A 09 00 01 01 00 00 01 00 32 10 01 DF F6
      gps_reset();
   }
   return !l5_signals_available;
}

static void gps_verify_or_set_configuration(void)
{
   // Ensure that all messages are disabled except for UBX-NAV-PVT and UBX-TIM-TM2
   const uint8_t ubx_msg_cfg_get[] = {UBX_SYNC, UBX_CFG_VALGET_MSG, 0x0C, 0x00, UBX_CFG_VALGET_BEGIN, UBX_GET_MSG_CFG_DATA, UBX_GET_MSG_CFG_CHKSUM};
   uart_write_bytes(UART_NUM_1, ubx_msg_cfg_get, sizeof(ubx_msg_cfg_get));
   while (gps_process_next_char() != UBX_CFG_VALGET);
   // TODO: Verify
   if (false)
   {
      const uint8_t ubx_msg_cfg_set[] = {UBX_SYNC, UBX_CFG_VALSET_MSG, 0x0E, 0x00, UBX_CFG_VALSET_BEGIN, UBX_SET_MSG_CFG_DATA, UBX_SET_MSG_CFG_CHKSUM};
      uart_write_bytes(UART_NUM_1, ubx_msg_cfg_set, sizeof(ubx_msg_cfg_set));
      while (gps_process_next_char() != UBX_ACK_ACK);
   }

   // Ensure that the GPS configuration parameters are set as expected
   // CFG-NAVSPG-DYNMODEL=2 (Stationary), CFG-NAVSPG-FIXMODE=3 (Auto 2D/3D), CFG-NAVSPG-INIFIX3D=1 (True)
   // CFG-NAVSPG-UTCSTANDARD=3 (USNO, derived from GPS time), CFG-SBAS-USE_RANGING=0, CFG-SBAS-USE_DIFFCORR=1
   // CFG-SBAS-USE_INTEGRITY=0, CFG-TP-TP1_ENA=0 (Timepulse disabled), CFG-TP-TIMEGRID_TP1=1 (GPS time reference for timepulse)
   // CFG-RATE-MEAS=500 (2Hz), CFG-RATE-NAV=2 (1Hz), CFG-RATE-TIMEREF=1 (align measurements to GPS time)
   const uint8_t ubx_general_cfg_get[] = {UBX_SYNC, UBX_CFG_VALGET_MSG, 0x34, 0x00, UBX_CFG_VALGET_BEGIN, UBX_GET_GEN_CFG_DATA, UBX_GET_GEN_CFG_CHKSUM};
   uart_write_bytes(UART_NUM_1, ubx_general_cfg_get, sizeof(ubx_general_cfg_get));
   while (gps_process_next_char() != UBX_CFG_VALGET);
   // TODO: Verify
   if (false)
   {
      const uint8_t ubx_general_cfg_set[] = {UBX_SYNC, UBX_CFG_VALSET_MSG, 0x42, 0x00, UBX_CFG_VALSET_BEGIN, UBX_SET_GEN_CFG_DATA, UBX_SET_GEN_CFG_CHKSUM};
      uart_write_bytes(UART_NUM_1, ubx_general_cfg_set, sizeof(ubx_general_cfg_set));
      while (gps_process_next_char() != UBX_ACK_ACK);
   }
}

static void gps_init(void)
{
   // Set up the GPIO pins for the GPS module
   gpio_config_t reset_pin_config = {
      .pin_bit_mask = 1ULL << GPS_RESET_PIN,
      .mode = GPIO_MODE_OUTPUT_OD,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
   };
   gpio_config_t extint_pin_config = {
      .pin_bit_mask = 1ULL << GPS_EXTINT_PIN,
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
   };
   gpio_config(&reset_pin_config);
   gpio_config(&extint_pin_config);
   gpio_set_level(GPS_RESET_PIN, 1);
   gpio_set_level(GPS_EXTINT_PIN, 0);

   // Configure the UART peripheral with a baud rate of 38400
   uart_config_t uart_config = {
      .baud_rate = 38400,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 122,
      .source_clk = UART_SCLK_DEFAULT,
      .flags = {
         .backup_before_sleep = 0,
      }
   };
   uart_driver_install(UART_NUM_1, 2 * UART_HW_FIFO_LEN(UART_NUM_1), 0, 0, NULL, 0);
   uart_param_config(UART_NUM_1, &uart_config);
   uart_set_pin(UART_NUM_1, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
   uart_flush(UART_NUM_1);

   // Ensure that the GPS is in its default startup configuration
   requested_timestamp = 0.0;
   gps_reset();

   // Loop to ensure that all configuration parameters requiring a reset are correctly set
   bool was_reset = true;
   while (was_reset)
   {
      // Ensure that the internal GPS LNA gain is in the correct state
      gps_wait_until_ready();
      if (gps_get_lna_gain() != LNA_GAIN_NORMAL)
      {
         gps_set_lna_gain();
         gps_reset();
         continue;
      }

      // Ensure that GPS communications only take place over the UART interface
      gps_verify_or_set_interface_config();

      // Check that the GNSS signal configuration is correct
      gps_verify_or_set_gnss_config();
      was_reset = gps_verify_l5_signals_available();
   }

   // Validate all non-reset-requiring configuration parameters
   gps_verify_or_set_configuration();
}

gps_timestamp_t gps_request_timestamp(void)
{
   // Toggle EXTINT line to request a timestamp from the GPS module
   static uint32_t next_extint_level = 1;
   gps_timestamp_t gps_timestamp = { .gps_timestamp = requested_timestamp };
   requested_timestamp = 0.0;
   gpio_set_level(GPS_EXTINT_PIN, next_extint_level);
   next_extint_level = !next_extint_level;

   // Return the previously requested timestamp
   return gps_timestamp;
}

void gps_get_llh(float *lat, float *lon, float *ht)
{
   // Return the most recently received GPS position
   *lat = lat_degrees;
   *lon = lon_degrees;
   *ht = height_meters;
}

void gps_task(void *args)
{
   // Initialize the GPS module
   gps_init();

   // Loop forever listening for GPS messages
   while (true)
      gps_process_next_char();
}
