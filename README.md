# eData

Configurable 1/2/4-box nav instrument display, running on Xteink X3/X4
hardware (ESP32-C3; 800x480 e-ink on the X4, 792x528 on the X3) as a bare
dev-kit target. Fed by NMEA 0183 over Wi-Fi (UDP or TCP) - the same
transport layer `eNMEA` uses, which this project was forked from.

**NAV data only.** No engine, tank, or AIS data - see the sibling `eEngine`
and `eAIS` projects for those.

**Hardware status: verified on an X3, all three box counts.** Flashed and
tested on real hardware 2026-09-03 - 1, 2, and 4 boxes per screen all
confirmed working. `env:x4` has not been built or tested (no X4 hardware
available) - see `IMPLEMENTATION_PLAN.md`'s "Not done" section for what's
still outstanding there.

## What it does

- Connects to a Wi-Fi network you configure, the same way `eNMEA` does.
- Listens for NMEA 0183 over UDP (broadcast) or connects out to a TCP NMEA
  server - see `eNMEA/README.md`'s "UDP vs TCP" section, which applies here
  unchanged.
- Shows **1, 2, or 4 data boxes at a time**, as big as the panel allows.
  Pick the box count on the web setup page; the panel shows readouts from a
  catalog of 13 nav items:

  Position, Speed (SOG), Course (COG), Heading, Depth, Water Temp, Wind
  Speed, Wind Direction, Distance to Waypoint, Bearing to Waypoint,
  Cross-Track Error, ETA, and Time to Go.

- **You configure up to 8 items, in order; the box count decides how they're
  grouped into pages** - not three separate layouts to maintain. At 1 box
  per screen, each configured item gets its own full-screen page; at 2
  boxes, they're paired two-per-page (stacked top/bottom); at 4, they're
  grouped four-per-page (a 2x2 grid). **Unused slots are skipped entirely**,
  so configuring only 3 items means scrolling through 3 pages at 1 box per
  screen, not 8 - the page count always tracks what you've actually
  configured.
- Scroll between pages with the **LEFT/RIGHT** buttons on the device.
- A data item with nothing feeding it (no source configured, or that
  sentence never arrives) shows **NO DATA AVAILABLE** rather than a blank
  box or a stale-looking number.
- ETA and Time-To-Go aren't things a NMEA 0183 sentence reliably carries -
  eData computes both itself from Distance-to-Waypoint and Speed-Over-
  Ground, the way a chartplotter does. Both need a waypoint (RMB), a speed,
  and a time-of-day all currently live; below about 0.2kn of boat speed they
  show NO DATA AVAILABLE rather than a wild multi-hour estimate from GPS
  speed noise near zero.
- Stores up to 8 named Wi-Fi/source profiles and switches between them from
  the buttons (UP/DOWN to pick, CONFIRM to apply) - identical to eNMEA,
  useful if you move this device between boats or test benches.
- Shuts down on a 2-second hold of the power button, and forgets the active
  Wi-Fi profile on a 3-second hold of BACK - see eNMEA's README for the
  exact mechanics, unchanged here.
- All settings - Wi-Fi, NMEA source, **and the display layout** - are
  entered through the same small web form eNMEA uses for its settings, with
  a new "Display Layout" section added. See "Setup flow" below.

## Setup flow

Identical to eNMEA's (see that project's README for the full walkthrough and
screenshots) with one addition: the settings page now also has a **Display
Layout** section - a box-count selector and 8 slot dropdowns, each offering
the 13-item catalog above plus "Unused". Saving the layout **does not
reboot the device** (unlike a Wi-Fi/profile change) - it takes effect on the
very next screen redraw, typically within 2 seconds.

1. First boot with no saved settings: the panel shows SETUP MODE and hosts a
   `eData-Setup` Wi-Fi access point.
2. Join that AP, browse to `http://192.168.7.1/`.
3. Fill in Wi-Fi + NMEA source (same fields as eNMEA), and the Display
   Layout section. Save.
4. The device reconnects (a Wi-Fi/source change reboots; a display-layout-
   only change does not) and starts the instrument display.
5. The settings page stays reachable both at `http://<device-ip>/` on your
   LAN and at `http://192.168.7.1/` over the always-up setup AP - the AP is
   what matters when the saved Wi-Fi config is wrong for wherever the
   device currently is.

## Build & flash

```sh
pio run -e x4 -t upload    # X4 / X4 Pro panel (default env name; x3 is this project's default_envs)
pio run -e x3 -t upload    # X3 panel
pio device monitor -e x4
```

Needs `freeink-sdk` cloned as a sibling directory first - see eNMEA's
README "Getting freeink-sdk" section for the exact commands and why the
pinned commit matters. **`env:x3` builds clean and is hardware-verified**
(2026-09-03, commit `24003795...`, 1.1MB image, RAM 13.1% / flash 16.9%
used - flashed to a real X3, all three box counts confirmed working).
**`env:x4` has not been built or tested** - an X4 is on order, expected
around 2026-09-10; until then treat X4 support as unverified even though
`EinkCanvas`'s runtime `width()/height()` accessors mean the same binary
logic should apply to both panels.

## Installing without a toolchain (for end users)

### [→ Install eData from your browser](https://cffinch62.github.io/eData/)

Same [ESP Web Tools](https://esphome.github.io/esp-web-tools/)-over-GitHub-
Pages pattern eNMEA and eAIS use. `docs/firmware/eData-x3-<version>.bin` +
`docs/manifest.json` are committed and the X3 build behind them is
hardware-verified (see "Hardware status" above). If the hosted link 404s,
GitHub Pages likely still needs enabling for this repo (Settings → Pages →
deploy from `main` branch, `/docs` folder) - a one-time manual step, not
something a commit can do.

Rebuild after a firmware change:

```sh
scripts/build_web_installer.sh   # builds env:x3, merges, regenerates the manifest
git add docs && git commit && git push
```

Only `env:x3` has an image behind this installer - `env:x4` will follow
once the on-order X4 board arrives.

## Tests

```sh
test/run_tests.sh
```

Host-side tests for the NMEA parsing/framing layer, profile selection, and
the display's pure-logic layer (page compaction, per-item formatting and
ETA/TTG math) - plain g++, no hardware, no PlatformIO, about a second to
run. 250 checks as of this writing. See `IMPLEMENTATION_PLAN.md` for what's
covered and the mutation-testing results proving the new RMB/page-layout/
ETA-TTG tests actually catch regressions, not just run.

Try it against a live feed with the bundled test server:

```sh
python3 scripts/nmea_test_server.py                # TCP - device dials in
python3 scripts/nmea_test_server.py --proto udp    # UDP broadcast
```

It emits the same NAV sentences eNMEA's test server does, plus a simulated
RMB (a fixed waypoint, distance counting down over time, cross-track error
oscillating across zero) so Distance/Bearing/XTE/ETA/Time-To-Go all have
something live to show.

## Design decisions worth knowing about

See `IMPLEMENTATION_PLAN.md`'s "Design decisions specific to eData" section
for the full reasoning (the shared-8-slot-list model, why unused slots
compact instead of leaving blank pages, why ETA/TTG are computed instead of
parsed, why RMB is the only waypoint sentence supported, and the box-layout
choices for 1/2/4 boxes). Everything eNMEA's own "Design decisions" section
covers (no CrossInk `GfxRenderer` reuse, no on-device text entry, the
double-buffering sync requirement, the setup-AP-always-up choice) applies
here unchanged - that section is not repeated in this README.

## What's still rough (known gaps, not hidden)

- **`env:x4` is unbuilt and untested** - no X4 hardware was available when
  this was written; one is on order (expected ~2026-09-10). The X3 build is
  hardware-verified across all three box counts.
- No Fahrenheit option for water temp, no feet option for depth - same gap
  as eNMEA, same reason (v1 scope).
- `HDG` heading is shown as magnetic, not corrected to true - same
  reasoning as eNMEA: guessing the deviation/variation sign wrong would be
  worse than an honestly-labeled magnetic reading.
- Only RMB is parsed for waypoint data; BWC/BWR/WPL/RTE/standalone XTE are
  not, on the theory that real equipment sending a route sends RMB. Revisit
  if a real unit turns out not to.
- The waypoint/ETA/TTG fields have only been exercised against the bundled
  test server's simulated RMB, not a real chartplotter's route - see
  `IMPLEMENTATION_PLAN.md`.

## License

MIT - see `LICENSE`. Built on the same two references as eNMEA: `freeink-sdk`
(the hardware libraries this links against) and CrossInk (reference only;
nothing here depends on it). Forked from eNMEA's own source, also MIT.
