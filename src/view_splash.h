#pragma once

// Boot-time splash. `status` is shown as a small subtitle (e.g. "Connecting…",
// "Fetching data…"). Pass nullptr or "" to omit.
void view_splash_render(const char* status);
