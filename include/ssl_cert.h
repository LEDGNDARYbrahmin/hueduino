#ifndef SSL_CERT_H
#define SSL_CERT_H

#include <Arduino.h>

// Self-signed certificate for HTTPS server
// This certificate is valid for 10 years from generation
// Common Name: hue-bridge.local
// Organization: diyHue
// Valid from: 2024-01-01 to 2034-01-01

// Server certificate (PEM format)
extern const char* server_cert;

// Server private key (PEM format)
extern const char* server_key;

// Initialize SSL certificates
void initSSLCertificates();

// Generate new self-signed certificate (optional, for future use)
bool generateSelfSignedCert();

#endif // SSL_CERT_H
