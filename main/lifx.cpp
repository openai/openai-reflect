#include <arpa/inet.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BROADCAST_IP "255.255.255.255"
#define LIFX_PORT 56700

#pragma pack(push, 1)
typedef struct {
  uint16_t size;
  uint16_t protocol : 12;
  uint16_t addressable : 1;
  uint16_t tagged : 1;
  uint16_t origin : 2;
  uint32_t source;

  uint8_t target[8];
  uint8_t reserved[6];
  uint8_t res_required : 1;
  uint8_t ack_required : 1;
  uint8_t : 6;
  uint8_t sequence;

  uint64_t at_time;
  uint16_t type;
  uint16_t reserved2;
} lifx_header_t;

typedef struct {
  lifx_header_t header;
  uint16_t level;
  uint32_t duration;
} lifx_set_power_t;

typedef struct {
  lifx_header_t header;
  uint8_t reserved;
  uint16_t hue;
  uint16_t saturation;
  uint16_t brightness;
  uint16_t kelvin;
  uint32_t duration;
} lifx_set_color_t;

typedef struct {
  lifx_header_t header;
  uint8_t reserved6;
  uint8_t transient;
  uint16_t hue;
  uint16_t saturation;
  uint16_t brightness;
  uint16_t kelvin;
  uint32_t period;
  float cycles;
  int16_t skew_ratio;
  uint8_t waveform;
} lifx_set_waveform_t;

#pragma pack(pop)

int lifx_socket = 0;
struct sockaddr_in lifx_addr;

void send_lifx_pkt(void *pkt, int size) {
  sendto(lifx_socket, pkt, size, 0, (struct sockaddr *)&lifx_addr,
         sizeof(lifx_addr));
}

void send_lifx_set_color(uint16_t hue, uint16_t saturation,
                         uint16_t brightness, uint16_t kelvin, uint32_t duration) {
  lifx_set_color_t pkt;
  memset(&pkt, 0, sizeof(pkt));

  pkt.header.size = sizeof(pkt);
  pkt.header.protocol = 1024;
  pkt.header.addressable = 1;
  pkt.header.tagged = 0;
  pkt.header.origin = 0;
  pkt.header.source = 0x12345678;
  memset(pkt.header.target, 0, sizeof(pkt.header.target)); // use MAC if known
  pkt.header.sequence = 1;
  pkt.header.type = 102;

  pkt.reserved = 0;
  pkt.hue = hue;
  pkt.saturation = saturation;
  pkt.brightness = brightness;
  pkt.kelvin = kelvin;
  pkt.duration = duration;

  send_lifx_pkt(&pkt, sizeof(pkt));
}

void send_lifx_set_waveform(uint16_t hue, uint16_t saturation,
                            uint16_t brightness) {
  lifx_set_waveform_t pkt;
  memset(&pkt, 0, sizeof(pkt));

  pkt.header.size = sizeof(pkt);
  pkt.header.protocol = 1024;
  pkt.header.addressable = 1;
  pkt.header.tagged = 0;
  pkt.header.origin = 0;
  pkt.header.source = 0x12345678;
  memset(pkt.header.target, 0, sizeof(pkt.header.target));
  pkt.header.res_required = 0;
  pkt.header.ack_required = 0;
  pkt.header.sequence = 1;
  pkt.header.at_time = 0;
  pkt.header.type = 103;

  pkt.reserved6 = 0;
  pkt.transient = 1;
  pkt.hue = hue;
  pkt.saturation = saturation;
  pkt.brightness = brightness;
  pkt.kelvin = 4000;
  pkt.period = 5000;
  pkt.cycles = 65535.0f;
  pkt.skew_ratio = 0;
  pkt.waveform = 1;

  send_lifx_pkt(&pkt, sizeof(pkt));
}

void send_lifx_set_power(int on) {
  lifx_set_power_t pkt;
  memset(&pkt, 0, sizeof(pkt));

  pkt.header.size = sizeof(pkt);
  pkt.header.protocol = 1024;
  pkt.header.addressable = 1;
  pkt.header.tagged = 0;
  pkt.header.origin = 0;
  pkt.header.source = 0x12345678;
  memset(pkt.header.target, 0, sizeof(pkt.header.target));
  pkt.header.res_required = 0;
  pkt.header.ack_required = 0;
  pkt.header.sequence = 1;
  pkt.header.at_time = 0;
  pkt.header.type = 21;
  pkt.level = on ? 65535 : 0;
  pkt.duration = 5000;

  send_lifx_pkt(&pkt, sizeof(pkt));
}

void reflect_lifx() {
  lifx_addr.sin_family = AF_INET;
  lifx_addr.sin_port = htons(LIFX_PORT);
  lifx_socket = socket(AF_INET, SOCK_DGRAM, 0);
  if (lifx_socket < 0) {
    return;
  }

  int broadcast = 1;
  if (setsockopt(lifx_socket, SOL_SOCKET, SO_BROADCAST, &broadcast,
                 sizeof(broadcast)) < 0) {
    return;
  }

  inet_pton(AF_INET, BROADCAST_IP, &lifx_addr.sin_addr);
  send_lifx_set_power(false);
}
