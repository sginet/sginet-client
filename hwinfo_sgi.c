#include "include/hwinfo_sgi.h"

#include <fcntl.h>  /* for uptime calc */
#include <stdio.h>
#include <stdlib.h>
#include <sys/procfs.h>  /* for uptime calc */
#include <unistd.h>
#include <string.h>
#include <time.h>

extern int debug;

static void IRIXMachine_updateCPUcount(IRIXMachine *machine);
static void IRIXMachine_scanCPUTime(IRIXMachine *machine);
static void IRIXMachine_scanMemoryInfo(IRIXMachine *machine);
static inline unsigned long long saturatingSub(unsigned long long a, unsigned long long b);

void getIPModel(char *buffer) {
#if defined(__sgi)
    struct utsname sysbuf;
    if (uname(&sysbuf) < 0) {
        sprintf(buffer, "-");
    } else {
        sprintf(buffer, "%.4s", sysbuf.machine);
    }
#else
    sprintf(buffer, "IPxx");
#endif
}
void getOSRelease(char *buffer) {
#if defined(__sgi)
    struct utsname sysbuf;
    if (uname(&sysbuf) < 0) {
        sprintf(buffer, "-");
    } else {
        sprintf(buffer, "%.6s %.6s", sysbuf.sysname, sysbuf.release);
    }
#else
    sprintf(buffer, "unknown");
#endif
}
void getHostname(char *buffer, size_t size) {
    int ret;
    ret = gethostname(&buffer[0], size - 1);
    if (ret == -1) {
        return;
    }
    buffer[size - 1] = '\0';
}
int getCPUCount() {
    long int s;
#if defined(__sgi)
    /* Get configured CPUs */
    /* MP_NPROCS Returns the number of processors physically configured. */
    s = sysmp(MP_NPROCS);
#else
    s = 8;
#endif
    return s;
}

void getSYSID(char *buffer) {
#if defined(__sgi)
    /* syssgi(SGI_SYSID, buffer); */
    char sb[MAXSYSIDSIZE];
    syssgi(SGI_SYSID, &sb);
    sprintf(buffer, "%x%x%x%x%x%x%x%x%x%x%x", sb[0], sb[1], sb[2], sb[3], sb[4], sb[5], sb[6], sb[7], sb[8], sb[9], sb[10]);
#else
    sprintf(buffer, "dev-test-x86");
#endif
}
int getHINVObject(struct json_object *parentObj) {
#if defined(__sgi)
    inventory_t *iptr;
#else
    int cnt;
#endif
    char OSRelease[32];
    char hinvCPU[1000];
    char hinvProcessor[1000];
    json_object *lstInv;
    /* add a hinv object */
    json_object *hinv = json_object_new_object();

    /* IP model number */
    char ipmodel[5];
    getIPModel(ipmodel);
    json_object_object_add(hinv, "ipmodel", json_object_new_string(ipmodel));

    /* CPU count */
    json_object_object_add(hinv, "cpucount", json_object_new_int(getCPUCount()));

    /* OS release */
    getOSRelease(OSRelease);
    json_object_object_add(hinv, "osrelease", json_object_new_string(OSRelease));

    if (readHINVCmd("hinv -t cpu", hinvCPU) == 0) {
        json_object_object_add(hinv, "cpu", json_object_new_string(hinvCPU));
    }

    if (readHINVCmd("hinv -c processor | egrep -i 'GHZ|MHZ'", hinvProcessor) == 0) {
        json_object_object_add(hinv, "processor", json_object_new_string(hinvProcessor));
    }

    /* create an inv array for raw getinvent() data */
    lstInv = json_object_new_array();
#if defined(__sgi)
    setinvent();  /* start */
    iptr = getinvent();
    while (iptr) {
        if (iptr->inv_class == INV_PROCESSOR) {
            /* CPU data is a pointer to a struct format */
            json_object *thisElem = json_object_new_object();
            json_object_object_add(thisElem, "class", json_object_new_int(iptr->inv_class));
            json_object_object_add(thisElem, "state", json_object_new_int(iptr->inv_state));
            json_object_object_add(thisElem, "type", json_object_new_int(iptr->inv_type));

            if (iptr->inv_type == INV_CPUCHIP) {
                struct cpu_inv_s *cpuinfo;
                cpuinfo = (struct cpu_inv_s *)&iptr->inv_state;
                json_object_object_add(thisElem, "cpufq", json_object_new_int(cpuinfo->cpufq));
                json_object_object_add(thisElem, "cpuflavor", json_object_new_int(cpuinfo->cpuflavor));
                json_object_object_add(thisElem, "sdsize", json_object_new_int(cpuinfo->sdsize));
#ifndef IRIX5
                json_object_object_add(thisElem, "sdfreq", json_object_new_int(cpuinfo->sdfreq));  /* not in IRIX 5 */
#endif
            }

            json_object_array_add(lstInv, thisElem);
            /*
printf("--- found CPU ---\n");
            printf("inv_state: %i	| inv_type: %i	| inv_class: %i \n", iptr->inv_state, iptr->inv_type, iptr->inv_class);
            if (iptr->inv_type == INV_CPUBOARD) {
                    printf("INV_CPUBOARD: %i\n", iptr->inv_state);
                    if (iptr->inv_state == INV_IP35BOARD) printf("It's a IP35 system, I know this!!!\n");
                    if (iptr->inv_state == INV_IP28BOARD) printf("It's a IP28 system, I know this!!!\n");

            }
*/
        } else {
            json_object *thisElem = json_object_new_object();
            json_object_object_add(thisElem, "class", json_object_new_int(iptr->inv_class));
            json_object_object_add(thisElem, "state", json_object_new_int(iptr->inv_state));
            json_object_object_add(thisElem, "type", json_object_new_int(iptr->inv_type));
            json_object_array_add(lstInv, thisElem);
        }
        iptr = getinvent();
    }
    endinvent();  /* free */
#else
    /* fake data for x86 dev box */
    cnt = 0;
    while (cnt < 6) {
        json_object *thisElem = json_object_new_object();
        json_object_object_add(thisElem, "class", json_object_new_int(cnt));
        json_object_object_add(thisElem, "state", json_object_new_int(cnt + 100));
        json_object_array_add(lstInv, thisElem);
        cnt++;
    }
    {
        json_object *thisElem = json_object_new_object();
        json_object_object_add(thisElem, "class", json_object_new_int(7));
        json_object_object_add(thisElem, "type", json_object_new_int(5577));  /* NVIDIA Corporation G94 [GeForce 9600 GT] */
        json_object_object_add(thisElem, "state", json_object_new_int(1));
        json_object_array_add(lstInv, thisElem);
    }
#endif
    /* attach inv obj to hinv */
    json_object_object_add(hinv, "inv", lstInv);

    /* attach hinv obj to parent */
    json_object_object_add(parentObj, "hinv", hinv);
    return 0;
}
int parse_uptime_loadavg(double *avg1, double *avg5, double *avg15) {
    char *got;
    char line[256];
    char *la_str;
    /* Fallback for systems without getloadavg (IRIX) */
    #if defined(__sgi)
    FILE *uptime = popen("/usr/bsd/uptime", "r"); /* root as daemon on IRIX probably can't find uptime in path */
    #else
    FILE *uptime = popen("uptime", "r");
    #endif
    if (!uptime) {
        perror("popen");
        return 1;
    }

    got = fgets(line, sizeof(line), uptime);
    pclose(uptime); /* always close the pipe, also on parse errors below (daemon calls this every ping) */
    if (!got) return 1;

    /* Parse formats like "load average: 1.23, 4.56, 7.89" or "load averages: 1.23 4.56 7.89" */
    la_str = strstr(line, "load average");
    if (!la_str) {
        la_str = strstr(line, "load averages");  /* Alternate format */
        if (!la_str) return 1;
    }
    /* Retry with space-separated format */
    if (sscanf(la_str, "%*[^0-9]%lf, %lf, %lf", avg1, avg5, avg15) != 3) {        
        if (sscanf(la_str, "%*[^0-9]%lf %lf %lf", avg1, avg5, avg15) != 3) {
            return 1;
        }
    }
    return 0;
}

int getUptime() {
#if defined(__sgi) && defined(IRIX5)
    /* This would probably also work in IRIX6 */
    struct utmp req;
    struct utmp *boot;
    time_t booted = 0;
    time_t now;
    req.ut_type = BOOT_TIME;
    setutent();
    boot = getutid(&req);
    if (boot != NULL) booted = boot->ut_time;
    endutent();
    if (booted == 0) return 0;
    time(&now);
    return (int)difftime(now, booted);
#elif defined(__sgi)
    struct timeval currTime;
    int fd;
    prpsinfo_t info;
    fd = open("/proc/pinfo/0000000001", O_RDONLY, 0);  /* use 1st process to get uptime */
    if (fd == -1) return 0;

    if (ioctl(fd, PIOCPSINFO, &info) == -1) {
        close(fd);
        return 0;
    }
    if (close(fd) == -1) return 0;
    gettimeofday(&currTime, NULL);
    return (int)difftime(currTime.tv_sec, info.pr_start.tv_sec);
#else
    return 0;
#endif
}

/* I couldn't figure out how to parse the cpu_info struct, so I'll just parse hinv command output */
int readHINVCmd(char* cmd, char* returnValue) {
#if defined(__sgi)
    /* "hinv -c processor" or "hinv -t cpu" */
    char buffer[1000];
    FILE *pipe;
    int len;

    pipe = popen(cmd, "r");
    if (NULL == pipe) return 1;

    if (fgets(buffer, sizeof(buffer), pipe) == NULL) {
        pclose(pipe);
        return 1; /* no output */
    }
    pclose(pipe);

    len = strlen(buffer);
    if (len > 0 && buffer[len - 1] == '\n') buffer[len - 1] = '\0';

    sprintf(returnValue, "%s", buffer);
    return 0; /* OK */
#else
    sprintf(returnValue, "%s", "8 Intel(R) Core(TM) i7-6700 CPU @ 3.40GHz");
    return 0; /* OK */
#endif
}


static void IRIXMachine_updateCPUcount(IRIXMachine *machine) {
#if !defined(__sgi)
    int c;
#endif
    long int s;
#if defined(__sgi)
    s = sysmp(MP_NPROCS);  /* MP_NPROCS Returns the number of processors physically configured */

    if (s < 1) {
        printf("Unable to read CPU count sysmp(MP_NPROCS)\n");
        return;
    }

    machine->cpuData = realloc(machine->cpuData, s * sizeof(CPUData));
    /* for (int i = 0; i < s; i++) machine->cpuData[i].online = false; */

    machine->existingCPUs = s;

    /* Get online CPUs */
    /* MP_NAPROCS returns the number of processors that are available to schedule unrestricted processes. */
    s = sysmp(MP_NAPROCS);

    if (s < 1) {
        printf("Unable to read active CPU count sysmp(MP_NAROCS)\n");
        return;
    }
    machine->activeCPUs = s;
#else
    /* fake data for x86 */
    s = 8;
    machine->activeCPUs = s;
    machine->existingCPUs = s;
    machine->cpuData = realloc(machine->cpuData, (s + 1) * sizeof(CPUData));
    for (c = 0; c < s; c++) machine->cpuData[c].totalPercent = (c * 4);
#endif
}
static inline unsigned long long saturatingSub(unsigned long long a, unsigned long long b) {
    return a > b ? a - b : 0;
}
static void IRIXMachine_scanCPUTime(IRIXMachine *machine) {
#if defined(__sgi)
    unsigned int activeCPUs = machine->activeCPUs;
    unsigned int existingCPUs = machine->existingCPUs;
    unsigned long long totalAllCPU = 0;
    struct sysinfo *si;
    int cpu_count;
    unsigned int i;
#ifdef IRIX5
    unsigned int n;

    /* IRIX 5 has no sysget(), read the per CPU sysinfo with sysmp() */
    si = (struct sysinfo *)calloc(existingCPUs, sizeof(struct sysinfo));
    for (n = 0; n < existingCPUs; n++) {
        sysmp(MP_SAGET1, MPSA_SINFO, (char *)&si[n], sizeof(struct sysinfo), n);
    }
#else
    sgt_info_t info;
    sgt_cookie_t cookie;

    /* Query IRIX for CPUs */
    SGT_COOKIE_INIT(&cookie);
    sysget(SGT_SINFO, (char *)&info, sizeof(info), SGT_INFO | SGT_CPUS, &cookie);
    si = (struct sysinfo *)malloc(info.si_num * info.si_size);

    SGT_COOKIE_INIT(&cookie);
    sysget(SGT_SINFO, (char *)si, info.si_num * info.si_size, SGT_READ | SGT_CPUS, &cookie);
#endif

    /* make sure number of CPUs in sysinfo matches existingCPUs */
    cpu_count = existingCPUs;
#ifndef IRIX5
    if (info.si_num < existingCPUs) cpu_count = info.si_num;
#endif

    /* loop all cpus, ignore disabled cpus */
    for (i = 0; i < machine->existingCPUs; i++) {
        double totalPercent;
        unsigned long long sysAllTime;
        double total;
        CPUData *cpu = &machine->cpuData[i];
        /* IRIX CPU data: */
        unsigned long long totalTime =
            si[i].cpu[CPU_USER] +
            si[i].cpu[CPU_KERNEL] +
            si[i].cpu[CPU_INTR] +
            si[i].cpu[CPU_IDLE] +
            si[i].cpu[CPU_SXBRK] +
            si[i].cpu[CPU_WAIT];
        totalAllCPU += totalTime;

        sysAllTime = si[i].cpu[CPU_INTR] + si[i].cpu[CPU_KERNEL];

        cpu->totalPeriod = saturatingSub(totalTime, cpu->totalTime);
        cpu->userPeriod = saturatingSub(si[i].cpu[CPU_USER], cpu->userTime);
        cpu->sysPeriod = saturatingSub(si[i].cpu[CPU_KERNEL], cpu->sysTime);
        cpu->sxbrkPeriod = saturatingSub(si[i].cpu[CPU_SXBRK], cpu->sxbrkTime);

        cpu->sysAllPeriod = saturatingSub(sysAllTime, cpu->sysAllTime);
        cpu->intrPeriod = saturatingSub(si[i].cpu[CPU_INTR], cpu->intrTime);
        cpu->idlePeriod = saturatingSub(si[i].cpu[CPU_IDLE], cpu->idleTime);

        cpu->totalTime = totalTime;
        cpu->userTime = si[i].cpu[CPU_USER];
        cpu->sysTime = si[i].cpu[CPU_KERNEL];
        cpu->waitTime = si[i].cpu[CPU_WAIT];

        cpu->sysAllTime = sysAllTime;
        cpu->intrTime = si[i].cpu[CPU_INTR];
        cpu->idleTime = si[i].cpu[CPU_IDLE];

        /* percent */
        total = cpu->totalPeriod == 0 ? 1 : cpu->totalPeriod;

        cpu->pct_nice = 0;  /* cpuData->nicePeriod / total * 100.0; */
        cpu->pct_normal = cpu->userPeriod / total * 100.0;
        cpu->pct_irq = cpu->intrPeriod / total * 100.0;
        cpu->pct_kernel = cpu->sysPeriod / total * 100.0;

        totalPercent = cpu->pct_normal + cpu->pct_kernel + cpu->pct_nice + cpu->pct_irq + cpu->pct_nice;
        cpu->totalPercent = CLAMP(totalPercent, 0.0, 100.0);
    }

    free(si);
    machine->lastTotalCPU = machine->totalCPU;
    machine->totalCPU = totalAllCPU;
#endif
}

static void IRIXMachine_scanMemoryInfo(IRIXMachine *machine) {
#if defined(__sgi)
#ifdef IRIX5
    off_t swapfree;  /* IRIX 5: swapctl() writes a 32 bit off_t */
    off_t swapmax;
    struct rminfo rmi;
    long pagesize = getpagesize();
    int rc = sysmp(MP_SAGET, MPSA_RMINFO, (char *)&rmi, sizeof(rmi)) < 0 ? -1 : 0;
#else
    long long int swapfree;
    long long int swapmax;
    struct statfs fs;
    int rc = statfs("/proc/0000000001", &fs, sizeof(struct statfs), 0);
#endif
    int rc1;
    int rc2;

    if (rc == 0) {
#ifdef IRIX5
        long long memTotal = ((long long)rmi.physmem * pagesize) / 1024;
        long long memAvail = ((long long)rmi.freemem * pagesize) / 1024;
#else
        long long memTotal = (fs.f_blocks * fs.f_bsize) / 1024;
        long long memAvail = (fs.f_bfree * fs.f_bsize) / 1024;
#endif
        long long memUsed = memTotal - memAvail;

        machine->totalMem = memTotal;
        machine->availableMem = memAvail != 0 ? MINIMUM(memAvail, memTotal) : memAvail;
        machine->usedMem = memUsed;

        /* Query swap data */
        rc1 = swapctl(SC_GETSWAPMAX, &swapmax);
        rc2 = swapctl(SC_GETFREESWAP, &swapfree);
        if (rc1 + rc2 == 0) {
            machine->totalSwap = ((long long)swapmax * 512 / 1024);
            machine->usedSwap = (((long long)swapmax - swapfree) * 512 / 1024);
        }
    }
#else
    /* fake data for x86 */
    machine->totalMem = 1024 * 1024;
    machine->availableMem = 768 * 1024;
    machine->usedMem = (machine->totalMem - machine->availableMem);
    machine->totalSwap = 256 * 1024;
    machine->usedSwap = 64 * 1024;
#endif
}
void Machine_scan(IRIXMachine *machine) {
    IRIXMachine_scanCPUTime(machine);
    IRIXMachine_scanMemoryInfo(machine);
}

IRIXMachine *Machine_new() {
    IRIXMachine *machine = calloc(1, sizeof(IRIXMachine));
#if defined(__sgi)
    machine->pageSize = sysmp(MP_PGSIZE);  /* IRIX MP_PGSIZE: The page size of the system is returned (see getpagesize(2)) */
    if (machine->pageSize == -1) printf("Unable to read pagesize by sysmp(MP_PGSIZE)\n");
    machine->pageSizeKB = machine->pageSize / 1024;
#else
    /* fake data for x86 */
    machine->pageSize = 256000000;
    machine->pageSizeKB = machine->pageSize / 1024;
#endif
    IRIXMachine_updateCPUcount(machine);
    return machine;
}

void freeMachineInfo(IRIXMachine *machine) {
    free(machine->cpuData);
    free(machine);
}
