#include "../src/integrations/FFmpegPolicy.h"
#include <cstdio>
int main() {
  if (!MediaCap::FFmpegPolicy::compatible()) {
    std::fprintf(stderr, "MediaCap requires LGPL FFmpeg; GPL/nonfree or "
                         "unknown licenses are rejected.\n");
    return 1;
  }
  int missing = 0;
  for (const char *name : MediaCap::FFmpegPolicy::encoders) {
    if (!avcodec_find_encoder_by_name(name)) {
      std::fprintf(stderr, "Missing required encoder: %s\n", name);
      ++missing;
    }
  }
  if (missing)
    return 2;
  std::printf("FFmpeg %s: LGPL, all 9 encoders present\n%s\n",
              av_version_info(), avcodec_configuration());
  return 0;
}
