#include "bsp/esp-bsp.h"
#include <opus.h>

#define CHANNELS 1
#define SAMPLE_RATE (24000)
#define BITS_PER_SAMPLE 16

#define PCM_BUFFER_SIZE 960

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
  int decoder_error = 0;
  opus_decoder = opus_decoder_create(SAMPLE_RATE, 1, &decoder_error);
  assert(decoder_error == OPUS_OK);

  decoder_buffer = (opus_int16 *)malloc(PCM_BUFFER_SIZE);
}

void reflect_play_audio(uint8_t *data, size_t size) {
  auto decoded_size =
      opus_decode(opus_decoder, data, size, decoder_buffer, PCM_BUFFER_SIZE, 0);

  if (decoded_size > 0) {
    esp_codec_dev_write(spk_codec_dev, decoder_buffer, PCM_BUFFER_SIZE);
  }
}
