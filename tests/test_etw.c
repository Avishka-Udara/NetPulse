#include <assert.h>
#include "../src/collector.c"
int main(void){
    EVENT_RECORD e={0};unsigned char bytes[64]={0};DWORD pid=GetCurrentProcessId(),size=100;
    memcpy(bytes,&pid,4);memcpy(bytes+4,&size,4);bytes[8]=8;bytes[12]=1;
    FILETIME ft;GetSystemTimeAsFileTime(&ft);e.EventHeader.TimeStamp.LowPart=ft.dwLowDateTime;e.EventHeader.TimeStamp.HighPart=(LONG)ft.dwHighDateTime;
    e.EventHeader.ProviderId=tcp_guid;e.EventHeader.ProcessId=99999;e.EventHeader.EventDescriptor.Version=2;e.EventHeader.EventDescriptor.Opcode=10;e.UserData=bytes;e.UserDataLength=64;
    on_event(&e);assert(queued==1 && queue[0].up==100 && queue[0].down==0);assert(wcsstr(queue[0].path,L"test_etw.exe"));
    e.EventHeader.EventDescriptor.Opcode=11;on_event(&e);assert(queue[0].down==100);
    e.EventHeader.EventDescriptor.Opcode=14;on_event(&e);assert(queue[0].up==100); /* no double counting retransmits */
    e.EventHeader.ProviderId=udp_guid;e.EventHeader.EventDescriptor.Opcode=26;on_event(&e);assert(queue[0].up==200);
    e.EventHeader.EventDescriptor.Opcode=27;on_event(&e);assert(queue[0].down==200);
    e.UserDataLength=4;on_event(&e);assert(lost==1 && queue[0].down==200);
    e.UserDataLength=64;e.EventHeader.EventDescriptor.Opcode=10;bytes[8]=127;on_event(&e);assert(queue[0].up==200);
    puts("PASS: ETW payload PID, TCP/UDP v4/v6, direction, retransmit exclusion, truncated payload and loopback");return 0;
}
