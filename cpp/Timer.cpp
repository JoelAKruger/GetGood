static u64 CPUFrequency;

static inline uint64_t GetCPUFrequency(int MillisecondsToWait)
{
    LARGE_INTEGER WindowsFreq;
    QueryPerformanceFrequency(&WindowsFreq);
    
    uint64_t CPUStart = __rdtsc();
    
    LARGE_INTEGER WindowsStart;
    QueryPerformanceCounter(&WindowsStart);
    
    uint64_t WindowsWaitTime = WindowsFreq.QuadPart * MillisecondsToWait / 1000;
    
    LARGE_INTEGER WindowsEnd;
    
    uint64_t WindowsElapsed = 0;
    while (WindowsElapsed < WindowsWaitTime)
    {
        QueryPerformanceCounter(&WindowsEnd);
        WindowsElapsed = WindowsEnd.QuadPart - WindowsStart.QuadPart;
    }
    
    uint64_t CPUEnd = __rdtsc();
    uint64_t CPUElapsed = CPUEnd - CPUStart;    
    
    if (WindowsElapsed)
    {
        CPUFrequency = WindowsFreq.QuadPart * CPUElapsed / WindowsElapsed;
        return CPUFrequency;
    }
    return 0;
}

static inline float CPUTimeToSeconds(uint64_t CPUTime, uint64_t CPUFrequency)
{
    return (float)CPUTime / CPUFrequency;
}

static inline uint64_t ReadCPUTimer()
{
    return __rdtsc();
}