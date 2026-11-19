#ifndef WESTEROS_SINK_L1_ESSOS_RESMGR_H
#define WESTEROS_SINK_L1_ESSOS_RESMGR_H

#include <stdbool.h>

typedef struct _EssRMgr EssRMgr;
typedef enum { EssRMgrResType_videoDecoder = 1 } EssRMgrResType;
typedef enum { EssRMgrEvent_granted = 1, EssRMgrEvent_revoked = 2 } EssRMgrEvent;
typedef enum { EssRMgrRes_idle = 0, EssRMgrRes_paused = 1, EssRMgrRes_active = 2 } EssRMgrResState;
typedef enum {
    EssRMgrVidCap_software = 1u << 0,
    EssRMgrVidUse_fullResolution = 1u << 0,
    EssRMgrVidUse_fullQuality = 1u << 1,
    EssRMgrVidUse_fullPerformance = 1u << 2
} EssRMgrVideoFlags;

typedef struct { int maxWidth; int maxHeight; } EssRMgrVideoInfo;
typedef struct { EssRMgrVideoInfo video; } EssRMgrInfo;
typedef struct { unsigned int capabilities; EssRMgrInfo info; } EssRMgrCaps;
typedef struct { unsigned int usage; EssRMgrInfo info; } EssRMgrUsage;
typedef void (*EssRMgrNotifyCB)(EssRMgr *, int, int, int, void *);
typedef struct {
    int type;
    unsigned int usage;
    unsigned int priority;
    EssRMgrInfo info;
    bool asyncEnable;
    EssRMgrNotifyCB notifyCB;
    void *notifyUserData;
    int assignedId;
    int requestId;
    unsigned int assignedCaps;
} EssRMgrRequest;

EssRMgr *EssRMgrCreate(void);
void EssRMgrDestroy(EssRMgr *rm);
bool EssRMgrResourceGetCaps(EssRMgr *rm, int type, int requestId, EssRMgrCaps *caps);
bool EssRMgrReleaseResource(EssRMgr *rm, int type, int requestId);
bool EssRMgrGetPolicyPriorityTie(EssRMgr *rm);
bool EssRMgrRequestResource(EssRMgr *rm, int type, EssRMgrRequest *request);
bool EssRMgrResourceSetState(EssRMgr *rm, int type, int requestId, int state);
bool EssRMgrRequestSetPriority(EssRMgr *rm, int type, int requestId, unsigned int priority);
bool EssRMgrRequestSetUsage(EssRMgr *rm, int type, int requestId, const EssRMgrUsage *usage);

#endif