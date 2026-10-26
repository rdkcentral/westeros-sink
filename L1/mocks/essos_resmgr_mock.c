#include "essos-resmgr.h"
struct _EssRMgr { int unused; };

EssRMgr *EssRMgrCreate(void) { return 0; }
void EssRMgrDestroy(EssRMgr *rm) { (void)rm; }
bool EssRMgrResourceGetCaps(EssRMgr *rm, int type, int id, EssRMgrCaps *caps)
{ (void)rm; (void)type; (void)id; (void)caps; return false; }
bool EssRMgrReleaseResource(EssRMgr *rm, int type, int id)
{ (void)rm; (void)type; (void)id; return true; }
bool EssRMgrGetPolicyPriorityTie(EssRMgr *rm) { (void)rm; return false; }
bool EssRMgrRequestResource(EssRMgr *rm, int type, EssRMgrRequest *request)
{ (void)rm; (void)type; (void)request; return false; }
bool EssRMgrResourceSetState(EssRMgr *rm, int type, int id, int state)
{ (void)rm; (void)type; (void)id; (void)state; return true; }
bool EssRMgrRequestSetPriority(EssRMgr *rm, int type, int id, unsigned int priority)
{ (void)rm; (void)type; (void)id; (void)priority; return true; }
bool EssRMgrRequestSetUsage(EssRMgr *rm, int type, int id, const EssRMgrUsage *usage)
{ (void)rm; (void)type; (void)id; (void)usage; return true; }