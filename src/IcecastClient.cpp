#include "IcecastClient.hpp"

#include "Error.hpp"

#include <sstream>

namespace modern_ices {
namespace {

int to_shout_protocol(Protocol protocol) {
  switch (protocol) {
  case Protocol::Http:
    return SHOUT_PROTOCOL_HTTP;
  case Protocol::Icy:
    return SHOUT_PROTOCOL_ICY;
  }
  return SHOUT_PROTOCOL_HTTP;
}

std::string bitrate_value(int bitrate) {
  return std::to_string(bitrate);
}

} // namespace

ShoutRuntime::ShoutRuntime() {
  shout_init();
}

ShoutRuntime::~ShoutRuntime() {
  shout_shutdown();
}

IcecastClient::IcecastClient(const Config& config) : shout_(shout_new()) {
  if (!shout_) {
    throw Error("libshout failed to allocate a connection object");
  }

  check(shout_set_host(shout_, config.host.c_str()), "set host");
  check(shout_set_port(shout_, config.port), "set port");
  check(shout_set_user(shout_, config.user.c_str()), "set user");
  check(shout_set_password(shout_, config.password.c_str()), "set password");
  check(shout_set_mount(shout_, config.mount.c_str()), "set mount");
  check(shout_set_protocol(shout_, to_shout_protocol(config.protocol)), "set protocol");
  check(shout_set_content_format(shout_, SHOUT_FORMAT_MP3, SHOUT_USAGE_AUDIO, nullptr),
        "set content format");
  check(shout_set_audio_info(shout_, SHOUT_AI_BITRATE, bitrate_value(config.bitrate_kbps).c_str()),
        "set bitrate metadata");
  check(shout_set_meta(shout_, SHOUT_META_NAME, config.name.c_str()), "set stream name");
  check(shout_set_meta(shout_, SHOUT_META_GENRE, config.genre.c_str()), "set stream genre");
  check(shout_set_meta(shout_, SHOUT_META_DESCRIPTION, config.description.c_str()),
        "set stream description");
  check(shout_set_meta(shout_, SHOUT_META_URL, config.url.c_str()), "set stream URL");
  check(shout_set_agent(shout_, "modern-ices/0.1"), "set user agent");
}

IcecastClient::~IcecastClient() {
  close();
  if (shout_) {
    shout_free(shout_);
  }
}

void IcecastClient::connect() {
  check(shout_open(shout_), "connect to server");
}

void IcecastClient::close() noexcept {
  if (shout_ && shout_get_connected(shout_) == SHOUTERR_CONNECTED) {
    (void)shout_close(shout_);
  }
}

void IcecastClient::send(const unsigned char* data, std::size_t length) {
  if (length == 0) {
    return;
  }
  check(shout_send(shout_, data, length), "send stream data");
}

void IcecastClient::sync() noexcept {
  if (shout_) {
    shout_sync(shout_);
  }
}

void IcecastClient::check(int rc, const std::string& action) const {
  if (rc != SHOUTERR_SUCCESS) {
    throw Error(last_error(action));
  }
}

std::string IcecastClient::last_error(const std::string& action) const {
  std::ostringstream out;
  out << "libshout failed to " << action;
  if (shout_) {
    const char* detail = shout_get_error(shout_);
    if (detail && *detail) {
      out << ": " << detail;
    }
    out << " (code " << shout_get_errno(shout_) << ")";
  }
  return out.str();
}

} // namespace modern_ices
