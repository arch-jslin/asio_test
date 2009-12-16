#ifndef ASIO_TEST_HELPER
#define ASIO_TEST_HELPER

#include <string>
#include <ctime>
#include <windows.h>

std::string make_daytime_string()
{
  using namespace std; // For time_t, time and ctime;
  time_t now = time(0);
  return ctime(&now);
}

int millisec_delta(LARGE_INTEGER& last)
{
    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    int delta = ((now.QuadPart - last.QuadPart) * 1000 / freq.QuadPart);
    last = now;
    return delta;
}

#endif //ASIO_TEST_HELPER
