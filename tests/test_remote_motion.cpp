#include <cassert>

#include "../src/remote_motion.h"

int main() {
  assert(remoteMotionEffect(false, false, true) == RemoteMotionEffect::Start);
  assert(remoteMotionEffect(true, true, true) == RemoteMotionEffect::Continue);
  assert(remoteMotionEffect(true, false, true) == RemoteMotionEffect::Stop);
}
