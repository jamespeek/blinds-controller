#pragma once

enum class RemoteMotionEffect : unsigned char {
  Start,
  Continue,
  Stop,
};

constexpr RemoteMotionEffect remoteMotionEffect(bool isMoving, bool movingUp, bool receivedUp) {
  if (!isMoving) return RemoteMotionEffect::Start;
  return movingUp == receivedUp ? RemoteMotionEffect::Continue : RemoteMotionEffect::Stop;
}
