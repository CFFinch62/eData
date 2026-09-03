#pragma once

// The product's own name, used wherever the firmware identifies itself: the
// setup page's title and heading, the access point it hosts, and the serial log
// prefix.
//
// It lives in a macro because eNMEA, eAIS and eData share the settings portal
// and network layer byte-for-byte (eData forked its copy from eNMEA's, then
// added display-layout config on top). A device flashed with eData that
// calls itself eNMEA on its own setup page is confusing in exactly the
// situation where the user is least sure what they are looking at - but
// forking those files again to fix the wording would mean maintaining
// multiple copies of code carrying hardware-found fixes. Set it per project
// in platformio.ini:
//
//     -DSETUP_PRODUCT_NAME='"eData"'
//
// String literal, so it concatenates at compile time and the page stays a
// constexpr with no runtime formatting.
#ifndef SETUP_PRODUCT_NAME
#define SETUP_PRODUCT_NAME "eData"
#endif

// Log prefix, e.g. "[eData] ". Written as a macro rather than assembled at
// runtime so format strings stay literals.
#define LOG_TAG "[" SETUP_PRODUCT_NAME "] "
