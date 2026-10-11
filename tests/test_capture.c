#define _CRT_SECURE_NO_WARNINGS
#include "collector.h"
#include <winhttp.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
static volatile LONG failures;
static uint64_t body_bytes;static unsigned transfers;
static DWORD duration=1;
static DWORD WINAPI download_work(void *unused){
    (void)unused;ULONGLONG end=GetTickCount64()+(ULONGLONG)duration*1000;
    do {
    HINTERNET session=WinHttpOpen(L"NetPulse capture validation",WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,NULL,NULL,0);
    if(!session)return 0;WinHttpSetTimeouts(session,10000,10000,10000,10000);
    HINTERNET connection=WinHttpConnect(session,L"raw.githubusercontent.com",INTERNET_DEFAULT_HTTPS_PORT,0);
    HINTERNET request=connection?WinHttpOpenRequest(connection,L"GET",L"/Avishka-Udara/NetPulse/main/vendor/sqlite3.h",NULL,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE):NULL;
    DWORD received=0,n=0;char buffer[8192];int ok=request && WinHttpSendRequest(request,NULL,0,NULL,0,0,0) && WinHttpReceiveResponse(request,NULL);
    if(ok)while(WinHttpReadData(request,buffer,sizeof(buffer),&n) && n)received+=n;
    if(request)WinHttpCloseHandle(request);if(connection)WinHttpCloseHandle(connection);WinHttpCloseHandle(session);
    if(!ok || received<100000)InterlockedIncrement(&failures);body_bytes+=received;transfers++;
        if(duration>1 && GetTickCount64()<end)Sleep(5000);
    }while(GetTickCount64()<end);
    return 0;
}
int wmain(int argc,wchar_t **argv){
    if(argc==4 && !wcscmp(argv[1],L"--collector"))return collector_run(argv[2],wcstoul(argv[3],NULL,10));
    if(!tracker_start(NULL)){puts("FAIL: collector launch declined or unavailable");return 1;}
    TrackPacket p;int ready=0;ULONGLONG until=GetTickCount64()+10000;uint64_t down=0,up=0,lost=0;
    while(GetTickCount64()<until && !ready){int r=tracker_read(&p);if(r<0)break;if(r==1){if(p.error){printf("FAIL: ETW start error %lu\n",(unsigned long)p.error);tracker_stop();return 1;}if(!p.path[0])ready=1;}Sleep(10);}
    if(!ready){puts("FAIL: no collector heartbeat");tracker_stop();return 1;}
    if(argc==3 && !wcscmp(argv[1],L"--seconds")){duration=wcstoul(argv[2],NULL,10);if(duration<1 || duration>600){tracker_stop();return 2;}}
    HANDLE worker=CreateThread(NULL,0,download_work,NULL,0,NULL);if(!worker){tracker_stop();return 1;}
    while(WaitForSingleObject(worker,0)==WAIT_TIMEOUT){int r=tracker_read(&p);if(r<0){InterlockedIncrement(&failures);break;}if(r==1){if(wcsstr(p.path,L"test_capture.exe")){down+=p.down;up+=p.up;}if(!p.path[0])lost=p.lost;if(p.error)InterlockedIncrement(&failures);}else Sleep(10);}
    WaitForSingleObject(worker,INFINITE);CloseHandle(worker);
    until=GetTickCount64()+6000;
    while(GetTickCount64()<until){int r=tracker_read(&p);if(r<0)break;if(r==1){if(wcsstr(p.path,L"test_capture.exe")){down+=p.down;up+=p.up;}if(!p.path[0])lost=p.lost;}else Sleep(10);}
    tracker_request_stop();until=GetTickCount64()+6000;
    while(tracker_enabled() && GetTickCount64()<until){int r=tracker_read(&p);if(r==1){if(wcsstr(p.path,L"test_capture.exe")){down+=p.down;up+=p.up;}if(!p.path[0])lost=p.lost;}else Sleep(10);}
    int clean=!tracker_enabled();tracker_stop();
    printf("Transfers=%u; HTTP body=%llu; attributed download=%llu upload=%llu; lost events=%llu; clean stop=%d\n",transfers,(unsigned long long)body_bytes,(unsigned long long)down,(unsigned long long)up,(unsigned long long)lost,clean);
    if(failures || body_bytes<100000 || down<body_bytes || down>body_bytes+transfers*1000000ULL || !up || lost || !clean)return 1;
    puts("PASS: live HTTPS transfer attributed to the requesting executable and collector drained on stop");return 0;
}
