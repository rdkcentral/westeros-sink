#include <stdbool.h>

typedef void EssRMgr;
typedef void EssRMgrCaps;

EssRMgr* EssRMgrCreate(const char *displayName) { return (EssRMgr*)0x1; }
void EssRMgrDestroy(EssRMgr *rm) {}
bool EssRMgrResourceGetCaps(EssRMgr *rm, int type, int requestId, EssRMgrCaps *caps) { return true; }
bool EssRMgrReleaseResource(EssRMgr *rm, int requestId) { return true; }
int EssRMgrGetPolicyPriorityTie(EssRMgr *rm, int type, int requestId) { return 0; }
int EssRMgrRequestResource(EssRMgr *rm, int type, void *userData) { return 1; }
bool EssRMgrResourceSetState(EssRMgr *rm, int requestId, int state) { return true; }
bool EssRMgrRequestSetPriority(EssRMgr *rm, int type, int requestId, int priority) { return true; }
bool EssRMgrRequestSetUsage(EssRMgr *rm, int type, int requestId, int usage) { return true; }