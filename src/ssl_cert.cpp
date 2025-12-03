#include <Arduino.h>
#include "ssl_cert.h"
#include "cert_data.h"

// Dummy PEM strings for API compatibility
// (The actual HTTPS server uses DER data from cert_data.h)
const char* server_cert = "-----BEGIN CERTIFICATE-----\nDUMMY\n-----END CERTIFICATE-----\n";
const char* server_key = "-----BEGIN PRIVATE KEY-----\nDUMMY\n-----END PRIVATE KEY-----\n";

void initSSLCertificates() {
    Serial.println("[SSL] ========================================");
    Serial.println("[SSL] SSL Certificate Information");
    Serial.println("[SSL] ========================================");
    Serial.println("[SSL] Type: Self-Signed Certificate (DER Format)");
    Serial.println("[SSL] Common Name: Server");
    Serial.println("[SSL] Organization: Espressif");
    Serial.println("[SSL] Key Size: 2048-bit RSA");
    Serial.println("[SSL] Validity: 10 years");
    Serial.printf("[SSL] Cert DER Size: %d bytes\n", server_cert_der_len);
    Serial.printf("[SSL] Key DER Size: %d bytes\n", server_key_der_len);
    Serial.println("[SSL] ========================================");
}

bool generateSelfSignedCert() {
    return true;
}
