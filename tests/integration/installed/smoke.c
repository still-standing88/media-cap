#include <MediaCap_c.h>

int main(void) {
    MediaCap_Session_Handle session = MediaCap_Session_Create();
    if (!session) return 1;
    int ok = MediaCap_Session_GetId(session) != 0 && !MediaCap_Session_IsCapturing(session);
    MediaCap_Session_Destroy(session);
    return ok ? 0 : 1;
}
