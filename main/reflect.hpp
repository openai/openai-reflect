#pragma once

#include <esp_log.h>
#include <peer.h>

#define SDP_BUFFER_SIZE 4096

bool reflect_display_pressed(void);
void reflect_audio();
void reflect_display();
void reflect_lifx();
void reflect_peer_connection_loop();
void reflect_play_audio(uint8_t *, size_t);
void reflect_send_audio(PeerConnection *);
void reflect_set_spin(bool);
void reflect_wifi();

void send_lifx_set_color(uint16_t, uint16_t, uint16_t, uint16_t, uint32_t);
void send_lifx_set_power(int);
void send_lifx_set_waveform(bool, uint16_t, uint16_t, uint16_t, uint16_t,
                            uint32_t, float, int16_t, uint8_t);

void oai_http_request(const char *offer, char *answer);

void send_session_update(PeerConnection *peer_connection);
