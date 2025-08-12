#include "bsp/esp-bsp.h"

#include <opus.h>
#include <peer.h>

#define CHANNELS 1
#define SAMPLE_RATE (8000)
#define BITS_PER_SAMPLE 16

#define PCM_BUFFER_SIZE 320

#define OPUS_BUFFER_SIZE 1276
#define OPUS_ENCODER_BITRATE 30000
#define OPUS_ENCODER_COMPLEXITY 0

esp_codec_dev_sample_info_t fs = {
    .bits_per_sample = BITS_PER_SAMPLE,
    .channel = CHANNELS,
    .channel_mask = 0,
    .sample_rate = SAMPLE_RATE,
    .mclk_multiple = 0,
};

esp_codec_dev_handle_t mic_codec_dev;
esp_codec_dev_handle_t spk_codec_dev;

OpusDecoder *opus_decoder = NULL;
opus_int16 *decoder_buffer = NULL;

OpusEncoder *opus_encoder = NULL;
uint8_t *encoder_output_buffer = NULL;
uint8_t *read_buffer = NULL;

void reflect_audio() {
  // Speaker
  spk_codec_dev = bsp_audio_codec_speaker_init();
  assert(spk_codec_dev);
  esp_codec_dev_open(spk_codec_dev, &fs);
  esp_codec_dev_set_out_vol(spk_codec_dev, 100);

  // Microphone
  mic_codec_dev = bsp_audio_codec_microphone_init();
  esp_codec_dev_set_in_gain(mic_codec_dev, 42.0);
  esp_codec_dev_open(mic_codec_dev, &fs);

  // Decoder
  int opus_error = 0;
  opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, &opus_error);
  assert(opus_error == OPUS_OK);
  decoder_buffer = (opus_int16 *)malloc(PCM_BUFFER_SIZE);

  // Encoder
  opus_encoder =
      opus_encoder_create(SAMPLE_RATE, 1, OPUS_APPLICATION_VOIP, &opus_error);
  assert(opus_error == OPUS_OK);
  assert(opus_encoder_init(opus_encoder, SAMPLE_RATE, 1,
                           OPUS_APPLICATION_VOIP) == OPUS_OK);

  opus_encoder_ctl(opus_encoder, OPUS_SET_BITRATE(OPUS_ENCODER_BITRATE));
  opus_encoder_ctl(opus_encoder, OPUS_SET_COMPLEXITY(OPUS_ENCODER_COMPLEXITY));
  opus_encoder_ctl(opus_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));

  read_buffer =
      (uint8_t *)heap_caps_malloc(PCM_BUFFER_SIZE, MALLOC_CAP_DEFAULT);
  encoder_output_buffer = (uint8_t *)malloc(OPUS_BUFFER_SIZE);
}

void reflect_play_audio(uint8_t *data, size_t size) {
  auto decoded_size =
      opus_decode(opus_decoder, data, size, decoder_buffer, PCM_BUFFER_SIZE, 0);

  if (decoded_size > 0) {
    esp_codec_dev_write(spk_codec_dev, decoder_buffer, PCM_BUFFER_SIZE);
  }
}

void reflect_send_audio(PeerConnection *peer_connection) {
  ESP_ERROR_CHECK(
      esp_codec_dev_read(mic_codec_dev, read_buffer, PCM_BUFFER_SIZE));

  auto encoded_size = opus_encode(opus_encoder, (const opus_int16 *)read_buffer,
                                  PCM_BUFFER_SIZE / sizeof(uint16_t),
                                  encoder_output_buffer, OPUS_BUFFER_SIZE);
  peer_connection_send_audio(peer_connection, encoder_output_buffer,
                             encoded_size);
}
