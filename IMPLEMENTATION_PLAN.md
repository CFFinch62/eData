# eData Implementation Plan

Read this whole document before touching code. It is written for an agent
picking up this project cold, with no memory of how it got here.

## What this project is

A standalone firmware sketch that turns an Xteink X3/X4 board (ESP32-C3,
800x480 or 792x528 1-bit e-ink) into a configurable 1/2/4-box nav instrument
display: connect to a NMEA 0183 multiplexer over Wi-Fi (UDP or TCP), and show
one page at a time of big, at-a-glance readouts - Position, SOG, COG,
Heading, Depth, Water Temp, Wind Speed, Wind Direction, Distance/Bearing to
Waypoint, Cross-Track Error, ETA and Time-To-Go - scrolled with the LEFT/RIGHT
buttons. **NAV data only** - no engine, tank or AIS data (see the sibling
`eEngine` and `eAIS` projects for those).

It was forked from `eNMEA`'s scaffold (same repo family, sibling directory):
the network transport, the versioned-NVS-blob settings pattern, the web
setup portal, the power/deep-sleep path and the raw-framebuffer drawing
primitives all came from there mostly unchanged. What's new is everything
about *what* gets drawn and *how it's configured*: the page/box layout, the
waypoint sentence parsing, and the display-layout section of the web form.
Read `eNMEA/README.md` and `eNMEA/IMPLEMENTATION_PLAN.md` first if you
haven't - this document assumes familiarity with that project's design
decisions and doesn't re-explain the ones that carried over unchanged (why no
on-device keyboard, why no CrossInk `GfxRenderer` reuse, the double-buffering
`syncWriteBufferFromActive()` requirement, etc).

## Design decisions specific to eData

**The slot model is one shared ordered list of 8 configurable data-item
slots, not three saved layouts.** Box count (1/2/4, `AppSettings::
DisplayConfig::boxCount`) decides how those 8 slots are grouped into pages -
switching box count on the web form never discards a slot's assignment.
Confirmed with the project owner during planning, along with the follow-up
concern that drove the next decision.

**Unset slots (`DataItem::None`) are skipped before grouping into pages**
(`PageLayout::buildPages`, `src/ui/PageLayout.cpp`), so page count is
`ceil(configuredSlotCount / boxCount)`, not a fixed `8/boxCount`. A user who
configures 3 items scrolls through 3 pages at box count 1, not 8. This is
the direct answer to the project owner's own question during planning
("how do I reorganize the selected data so the user doesn't have to scroll
so much") - the answer is compaction, not a smarter grouping heuristic.
Covered by `test/ui/test_page_layout.cpp`, including the uneven-count case
(a short last page with fewer than `boxCount` items).

**ETA and Time-To-Go are computed on-device, not parsed.** No standard NMEA
0183 sentence reliably carries either across real equipment. Both are
derived in `DataCatalog.cpp` from RMB's distance-to-waypoint, RMC/VTG's
speed-over-ground, and RMC's time-of-day field (`NmeaData::
timeOfDaySeconds`, added for this purpose - see below). TTG = distance/SOG;
ETA = time-of-day + TTG, wrapped mod 24h. Guard: SOG below
`MIN_SOG_FOR_ETA_KNOTS` (0.2kn) or any of the three inputs missing/stale
reports unavailable rather than a nonsense multi-hour ETA from GPS speed
noise near zero. Covered by `test/ui/test_data_catalog.cpp`, including the
near-zero-SOG guard and the midnight-wraparound arithmetic.

**RMB is the only waypoint sentence parsed.** It's the single sentence that
carries distance, bearing, XTE and a destination waypoint ID together;
BWC/BWR/WPL/RTE/XTE(the standalone sentence) would be redundant sources for
subsets of the same data from different equipment and were deliberately not
added - add one only if a real unit turns out to send RMB inconsistently.
`NmeaParser.cpp`'s RMB branch only trusts the sentence when its own status
field is `'A'` (field 1); a `'V'` (warning/no active route) sentence is
checksum-valid but leaves `NmeaData::hasWaypoint` untouched, same as any
other "sentence present but data not ready" case elsewhere in this parser.

**No sentence checklist.** Unlike eNMEA, eData's whole screen is the
instrument grid - there's no diagnostic "sentences seen" view.
`SentenceTable`/`SentenceStatus` still exist and `NmeaSource` still tallies
into them (copied unmodified from eNMEA, and removing that plumbing bought
nothing), but nothing in eData's UI reads the table. A future diagnostics
page is a display-only addition, not a plumbing change.

**AIS support was removed, not left dormant.** eNMEA's `VDM`/`VDO` decode,
`AisTargetTable`, and the AIS-payload bit-unpacking helper in
`NmeaParser.cpp` were deleted rather than carried forward unused - eAIS
already owns that concern, and AIS targets aren't in eData's nav-only data
catalog.

**"Big as possible" text is a fit-to-box search, not a fixed scale.**
`InstrumentDisplay::drawBoxValue` (via the file-local `fitScale()` helper)
tries decreasing integer font scales against `EinkCanvas::textWidth/
textHeight` until the formatted value fits the box's inner area, then draws
it centered - replacing eNMEA Dashboard's fixed scale-2 text. A two-line
value (e.g. Position's lat/lon, or Distance-to-Waypoint's "TO <id>" second
line) fits both lines to the same scale so they read as one unit, not two
different sizes.

**Box layout by count** (`InstrumentDisplay::boxRect`, computed from
`canvas_.width()/height()` at runtime like eNMEA's grid math, so the same
code fits the X3 and X4 without a per-panel branch):
- 1 box: full screen minus a thin footer status line.
- 2 boxes: stacked top/bottom halves (full width each) - more digits fit on
  one line than a left/right split would allow.
- 4 boxes: a 2x2 grid.

This is a design call made without hardware in hand (see "What's
unverified" below) - reasonable on paper, but the one thing in this project
most worth re-checking by eye on a real panel before calling it done.

**Display-layout changes don't reboot.** Unlike a Wi-Fi/profile save
(`ProvisioningPortal::handleSave`), box count and slot assignment can't
affect the network, so `handleSaveDisplay()` just persists via
`saveAppSettings()` and calls `markDisplayConfigChanged()`
(`AppSettings.h`/`.cpp`) - a plain flag `main.cpp`'s `loop()` polls once per
tick to rebuild the page list and jump back to page 0, taking effect on the
very next redraw with no ~15s restart.

## What's shared vs. new (from the eNMEA fork)

**Copied unmodified (or near enough - see `Product.h`/`platformio.ini`
naming below)**: `src/ui/EinkCanvas.{h,cpp}`, `src/ui/Font5x7.{h,cpp}`,
`src/net/NmeaSource.{h,cpp}`, `src/nmea/NmeaLineReader.{h,cpp}`,
`src/PowerControl.{h,cpp}`, `src/BoardPins.h`, `partitions.csv`, the
`setup()`/`loop()` + `handleGestures()` skeleton in `main.cpp` (POWER-hold-
2s shutdown, BACK-hold-3s forget-profile, UP/DOWN+CONFIRM profile
browsing/switching, the `WIFI_AP_STA` setup-AP-always-up pattern).

**Extended**: `src/nmea/NmeaTypes.h` (RMB/time-of-day fields added, AIS
fields removed), `src/nmea/NmeaParser.{h,cpp}` (RMB + RMC-time branches
added, VDM/AIS branch removed), `src/settings/AppSettings.{h,cpp}`
(`DisplayConfig` embedded in the stored blob, own namespace/magic - `edata`/
`"eDT1"`, independent of eNMEA's `enmea`/`"eNM1"`), `src/settings/
ProvisioningPortal.cpp` (new "Display Layout" web-form section + `/save-
display` route), `src/Product.h` (default product name), `platformio.ini`
(board-select flag renamed `EDATA_BOARD_X3`, matching `EinkCanvas.cpp`'s
`#ifdef`).

**Entirely new**: `src/ui/DataCatalog.{h,cpp}` (the `DataItem` enum, box
labels, and per-item formatting incl. ETA/TTG), `src/ui/PageLayout.{h,cpp}`
(pure compaction/grouping logic), `src/ui/InstrumentDisplay.{h,cpp}`
(replaces `Dashboard` - the whole box-grid renderer), `src/main.cpp`'s
LEFT/RIGHT page-scroll gesture and display-config-change polling, and the
RMB test-vector/waypoint-simulation additions to `scripts/
nmea_test_server.py`.

## Host-side tests

`test/run_tests.sh` - plain g++, no hardware, no PlatformIO, about a second
end to end. **250 checks, 0 failures** as of this writing, covering:

- Everything eNMEA's parser/line-reader/profile-selection tests already
  covered (unchanged behavior, carried forward as regression coverage).
- `test/nmea_parser/test_rmb.cpp`: RMB status-A vs status-V gating, XTE/
  direction fields being independently optional from distance/bearing, and
  RMC's new time-of-day extraction.
- `test/ui/test_page_layout.cpp`: page compaction/grouping across all three
  box counts, including the uneven-slot-count case central to the "avoid
  extra scrolling" design decision above.
- `test/ui/test_data_catalog.cpp`: the NO DATA AVAILABLE fallback, per-item
  staleness independence, the ETA/TTG readiness gate (all three inputs
  required), the near-zero-SOG guard, and the midnight-wraparound
  arithmetic.

**Mutation-tested** during development (the discipline eNMEA's Task 1
established - passing tests prove nothing by themselves): five deliberate
bugs were injected one at a time into the new RMB parsing, page-compaction,
and ETA/TTG logic, and all five were caught -

| Injected bug | Checks failed |
| --- | --- |
| RMB bearing read from the wrong field index | 2 |
| XTE direction never stored | 1 |
| PageLayout stops skipping `DataItem::None` | 10 |
| ETA/TTG min-SOG guard removed | 1 |
| ETA loses its 24h wraparound | 1 |

Also cross-checked end to end once: a live RMB sentence from `scripts/
nmea_test_server.py`'s waypoint simulation was fed through the real
`NmeaParser` (not just hand-written test vectors) and produced the expected
distance/bearing/XTE/waypoint-ID - confirms the test server's sentence
format and the parser's field indices actually agree, not just that each
was internally self-consistent.

Worth repeating the mutation exercise after adding cases - it's the only way
to tell a test that checks behavior from one that merely runs it.

## Tasks / what's left

### Done (this pass, no hardware)
- Full scaffold, settings/portal extensions, RMB+time parsing, page-layout
  and data-catalog logic, `InstrumentDisplay` renderer, `main.cpp` wiring,
  host-side tests (mutation-tested), test-server RMB simulation.

### Done (2026-09-03, first build)
1. **Firmware build - DONE.** `env:x3` builds clean via
   `scripts/build_web_installer.sh` (which wraps `pio run -e x3`) - a fresh
   `~/.venvs/pio` PlatformIO 6.1.19 + esptool 5.4.0 install, `freeink-sdk`
   at commit `24003795381a6c23630a26472ae3b06550333e71` (one commit past
   eNMEA's pinned `fad70f28...`; diffed first - the only change in between
   is an unrelated SD-rail fix on a different board, nothing touching
   X3/X4 or the API surface this project uses). 1.1MB image, RAM 13.1%
   (42876/327680 bytes), flash 16.9% - comfortable headroom on both. No
   warnings from project code. **`env:x4` has not been built or flashed -
   only x3 has hardware to test on.**
   `docs/firmware/eData-x3-f9d111b.bin` and `docs/manifest.json` now exist
   and are committed; the installer page at
   https://cffinch62.github.io/eData/ is live once GitHub Pages is enabled
   for this repo (`main` branch, `/docs`).

### Not done - needs a human with hardware
2. **All on-panel visual verification.** Box geometry, "big as possible"
   text fit at each of the three box counts, the footer layout, the LEFT/
   RIGHT page-scroll gesture, the NO DATA AVAILABLE fallback, and the
   display-layout web form actually round-tripping to the panel - none of
   this has been seen rendered. Compiling clean and fitting comfortably in
   flash/RAM (see above) says nothing about whether the layout looks right.
   eNMEA's own bring-up checklist is the template to follow; there's still
   no simulator (eNMEA's `IMPLEMENTATION_PLAN.md` "Simulator" section
   explains the gap and why it wasn't closed - nothing here changes that
   calculus).
3. **A real waypoint/route source, if available.** The test server's RMB
   simulation is a straight-line countdown, not a real chartplotter's
   route - if a GPS/plotter that actually emits RMB is on hand, confirm
   against it once, since equipment RMB implementations are known to vary
   (e.g. some units never populate the closing-velocity field, which eData
   doesn't use anyway, but it's worth knowing what's real-world-common vs.
   spec-only).
4. **`env:x4` build/flash**, if an X4 board turns up - only `env:x3` has
   been built and only against real X3 hardware would either env get
   flashed.

### Done (2026-09-03) - web installer
`docs/index.html`, `docs/manifest.json` and
`docs/firmware/eData-x3-f9d111b.bin` are all committed and pushed - the
first build above (Task 1) is what they package. The installer at
https://cffinch62.github.io/eData/ goes live once GitHub Pages is enabled
for this repo (`main` branch, `/docs`), which is a one-time manual step in
the repo's Settings, not something a commit can do. **The Install button
itself has not been clicked against real hardware** - everything from "no
device appears" onward in the page's troubleshooting table is inherited
reasoning from eNMEA's page, not something verified for eData specifically.
`env:x4` was not built, so there is no X4 image behind this page yet
(matches the page's own "Which devices" caveat).

### Feature polish (after 1-2 above are solid; low individual risk)
- **Fahrenheit/feet unit options** - same gap eNMEA has (Celsius/meters
  only), same fix shape (an `AppSettings` field + a formatting-site branch
  in `DataCatalog.cpp`).
- **HDG magnetic-to-true conversion** - not attempted here either, same
  reasoning as eNMEA's README: getting the deviation/variation sign
  convention wrong silently produces a plausible-but-wrong heading, worse
  than an honestly-labeled magnetic one.
- **A duplicate-profile action** on the settings page - same gap eNMEA has.

## How to verify you haven't broken anything

1. `test/run_tests.sh` after touching `src/nmea/*`, `src/ui/PageLayout.*`,
   or `src/ui/DataCatalog.*` - fast, mandatory, no excuses.
2. `pio run -e x4` (and `-e x3` if you touched anything `EDATA_BOARD_X3`-
   conditional) once `freeink-sdk` is available - **not yet done even
   once**, see Task 1 above. Don't assume it builds clean just because
   eNMEA's nearly-identical `platformio.ini` does.
3. If you touched `src/ui/InstrumentDisplay.*`: flash real hardware and
   eyeball all three box counts, a short last page (uneven slot count), and
   the NO DATA AVAILABLE fallback (easiest to trigger by leaving a source
   disconnected, or configuring a waypoint item with no RMB feed). There is
   no automated visual check and no simulator.
4. If you touched `src/settings/ProvisioningPortal.cpp`'s display-layout
   section: save a layout from the web form and confirm the panel updates
   within one redraw tick, with no reboot.
5. If you touched `src/net/*` or `src/nmea/*`: run `scripts/
   nmea_test_server.py` in both `--proto tcp` and `--proto udp` and confirm
   distance/bearing/XTE populate, and that ETA/Time-To-Go start showing once
   RMB, speed and time-of-day are all live.
6. Update this file and `README.md` if you change user-facing behavior or a
   design decision documented here, so the next agent isn't working from a
   stale list - same discipline eNMEA's own plan asks for.
