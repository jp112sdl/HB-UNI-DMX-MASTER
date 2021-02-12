//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
// 2018-11-03 jp112sdl Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------
// ci-test=yes board=328p aes=no

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#define NDEBUG

#define EI_NOTEXTERNAL
#include <EnableInterrupt.h>
#include <AskSinPP.h>
#include <LowPower.h>
#include <Register.h>
#include <Switch.h>
#include <MultiChannelDevice.h>

#define MSG_START_CHR  0x56
#define MSG_END_CHR    0x57

uint8_t G_DMX_ON_COMMAND [] = {MSG_START_CHR, 0x01, 0xff, 0x02, 0xff, 0x03, 0xff, MSG_END_CHR};
uint8_t G_DMX_OFF_COMMAND[] = {MSG_START_CHR, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, MSG_END_CHR};

#define CONFIG_BUTTON_PIN  8
#define LED_PIN            4
#define ERROR_LED_PIN      6
#define BTN_1_PIN          7
#define BTN_2_PIN          9

// number of available peers per channel
#define PEERS_PER_CHANNEL  8

struct dmxparam_t {
  byte Channel = 0;
  byte Value = 0;
} DmxParam;

class DMXDev {
#define DMXMAXCHANNELS 0xff
#define DMXSPEED       250000
#define DMXFORMAT      SERIAL_8N2
#define BREAKSPEED     83333
#define BREAKFORMAT    SERIAL_8N1
    bool dmxStarted = false;
    int sendPin = 1;
    uint8_t dmxData[DMXMAXCHANNELS] = {};
    uint16_t chanSize;
  public:
    void init(uint16_t chanQuant) {
      chanSize = chanQuant;
#ifdef NDEBUG
      Serial.begin(DMXSPEED);
      pinMode(sendPin, OUTPUT);
#endif
      dmxStarted = true;
    }
    void write(uint16_t Channel, uint8_t value) {
      if (dmxStarted == false) init(chanSize);
      if (Channel < 1) Channel = 1;
      if (Channel > chanSize) Channel = chanSize;
      //if (value < 0) value = 0;
      //if (value > 255) value = 255;
      dmxData[Channel] = value;
    }
    void update() {
      if (dmxStarted == false) init(chanSize);
      //Send break
#ifdef NDEBUG

      digitalWrite(sendPin, HIGH);
      Serial.begin(BREAKSPEED, BREAKFORMAT);
      Serial.write(0);
      Serial.flush();
      delay(1);
      Serial.end();

      //send data
      Serial.begin(DMXSPEED, DMXFORMAT);
      digitalWrite(sendPin, LOW);
      Serial.write(dmxData, chanSize);
      Serial.flush();
      delay(1);
      Serial.end();
#endif
    }
};

DMXDev dmx;

#define remISR(device,chan,pin) class device##chan##ISRHandler { \
    public: \
      static void isr () { device.remoteChannel(chan).irq(); } \
  }; \
  device.remoteChannel(chan).button().init(pin); \
  if( digitalPinToInterrupt(pin) == NOT_AN_INTERRUPT ) \
    enableInterrupt(pin,device##chan##ISRHandler::isr,CHANGE); \
  else \
    attachInterrupt(digitalPinToInterrupt(pin),device##chan##ISRHandler::isr,CHANGE);


// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
  {0xF3, 0x42, 0x01},          // Device ID
  "JPDMX00001",                // Device Serial
  {0xF3, 0x42},                // Device Model
  0x10,                        // Firmware Version
  as::DeviceType::Switch,      // Device Type
  {0x00, 0x00}                 // Info Bytes
};

/**
   Configure the used hardware
*/
typedef AskSin<StatusLed<LED_PIN>, NoBattery, Radio<AvrSPI<10, 11, 12, 13>, 2>> Hal;
typedef StatusLed<ERROR_LED_PIN> ErrorLedType;
ErrorLedType ErrorLED;
Hal hal;


DEFREGISTER(DMXReg0, MASTERID_REGS, DREG_INTKEY)
class DMXList0 : public RegList0<DMXReg0> {
  public:
    DMXList0 (uint16_t addr) : RegList0<DMXReg0>(addr) {}

    void defaults () {
      intKeyVisible(true);
      clear();
    }
};

DEFREGISTER(DMXReg1, CREG_AES_ACTIVE)
class DMXList1 : public RegList1<DMXReg1> {
  public:
    DMXList1 (uint16_t addr) : RegList1<DMXReg1>(addr) {}
    void defaults () {
      clear();
    }
};

class SwChannel : public SwitchChannel<Hal,PEERS_PER_CHANNEL,DMXList0> {
public:
  SwChannel () {};
  virtual ~SwChannel () {};

  void init () {
    BaseChannel::changed(true);
  }
  virtual void switchState(__attribute__((unused)) uint8_t oldstate,uint8_t newstate,__attribute__((unused)) uint32_t delay) {
    if ( newstate == AS_CM_JT_ON ) {
      DPRINTLN("SWITCH TURN ON");
      for (uint8_t cidx = 1; cidx < sizeof(G_DMX_ON_COMMAND) - 1; cidx++) {
        if (G_DMX_ON_COMMAND[cidx] == MSG_END_CHR)
          break;
        uint8_t ch = G_DMX_ON_COMMAND[cidx]; uint8_t val = G_DMX_ON_COMMAND[cidx + 1];
        DPRINT("Channel = "); DDECLN(ch); DPRINT("Value   = "); DDECLN(val);
        DmxParam.Channel = ch; DmxParam.Value = val;
        dmx.write(DmxParam.Channel, DmxParam.Value);
        cidx++;
       }
    }
    else if ( newstate == AS_CM_JT_OFF ) {
      DPRINTLN("SWITCH TURN OFF");
      for (uint8_t cidx = 1; cidx < sizeof(G_DMX_OFF_COMMAND) - 1; cidx++) {
        if (G_DMX_OFF_COMMAND[cidx] == MSG_END_CHR)
          break;
        uint8_t ch = G_DMX_OFF_COMMAND[cidx]; uint8_t val = G_DMX_OFF_COMMAND[cidx + 1];
        DPRINT("Channel = "); DDECLN(ch); DPRINT("Value   = "); DDECLN(val);
        DmxParam.Channel = ch; DmxParam.Value = val;
        dmx.write(DmxParam.Channel, DmxParam.Value);
        cidx++;
       }
    }
    BaseChannel::changed(true);
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

    bool process (__attribute__((unused)) const Message& msg) {
      DPRINTLN("process Message");
      return true;
    }


    bool process (const ActionCommandMsg& msg) {
      for (int i = 0; i < msg.len(); i++) {
        command[commandIdx] = msg.value(i);
        commandIdx++;
      }

      if (msg.eot(MSG_END_CHR)) {
        if (commandIdx > 0 && command[0] == MSG_START_CHR) {
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
        } else {
          ErrorLED.ledOn(seconds2ticks(3));
        }
        commandIdx = 0;
        memset(command, 0, sizeof(command));
      }
      return true;
    }

    bool process (__attribute__((unused)) const RemoteEventMsg& msg) {
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

class DMXDevice : public ChannelDevice<Hal, VirtBaseChannel<Hal, DMXList0>, 3, DMXList0> {
  public:
    VirtChannel<Hal, SwChannel,  DMXList0> c1;
    VirtChannel<Hal, DMXChannel, DMXList0> c2, c3;
  public:
    typedef ChannelDevice<Hal, VirtBaseChannel<Hal, DMXList0>, 3, DMXList0> DeviceType;
    DMXDevice (const DeviceInfo& info, uint16_t addr) : DeviceType(info, addr) {
      DeviceType::registerChannel(c1, 1);
      DeviceType::registerChannel(c2, 2);
      DeviceType::registerChannel(c3, 3);
    }
    virtual ~DMXDevice () {}
    SwChannel& switchChannel ()  {
      return c1;
    }
    DMXChannel& remoteChannel (uint8_t num)  {
      switch (num) {
        case 2:
          return c2;
          break;
        case 3:
          return c3;
          break;
        default:
          return c2;
      }
    }

    virtual void configChanged () {
    }
};

DMXDevice sdev(devinfo, 0x20);
ConfigButton<DMXDevice> cfgBtn(sdev);

void initPeerings (bool first) {
  // create internal peerings - CCU2 needs this
  if ( first == true ) {
    HMID devid;
    sdev.getDeviceID(devid);
    sdev.switchChannel().peer(Peer(devid, 2));
    sdev.remoteChannel(2).peer(Peer(devid, 1));
  }
}

void setup () {
  dmx.init(DMXMAXCHANNELS);
  DINIT(57600, ASKSIN_PLUS_PLUS_IDENTIFIER);
  bool first = sdev.init(hal);
  sdev.switchChannel().init();
  remISR(sdev, 2, BTN_1_PIN);
  remISR(sdev, 3, BTN_2_PIN);

  ErrorLED.init();
  buttonISR(cfgBtn, CONFIG_BUTTON_PIN);
  initPeerings(first);
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
