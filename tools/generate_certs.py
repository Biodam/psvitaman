import re
import base64

with open('assets/cacert.pem', 'r') as f:
    text = f.read()

pattern = re.compile(r'([^\n]+)\n={5,}\n(?:[^\n]+\n)*?-----BEGIN CERTIFICATE-----\n(.*?)\n-----END CERTIFICATE-----', re.DOTALL)

names = [
    ('cert_certainly_r1', 'Certainly Root R1'),
    ('cert_certainly_e1', 'Certainly Root E1'),
    ('cert_digicert_g2', 'DigiCert Global Root G2'),
    ('cert_digicert_g3', 'DigiCert Global Root G3'),
    ('cert_digicert_assured_g2', 'DigiCert Assured ID Root G2'),
    ('cert_digicert_assured_g3', 'DigiCert Assured ID Root G3'),
    ('cert_digicert_trusted_g4', 'DigiCert Trusted Root G4'),
    ('cert_isrg_x1', 'ISRG Root X1'),
    ('cert_isrg_x2', 'ISRG Root X2'),
]

certs_dict = {}
for match in pattern.finditer(text):
    name = match.group(1).strip()
    for var, target in names:
        if (target == name or target in name) and var not in certs_dict:
            b64 = match.group(2).replace('\n', '').strip()
            der = base64.b64decode(b64)
            certs_dict[var] = (name, der)

with open('src/root_certs.h', 'w') as f:
    f.write('''/**
 * PSVitaman - Embedded Modern Root CA Certificates
 * Generated from Mozilla CA Bundle for SceHttpsLoadCert
 */

#ifndef PSVITAMAN_ROOT_CERTS_H
#define PSVITAMAN_ROOT_CERTS_H

#include <stdbool.h>

bool root_certs_load(void);
void root_certs_unload(void);

#endif /* PSVITAMAN_ROOT_CERTS_H */
''')

with open('src/root_certs.c', 'w') as f:
    f.write('''/**
 * PSVitaman - Embedded Modern Root CA Certificates Implementation
 */

#include "root_certs.h"
#include "logger.h"
#include <stddef.h>

#if defined(__psp2__) || defined(__VITA__)
#include <psp2/net/http.h>

''')
    for var, (name, der) in certs_dict.items():
        f.write(f'/* {name} ({len(der)} bytes) */\n')
        f.write(f'static const unsigned char {var}[] = {{\n')
        for i in range(0, len(der), 12):
            chunk = der[i:i+12]
            f.write('    ' + ', '.join(f'0x{b:02x}' for b in chunk) + ',\n')
        f.write('};\n\n')

    f.write(f'''static SceHttpsData s_ca_data[{len(certs_dict)}] = {{\n''')
    for var in certs_dict.keys():
        f.write(f'    {{ (char *){var}, sizeof({var}) }},\n')
    f.write(f'''}};\n\nstatic const SceHttpsData *s_ca_ptrs[{len(certs_dict)}];\n\n''')
    f.write(f'''bool root_certs_load(void) {{\n''')
    f.write(f'''    for (int i = 0; i < {len(certs_dict)}; i++) {{\n''')
    f.write(f'''        s_ca_ptrs[i] = &s_ca_data[i];\n''')
    f.write(f'''    }}\n''')
    f.write(f'''    int res = sceHttpsLoadCert({len(certs_dict)}, s_ca_ptrs, NULL, NULL);\n''')
    f.write(f'''    if (res < 0) {{\n''')
    f.write(f'''        LOG_ERROR("root_certs_load: sceHttpsLoadCert failed: 0x%08x", res);\n''')
    f.write(f'''        return false;\n''')
    f.write(f'''    }}\n''')
    f.write(f'''    LOG_INFO("root_certs_load: registered {len(certs_dict)} Root CAs into SceHttps");\n''')
    f.write(f'''    return true;\n''')
    f.write(f'''}}\n\n''')
    f.write(f'''void root_certs_unload(void) {{\n''')
    f.write(f'''    sceHttpsUnloadCert();\n''')
    f.write(f'''}}\n\n''')
    f.write(f'''#else\n\n''')
    f.write(f'''bool root_certs_load(void) {{ return true; }}\n''')
    f.write(f'''void root_certs_unload(void) {{}}\n\n''')
    f.write(f'''#endif\n''')

print('Generated src/root_certs.h and src/root_certs.c successfully!')
