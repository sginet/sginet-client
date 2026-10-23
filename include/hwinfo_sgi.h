#ifndef HEADER_SGIHWINFO
#define HEADER_SGIHWINFO

#include <json-c/json.h>

#if defined(__sgi)
#include <sys/sysinfo.h>
#include <sys/sysmp.h>
#include <sys/utsname.h>
#include <invent.h>  /* not to be confused with sys/invent.h !! */
#include <sys/syssgi.h>
#ifdef IRIX5
#include <utmp.h>      /* boot time for uptime */
#else
#include <sys/sysget.h>  /* IRIX 6.5 only */
#endif
#endif

#include <stdbool.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/resource.h>
#include <sys/swap.h>
#include <sys/stat.h>
#include <sys/statfs.h>

typedef struct CPUData_ {
   unsigned long long int totalTime;
   unsigned long long int userTime; /* CPU_USER */
   unsigned long long int sysTime;  /* CPU_KERNEL */
   unsigned long long int sysAllTime;
   unsigned long long int sxbrkTime; /* CPU_SXBRK  (probably similar to WAIT) */
   unsigned long long int intrTime;  /* CPU_INTR */
   unsigned long long int idleTime;  /* CPU_IDLE */
   unsigned long long int waitTime;  /* CPU_WAIT */

   unsigned long long int totalPeriod;
   unsigned long long int userPeriod;
   unsigned long long int sysPeriod;
   unsigned long long int sysAllPeriod;
   unsigned long long int sxbrkPeriod;
   unsigned long long int intrPeriod;
   unsigned long long int idlePeriod;
   unsigned long long int waitTimePeriod;
   bool online;

   double totalPercent;
   double pct_nice;
   double pct_normal;
   double pct_irq;
   double pct_kernel;
} CPUData;

typedef struct IRIXMachine_ {
   int pageSize;
   int pageSizeKB;
   CPUData *cpuData;

   unsigned long long totalMem;
   unsigned long long usedMem;
   unsigned long long availableMem;
   unsigned long long totalSwap;
   unsigned long long usedSwap;
   unsigned long long cachedSwap;

   unsigned int activeCPUs;
   unsigned int existingCPUs;

   unsigned long long int totalCPU;
   unsigned long long int lastTotalCPU;
   unsigned long long int lastPeriod; /* time of last process stats collection */
} IRIXMachine;

#ifndef MINIMUM
#define MINIMUM(a, b)                  ((a) < (b) ? (a) : (b))
#endif

#ifndef MAXIMUM
#define MAXIMUM(a, b)                  ((a) > (b) ? (a) : (b))
#endif

#ifndef CLAMP
#define CLAMP(x, low, high)            (((x) > (high)) ? (high) : MAXIMUM(x, low))
#endif

void getSYSID(char* buffer);
int getHINVObject(struct json_object *parentObj);
void getHostname(char* buffer, size_t size);
void getIPModel(char* buffer);
void getOSRelease(char* buffer);
int getCPUCount();
int getUptime();
int readHINVCmd(char* cmd, char* returnValue);
int parse_uptime_loadavg(double *avg1, double *avg5, double *avg15);

void Machine_scan(IRIXMachine *machine);
IRIXMachine *Machine_new();
void freeMachineInfo(IRIXMachine *machine);

#endif /* HEADER_SGIHWINFO */
