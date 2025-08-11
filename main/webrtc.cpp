#include <opus.h>
#include <peer.h>

PeerConnection *reflect_new_peer_connection() {
  PeerConfiguration peer_connection_config = {
      .ice_servers = {},
      .audio_codec = CODEC_OPUS,
      .video_codec = CODEC_NONE,
      .datachannel = DATA_CHANNEL_STRING,
      .onaudiotrack = [](uint8_t *data, size_t size, void *userdata) -> void {},
      .onvideotrack = NULL,
      .on_request_keyframe = NULL,
      .user_data = NULL,
  };

  auto peer_connection = peer_connection_create(&peer_connection_config);
  assert(peer_connection != NULL);

  return peer_connection;
}
