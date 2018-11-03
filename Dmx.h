#include <inttypes.h>

#ifndef DMXDev_h
#define DMXDev_h
class DMXDev {
public:
  void init();
  void init(int MaxChan);
  uint8_t read(int Channel);
  void write(int channel, uint8_t value);
  void update();
  void end();
};

#endif
