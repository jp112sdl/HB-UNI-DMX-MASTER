//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2018-11-03 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER
#define NDEBUG

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <AskSinPP.h>
#include <LowPower.h>

#include <Register.h>
#include <MultiChannelDevice.h>
#include "Dmx.h"
DMXDev dmx;


#define CONFIG_BUTTON_PIN  8
#define LED_PIN            4
#define MSG_START_KEY   0x56
#define MSG_END_KEY     0x57


// number of available peers per channel
#define PEERS_PER_CHANNEL  8

struct dmxparam_t {
  byte Channel = 0;
  byte Value = 0;
} DmxParam;

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  {0xF3, 0x42, 0x01},          // Device ID
  "JPDMX00001",                // Device Serial
  {0xF3, 0x42},                // Device Model
  0x10,                        // Firmware Version
  as::DeviceType::Remote,      // Device Type
  {0x00, 0x00}                 // Info Bytes
};

/**
   Configure the used hardware
*/
typedef AskSin<StatusLed<LED_PIN>, NoBattery, Radio<AvrSPI<10, 11, 12, 13>, 2>> Hal;
Hal hal;


DEFREGISTER(DMXReg0, MASTERID_REGS)
class DMXList0 : public RegList0<DMXReg0> {
  public:
    DMXList0 (uint16_t addr) : RegList0<DMXReg0>(addr) {}
    void defaults () {
      clear();
    }
};

DEFREGISTER(DMXReg1)
class DMXList1 : public RegList1<DMXReg1> {
  public:
    DMXList1 (uint16_t addr) : RegList1<DMXReg1>(addr) {}
    void defaults () {
      clear();
    }
};

class DMXChannel : public Channel<Hal, DMXList1, EmptyList, DefList4, PEERS_PER_CHANNEL, DMXList0>,
  public Button {

  private:
    uint8_t       repeatcnt;
    volatile bool isr;
    uint8_t       commandIdx;
    uint8_t       command[112];

  public:

    DMXChannel () : Channel(), repeatcnt(0), isr(false), commandIdx(0) {}
    virtual ~DMXChannel () {}

    Button& button () {
      return *(Button*)this;
    }

    void configChanged() {

    }

    uint8_t status () const {
      return 0;
    }

    uint8_t flags () const {
      return 0;
    }

    bool process (const Message& msg) {
      DPRINTLN("process Message");
      return true;
    }


    bool process (const ActionCommandMsg& msg) {
      for (int i = 0; i < msg.len(); i++) {
        command[commandIdx] = msg.value(i);
        commandIdx++;
      }

      if (msg.eot(MSG_END_KEY)) {
        if (commandIdx > 0 && command[0] == MSG_START_KEY) {
          DPRINT("RECV: ");
          for (int i = 0; i < commandIdx; i++) {
            DHEX(command[i]); DPRINT(" ");

          }
        }
        DPRINTLN("");

        if (commandIdx % 2 == 0) {
          for (uint8_t cidx = 1; cidx < commandIdx - 1; cidx++) {
            uint8_t ch = command[cidx];
            uint8_t val = command[cidx + 1];
            cidx++;
            DPRINT("Channel = "); DDECLN(ch);
            DPRINT("Value   = "); DDECLN(val);
            DmxParam.Channel = ch;
            DmxParam.Value = val;
            dmx.write(DmxParam.Channel, DmxParam.Value);
          }
        }

        commandIdx = 0;
        memset(command, 0, sizeof(command));
      }
      return true;
    }

    bool process (const RemoteEventMsg& msg) {
      return true;
    }

    virtual void state(uint8_t s) {
      DHEX(Channel::number());
      Button::state(s);
      RemoteEventMsg& msg = (RemoteEventMsg&)this->device().message();
      msg.init(this->device().nextcount(), this->number(), repeatcnt, (s == longreleased || s == longpressed), this->device().battery().low());
      if ( s == released || s == longreleased) {
        this->device().sendPeerEvent(msg, *this);
        repeatcnt++;
      }
      else if (s == longpressed) {
        this->device().broadcastPeerEvent(msg, *this);
      }
    }

    uint8_t state() const {
      return Button::state();
    }

    bool pressed () const {
      uint8_t s = state();
      return s == Button::pressed || s == Button::debounce || s == Button::longpressed;
    }
};

class DMXDevice : public ChannelDevice<Hal, VirtBaseChannel<Hal, DMXList0>, 2, DMXList0> {
  public:
    VirtChannel<Hal, DMXChannel, DMXList0> c1, c2;
  public:
    typedef ChannelDevice<Hal, VirtBaseChannel<Hal, DMXList0>, 2, DMXList0> DeviceType;
    DMXDevice (const DeviceInfo& info, uint16_t addr) : DeviceType(info, addr) {
      DeviceType::registerChannel(c1, 1);
      DeviceType::registerChannel(c2, 2);
    }
    virtual ~DMXDevice () {}

    DMXChannel& dmx1Channel ()  {
      return c1;
    }
    DMXChannel& dmx2Channel ()  {
      return c2;
    }
    virtual void configChanged () {

    }
};

DMXDevice sdev(devinfo, 0x20);
ConfigButton<DMXDevice> cfgBtn(sdev);

void setup () {
  dmx.init(512);
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  sdev.init(hal);
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  sdev.initDone();
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if ( worked == false && poll == false ) {
   // hal.activity.savePower<Idle<true>>(hal);
  }

  dmx.update();
}
