#include <Arduino.h>
#include <stdarg.h>
#include "diagnostics.h"
// One Serial.write per line; HardwareSerial serializes concurrent writers.
extern "C" void disco_log(const char *level,const char *tag,const char *format,...) {
#if SERIAL_DIAGNOSTICS
  char line[640];
  int prefix=snprintf(line,sizeof(line),"[%lu ms] %s %s: ",millis(),level,tag);
  if(prefix<0 || (size_t)prefix>=sizeof(line)-2) return;
  va_list args; va_start(args,format);
  vsnprintf(line+prefix,sizeof(line)-prefix-2,format,args); va_end(args);
  size_t length=strlen(line); line[length++]='\r'; line[length++]='\n';
  Serial.write((const uint8_t*)line,length);
#endif
}
