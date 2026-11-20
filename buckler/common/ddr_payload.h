// The Bluetooth packet the robots broadcast to each other, shared by the driving robot's
// Buckler app (coords) and the receiving Buckler app (receiver), so they cannot disagree.
//
// It travels inside the "manufacturer specific data" field of a Bluetooth Low Energy
// advertisement: no pairing, no connection. The three tag bytes let us ignore every other
// device's advertisements. 11 bytes of the 24 that fit.
#pragma once

#include <stdint.h>
#include <string.h>

#define DDR_MANUFACTURER_ID 0x02E0  // the id the simple_ble library puts in front of the data

typedef struct __attribute__((packed)) {
  uint8_t tag[3];  // 'D', 'D', 'R'
  uint8_t id;      // which robot sent it
  int16_t x;       // mm on the shared map
  int16_t y;       // mm on the shared map
  int16_t h;       // heading in degrees
  uint8_t seq;     // counts up by one per broadcast (to spot lost packets while debugging)
} ddr_payload_t;

// Looks through one advertisement for a DDR packet. An advertisement is a list of fields,
// each [length][type][data...]; ours is the manufacturer specific one (type 0xFF) with the
// id above followed by a ddr_payload_t. Returns 1 and fills *out if one is found, else 0.
// Safe on malformed data: it never reads past 'len'.
static inline int ddr_parse_adv(const uint8_t* d, uint16_t len, ddr_payload_t* out) {
  uint16_t i = 0;
  while (i + 1 < len) {
    uint8_t field_len = d[i];  // bytes after this one: the type byte plus the data
    if (field_len == 0 || (uint16_t)(i + field_len) >= len) {
      return 0;  // malformed or truncated
    }
    uint8_t type = d[i + 1];
    if (type == 0xFF && field_len >= 1 + 2 + sizeof(ddr_payload_t)) {
      uint16_t company = (uint16_t)(d[i + 2] | (d[i + 3] << 8));
      if (company == DDR_MANUFACTURER_ID) {
        ddr_payload_t p;
        memcpy(&p, &d[i + 4], sizeof p);
        if (p.tag[0] == 'D' && p.tag[1] == 'D' && p.tag[2] == 'R') {
          *out = p;
          return 1;
        }
      }
    }
    i += (uint16_t)(field_len + 1);
  }
  return 0;
}
