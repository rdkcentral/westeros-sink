/*
* This library is free software; you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public
* License as published by the Free Software Foundation; either
* version 2.1 of the License, or (at your option) any later version.
*
* This library is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* You should have received a copy of the GNU Lesser General Public
* License along with this library; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
*/
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