/*
 * w32reg.h:  Winvile OLE registry data (currently applicable only
 *            to OLE Automation).
 *
 * $Id: w32reg.h,v 1.2 1998/08/15 20:42:00 tom Exp $
 */

#ifndef W32REG_H
#define W32REG_H

/* VTL -- (Vile Type Library) version numbering macros */
#define  VTL_MAJOR    1
#define  VTL_MINOR    0
#define  VTL_VERSTR   VTL_VER_TO_STR(VTL_MAJOR) "." VTL_VER_TO_STR(VTL_MINOR)

#define  VTL_VER_TO_STR(x)    VTL_TMP_MAC(x)
#define  VTL_TMP_MAC(x)       #x

/* {664D8543-2FDD-11d2-B8E4-0020AF0F4354} */
#define LIBID_VILEAUTO_KEY   "{664D8543-2FDD-11d2-B8E4-0020AF0F4354}"
DEFINE_GUID(LIBID_VileAuto,
0x664d8543, 0x2fdd, 0x11d2, 0xb8, 0xe4, 0x0, 0x20, 0xaf, 0xf, 0x43, 0x54);

/* {664D8544-2FDD-11d2-B8E4-0020AF0F4354} */
#define IID_IVILEAUTO_KEY    "{664D8544-2FDD-11d2-B8E4-0020AF0F4354}"
DEFINE_GUID(IID_IVileAuto,
0x664d8544, 0x2fdd, 0x11d2, 0xb8, 0xe4, 0x0, 0x20, 0xaf, 0xf, 0x43, 0x54);


/* {664D8545-2FDD-11d2-B8E4-0020AF0F4354} */
#define CLSID_VILEAUTO_KEY   "{664D8545-2FDD-11d2-B8E4-0020AF0F4354}"
DEFINE_GUID(CLSID_VileAuto,
0x664d8545, 0x2fdd, 0x11d2, 0xb8, 0xe4, 0x0, 0x20, 0xaf, 0xf, 0x43, 0x54);


/* {00020424-0000-0000-C000-000000000046} */
#define CLSID_PROXY_STUB     "{00020424-0000-0000-C000-000000000046}"

#endif
