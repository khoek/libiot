#pragma once

/*
    The certficate hierachy works like this (each bullet point is a
   certificate):
        * hoek.io Root CA
            * hoek.io IOT Device Authority
                * IOT Device (node-red)
                * IOT Device (buzzer)
                * IOT Device (powermeter)
                * <etc.>
            * hoek.io Endpoint Authority
                * storagebox.local (MQTT broker)

    The endpoint authority certficate and root certificate form a chain encoded
   in the string `LIBIOT_CERT_AUTHORITY_ENDPOINT` below.
*/

extern const char *LIBIOT_CERT_AUTHORITY_ENDPOINT;
