#define _CRT_SECURE_NO_WARNINGS
#define INITGUID
#include "collector.h"
#include <evntrace.h>
#include <evntcons.h>
#include <shellapi.h>
#include <sddl.h>
#include <stdio.h>
#include <wchar.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
static const GUID tcp_guid={0x9a280ac0,0xc8e0,0x11d1,{0x84,0xe2,0x00,0xc0,0x4f,0xb9,0x98,0xa2}};
static const GUID udp_guid={0xbf3a50c5,0xa9c9,0x4988,{0xa0,0x05,0x2d,0xf0,0xb7,0xc8,0x0f,0x80}};
#define QUEUE_SIZE 512
static TrackPacket queue[QUEUE_SIZE];static unsigned queued;static uint64_t lost;static SRWLOCK queue_lock=SRWLOCK_INIT;
typedef struct { DWORD pid;HANDLE process;uint64_t created;wchar_t path[TRACK_PATH]; } Identity;
static Identity identities[128];static unsigned next_identity;
static void identity(DWORD pid,uint64_t stamp,wchar_t *out) {
    for(unsigned i=0;i<128;i++)if(identities[i].pid==pid && identities[i].process && WaitForSingleObject(identities[i].process,0)==WAIT_TIMEOUT){wcscpy(out,identities[i].created<=stamp?identities[i].path:L"[Unresolved process]");return;}
    Identity *id=&identities[next_identity++%128];if(id->process)CloseHandle(id->process);memset(id,0,sizeof(*id));
    id->pid=pid;id->process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);DWORD n=TRACK_PATH;
    if(!id->process || !QueryFullProcessImageNameW(id->process,0,id->path,&n)){if(id->process)CloseHandle(id->process);id->process=NULL;wcscpy(out,L"[Unresolved process]");return;}
    FILETIME created,exit,kernel,user;ULARGE_INTEGER value;if(!GetProcessTimes(id->process,&created,&exit,&kernel,&user)){wcscpy(out,L"[Unresolved process]");return;}value.LowPart=created.dwLowDateTime;value.HighPart=created.dwHighDateTime;id->created=value.QuadPart;if(id->created>stamp){wcscpy(out,L"[Unresolved process]");return;}
    CharLowerBuffW(id->path,(DWORD)wcslen(id->path));wcscpy(out,id->path);
}
static void WINAPI on_event(EVENT_RECORD *event) {
    if(memcmp(&event->EventHeader.ProviderId,&tcp_guid,sizeof(GUID)) && memcmp(&event->EventHeader.ProviderId,&udp_guid,sizeof(GUID)))return;
    UCHAR op=event->EventHeader.EventDescriptor.Opcode;
    if(op!=10 && op!=11 && op!=26 && op!=27)return; /* exclude retransmits, connect and copy events */
    unsigned minimum=op>=26?40:16;
    if(event->EventHeader.EventDescriptor.Version<1 || event->EventHeader.EventDescriptor.Version>2 || event->UserDataLength<minimum){AcquireSRWLockExclusive(&queue_lock);lost++;ReleaseSRWLockExclusive(&queue_lock);return;}
    const unsigned char *data=event->UserData;DWORD pid,size;memcpy(&pid,data,4);memcpy(&size,data+4,4);if(!size)return;
    if(op<26){if(data[8]==127 || data[12]==127)return;}
    else {static const unsigned char loop[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};if(!memcmp(data+8,loop,16)||!memcmp(data+24,loop,16))return;}
    TrackPacket packet={0};packet.magic=TRACK_MAGIC;packet.stamp=(event->EventHeader.TimeStamp.QuadPart/10000000LL)-11644473600LL;packet.stamp-=packet.stamp%60;
    identity(pid,(uint64_t)event->EventHeader.TimeStamp.QuadPart,packet.path);if(op==11 || op==27)packet.down=size;else packet.up=size;
    AcquireSRWLockExclusive(&queue_lock);
    unsigned i;for(i=0;i<queued;i++)if(queue[i].stamp==packet.stamp && !wcscmp(queue[i].path,packet.path))break;
    if(i<queued){queue[i].down+=packet.down;queue[i].up+=packet.up;}else if(queued<QUEUE_SIZE)queue[queued++]=packet;else lost++;
    ReleaseSRWLockExclusive(&queue_lock);
}
static TRACEHANDLE consumer;
static DWORD WINAPI consume(void *unused){(void)unused;return ProcessTrace(&consumer,1,NULL,NULL);}
static int send_packet(HANDLE pipe,HANDLE parent,const TrackPacket *packet) {
    OVERLAPPED ov={0};ov.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);if(!ov.hEvent)return 0;
    DWORD bytes=0;BOOL ok=WriteFile(pipe,packet,sizeof(*packet),&bytes,&ov);
    if(!ok && GetLastError()==ERROR_IO_PENDING){HANDLE waits[2]={parent,ov.hEvent};if(WaitForMultipleObjects(2,waits,FALSE,3000)==WAIT_OBJECT_0+1)ok=GetOverlappedResult(pipe,&ov,&bytes,FALSE);else {CancelIoEx(pipe,&ov);GetOverlappedResult(pipe,&ov,&bytes,TRUE);ok=FALSE;}}
    CloseHandle(ov.hEvent);return ok && bytes==sizeof(*packet);
}
int collector_run(const wchar_t *pipe_name,DWORD parent_id) {
    if(wcsncmp(pipe_name,L"\\\\.\\pipe\\NetPulse-",18))return 1;
    HANDLE parent=OpenProcess(SYNCHRONIZE|PROCESS_QUERY_LIMITED_INFORMATION,FALSE,parent_id);if(!parent)return 1;
    wchar_t own[MAX_PATH],other[MAX_PATH];DWORD n=MAX_PATH;GetModuleFileNameW(NULL,own,MAX_PATH);
    if(!QueryFullProcessImageNameW(parent,0,other,&n)||_wcsicmp(own,other)){CloseHandle(parent);return 1;}
    HANDLE pipe=CreateFileW(pipe_name,GENERIC_WRITE,0,NULL,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,NULL);
    if(pipe==INVALID_HANDLE_VALUE){CloseHandle(parent);return 1;}ULONG server=0;
    if(!GetNamedPipeServerProcessId(pipe,&server)||server!=parent_id){CloseHandle(pipe);CloseHandle(parent);return 1;}
    wchar_t stop_name[96];swprintf(stop_name,96,L"Local\\NetPulse.Stop.%ls",pipe_name+18);HANDLE stop=OpenEventW(SYNCHRONIZE,FALSE,stop_name);
    if(!stop){CloseHandle(pipe);CloseHandle(parent);return 1;}
    struct {EVENT_TRACE_PROPERTIES p;wchar_t name[96];} props;memset(&props,0,sizeof(props));
    swprintf(props.name,96,L"NetPulse.Network.%lu",GetCurrentProcessId());props.p.Wnode.BufferSize=sizeof(props);props.p.Wnode.Flags=WNODE_FLAG_TRACED_GUID;props.p.Wnode.ClientContext=2;CoCreateGuid(&props.p.Wnode.Guid);
    props.p.LogFileMode=EVENT_TRACE_REAL_TIME_MODE|EVENT_TRACE_SYSTEM_LOGGER_MODE;props.p.EnableFlags=EVENT_TRACE_FLAG_NETWORK_TCPIP;props.p.BufferSize=64;props.p.MinimumBuffers=4;props.p.MaximumBuffers=32;props.p.FlushTimer=1;props.p.LoggerNameOffset=sizeof(EVENT_TRACE_PROPERTIES);
    TRACEHANDLE session=0;ULONG error=StartTraceW(&session,props.name,&props.p);TrackPacket status={0};status.magic=TRACK_MAGIC;status.error=error;
    if(!send_packet(pipe,parent,&status)||error){if(!error)ControlTraceW(session,NULL,&props.p,EVENT_TRACE_CONTROL_STOP);CloseHandle(stop);CloseHandle(pipe);CloseHandle(parent);return 1;}
    EVENT_TRACE_LOGFILEW log={0};log.LoggerName=props.name;log.ProcessTraceMode=PROCESS_TRACE_MODE_REAL_TIME|PROCESS_TRACE_MODE_EVENT_RECORD;log.EventRecordCallback=on_event;consumer=OpenTraceW(&log);
    HANDLE thread=consumer==INVALID_PROCESSTRACE_HANDLE?NULL:CreateThread(NULL,0,consume,NULL,0,NULL);
    TrackPacket *batch=calloc(QUEUE_SIZE,sizeof(TrackPacket));int running=thread && batch;
    if(!running){status.error=ERROR_NOT_ENOUGH_MEMORY;send_packet(pipe,parent,&status);}
    HANDLE waits[2]={parent,stop};
    while(running && WaitForMultipleObjects(2,waits,FALSE,1000)==WAIT_TIMEOUT){
        AcquireSRWLockExclusive(&queue_lock);unsigned count=queued;memcpy(batch,queue,count*sizeof(*batch));queued=0;status.lost=lost;ReleaseSRWLockExclusive(&queue_lock);
        if(ControlTraceW(session,NULL,&props.p,EVENT_TRACE_CONTROL_QUERY)==ERROR_SUCCESS)status.lost+=props.p.EventsLost+props.p.RealTimeBuffersLost;
        for(unsigned i=0;i<count && running;i++)running=send_packet(pipe,parent,&batch[i]);
        if(running)running=send_packet(pipe,parent,&status);
        if(WaitForSingleObject(thread,0)!=WAIT_TIMEOUT)running=0;
    }
    ControlTraceW(session,NULL,&props.p,EVENT_TRACE_CONTROL_STOP);
    if(thread)WaitForSingleObject(thread,3000);
    if(consumer!=INVALID_PROCESSTRACE_HANDLE)CloseTrace(consumer);
    if(thread){WaitForSingleObject(thread,INFINITE);CloseHandle(thread);}
    if(batch && WaitForSingleObject(parent,0)==WAIT_TIMEOUT){
        for(unsigned i=0;i<queued;i++)if(!send_packet(pipe,parent,&queue[i]))break;
        status.lost=lost+props.p.EventsLost+props.p.RealTimeBuffersLost;send_packet(pipe,parent,&status);
    }
    free(batch);CloseHandle(stop);
    for(unsigned i=0;i<128;i++)if(identities[i].process)CloseHandle(identities[i].process);
    CloseHandle(pipe);CloseHandle(parent);return 0;
}
static HANDLE server_pipe=INVALID_HANDLE_VALUE,helper,stop_event;static OVERLAPPED read_ov;static TrackPacket incoming;static int connecting,pending;
void tracker_request_stop(void){if(stop_event)SetEvent(stop_event);}
int tracker_enabled(void){return server_pipe!=INVALID_HANDLE_VALUE;}
void tracker_stop(void){tracker_request_stop();if(server_pipe!=INVALID_HANDLE_VALUE){CancelIoEx(server_pipe,NULL);if(pending){DWORD n;GetOverlappedResult(server_pipe,&read_ov,&n,TRUE);}CloseHandle(server_pipe);server_pipe=INVALID_HANDLE_VALUE;}if(read_ov.hEvent)CloseHandle(read_ov.hEvent);memset(&read_ov,0,sizeof(read_ov));if(helper)CloseHandle(helper);helper=NULL;if(stop_event)CloseHandle(stop_event);stop_event=NULL;connecting=pending=0;}
int tracker_start(HWND owner) {
    if(tracker_enabled())return 1;
    HANDLE token;DWORD size=0;LPWSTR sid=NULL;PSECURITY_DESCRIPTOR sd=NULL;
    if(!OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&token))return 0;GetTokenInformation(token,TokenUser,NULL,0,&size);TOKEN_USER *user=malloc(size);
    int ok=user && GetTokenInformation(token,TokenUser,user,size,&size) && ConvertSidToStringSidW(user->User.Sid,&sid);CloseHandle(token);free(user);if(!ok)return 0;
    wchar_t sddl[256];swprintf(sddl,256,L"D:P(A;;GA;;;SY)(A;;GA;;;BA)(A;;GA;;;%ls)",sid);LocalFree(sid);
    if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl,SDDL_REVISION_1,&sd,NULL))return 0;
    SECURITY_ATTRIBUTES sa={sizeof(sa),sd,FALSE};GUID guid;CoCreateGuid(&guid);wchar_t guid_text[40],name[96];StringFromGUID2(&guid,guid_text,40);swprintf(name,96,L"\\\\.\\pipe\\NetPulse-%ls",guid_text);
    server_pipe=CreateNamedPipeW(name,PIPE_ACCESS_INBOUND|FILE_FLAG_OVERLAPPED|FILE_FLAG_FIRST_PIPE_INSTANCE,PIPE_TYPE_MESSAGE|PIPE_READMODE_MESSAGE|PIPE_WAIT|PIPE_REJECT_REMOTE_CLIENTS,1,0,sizeof(TrackPacket)*64,0,&sa);wchar_t stop_name[96];swprintf(stop_name,96,L"Local\\NetPulse.Stop.%ls",guid_text);stop_event=CreateEventW(&sa,TRUE,FALSE,stop_name);LocalFree(sd);if(server_pipe==INVALID_HANDLE_VALUE || !stop_event){tracker_stop();return 0;}
    read_ov.hEvent=CreateEventW(NULL,TRUE,FALSE,NULL);if(!read_ov.hEvent){tracker_stop();return 0;}
    BOOL connected=ConnectNamedPipe(server_pipe,&read_ov);DWORD error=GetLastError();connecting=1;pending=!connected && error==ERROR_IO_PENDING;
    if(!connected && !pending && error!=ERROR_PIPE_CONNECTED){tracker_stop();return 0;}
    wchar_t exe[MAX_PATH],args[160];GetModuleFileNameW(NULL,exe,MAX_PATH);swprintf(args,160,L"--collector %ls %lu",name,GetCurrentProcessId());
    SHELLEXECUTEINFOW launch={0};launch.cbSize=sizeof(launch);launch.fMask=SEE_MASK_NOCLOSEPROCESS;launch.hwnd=owner;launch.lpVerb=L"runas";launch.lpFile=exe;launch.lpParameters=args;launch.nShow=SW_HIDE;
    if(!ShellExecuteExW(&launch)){tracker_stop();return 0;}helper=launch.hProcess;return 1;
}
int tracker_read(TrackPacket *packet) {
    if(!tracker_enabled())return 0;DWORD bytes=0;
    if(pending){if(!GetOverlappedResult(server_pipe,&read_ov,&bytes,FALSE)){if(GetLastError()==ERROR_IO_INCOMPLETE){if(helper && WaitForSingleObject(helper,0)==WAIT_OBJECT_0){tracker_stop();return -1;}return 0;}tracker_stop();return -1;}pending=0;if(!connecting){if(bytes!=sizeof(incoming)||incoming.magic!=TRACK_MAGIC){tracker_stop();return -1;}incoming.path[TRACK_PATH-1]=0;*packet=incoming;return 1;}}
    connecting=0;ResetEvent(read_ov.hEvent);
    if(ReadFile(server_pipe,&incoming,sizeof(incoming),&bytes,&read_ov)){if(bytes!=sizeof(incoming)||incoming.magic!=TRACK_MAGIC){tracker_stop();return -1;}incoming.path[TRACK_PATH-1]=0;*packet=incoming;return 1;}
    if(GetLastError()==ERROR_IO_PENDING){pending=1;return 0;}tracker_stop();return -1;
}
