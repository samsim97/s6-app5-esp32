#pragma once

// Blocks until a DHCP address is obtained (retries automatically on disconnect)
void wifi_connect(const char *ssid, const char *password);
