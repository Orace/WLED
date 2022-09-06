#pragma once
#include "wled.h"

#define RXD2 16
#define TXD2 17

struct LevelByOffset
{
  int offset;
  uint8_t level;
};

class Staircase : public Usermod
{
private:
  LevelByOffset levelByOffsets[10] = {
      {+0, 0xFF}, {-1, 0xFF}, {-2, 0x80}, {-3, 0x40}, {-4, 0x20},
      {+1, 0xFF}, {+2, 0xFF}, {+3, 0x80}, {+4, 0x40}, {+5, 0x20}};

  bool isOnArray[42] = {false};
  bool arePinsAllocated;

public:
  void setup()
  {
    PinManagerPinType pins[2] = {{RXD2, false}, {TXD2, true}};
    arePinsAllocated = pinManager.allocateMultiplePins(pins, 2, PinOwner::UM_Staircase);

    if (arePinsAllocated)
    {
      Serial2.begin(115200, SERIAL_8N1, RXD2, TXD2);
    }
  }

  void loop()
  {
    if (!arePinsAllocated || strip.isUpdating())
    {
      return;
    }

    bool hasChanged = false;

    while (Serial2.available())
    {
      int b = Serial2.read();
      bool isOn = b & 0x80;
      int index = b & 0x7F;

      if ((index < 42) && (isOnArray[index] != isOn))
      {
        isOnArray[index] = isOn;
        hasChanged = true;
      }
    }

    if (!hasChanged)
    {
      return;
    }

    WS2812FX::Segment *const segments = strip.getSegments();

    // no segments
    if (!segments[0].isActive())
    {
      return;
    }

    // only one segment
    if (!segments[1].isActive())
    {
      bool isOn = segments[0].getOption(SEG_OPTION_ON);

      bool nextIsOn = false;
      for (int i = 0; i < 42; i++)
      {
        if (isOnArray[i])
        {
          nextIsOn = true;
          break;
        }
      }

      if (isOn == nextIsOn)
      {
        return;
      }

      segments[0].setOption(SEG_OPTION_ON, nextIsOn, 0);
      segments[0].setOption(SEG_OPTION_TRANSITIONAL, true, 0);
      colorUpdated(CALL_MODE_DIRECT_CHANGE);

      if (!nextIsOn != !bri)
        toggleOnOff();

      return;
    }

    uint8_t levelArray[14] = {0};

    for (int i = 0; i < 14; i++)
    {
      if (!(isOnArray[3 * i + 0] | isOnArray[3 * i + 1] | isOnArray[3 * i + 2]))
      {
        continue;
      }

      for (int j = 0; j < sizeof(levelByOffsets)/sizeof(*levelByOffsets); j++)
      {
        LevelByOffset levelByOffset = levelByOffsets[j];
        int index = i + levelByOffset.offset;

        if (index < 0 || index >= 14)
        {
          continue;
        }

        uint8_t level = levelByOffset.level;
        if (level > levelArray[index])
        {
          levelArray[index] = level;
        }
      }
    }

    hasChanged = false;
    bool nextIsOn = false;

    WS2812FX::Segment *segment = segments;
    for (int i = 0; i < 14 && i < MAX_NUM_SEGMENTS; i++, segment++)
    {
      if (!segment->isActive())
      {
        break;
      }

      bool isSegmentOn = segment->getOption(SEG_OPTION_ON);
      uint8_t level = levelArray[i];
      bool nextIsSegmentOn = level > 0;
      nextIsOn |= nextIsSegmentOn;

      if (isSegmentOn == nextIsSegmentOn && segment->opacity == level)
      {
        continue;
      }

      hasChanged = true;
      segment->setOption(SEG_OPTION_ON, nextIsSegmentOn, i);
      segment->setOpacity(level, i);
      segment->setOption(SEG_OPTION_TRANSITIONAL, true, i);
    }

    if (hasChanged)
    {
      colorUpdated(CALL_MODE_DIRECT_CHANGE);
      if (!nextIsOn != !bri)
        toggleOnOff();
    }
  }

  uint16_t getId()
  {
    return USERMOD_ID_STAIRCASE;
  }
};