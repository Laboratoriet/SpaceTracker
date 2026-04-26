#pragma once

// U8g2 fonts — clean, anti-aliased, many sizes
// Only the fonts we actually reference get linked into the binary
#include <U8g2lib.h>

// Custom Inter Black 92px font (digits 0-9 + colon) — generated via otf2bdf + bdfconv
// from official Inter v4.0 Inter-Black.otf. ~1.3KB in flash. See preview/index.html
// for the matching Inter Black @ 92px in the design sketches.
#include "font_inter_black_92.h"

// ── Font aliases — Inter family throughout ──

// Big crew count number / clock time — Inter Black 92px (custom-generated)
#define FONT_BIG_NUM    font_inter_black_92

// Title text — 24px Inter Bold (e.g. "SPACE TRACKER")
#define FONT_TITLE      u8g2_font_inb24_mr

// Subtitle / medium text — 16px Inter Regular (smaller, understated)
#define FONT_SUBTITLE   u8g2_font_inr16_mr

// Headers — 19px Inter Bold (e.g. craft names "ISS", "Tiangong")
#define FONT_HEADER     u8g2_font_inb19_mr

// Body text — 16px Inter Regular (e.g. crew names)
#define FONT_BODY       u8g2_font_inr16_mr

// Body text bold variant — 16px Inter Bold (same size as FONT_BODY for
// in-paragraph emphasis without changing line height)
#define FONT_BODY_BOLD  u8g2_font_inb16_mr

// Small text — 16px Inter Regular (orbit labels)
#define FONT_SMALL      u8g2_font_inr16_mr

// Tiny text — for status bar with coordinates
#define FONT_TINY       u8g2_font_helvR10_tr
