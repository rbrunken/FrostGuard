#ifndef MyDateTime_h
#define MyDateTime_h

#include <ESPDateTime.h>
constexpr static const char* MyISO8601 = "%FT%TZ";
#define FormatTime(x) (DateTimeParts::from(x).format(MyISO8601).c_str())
#endif