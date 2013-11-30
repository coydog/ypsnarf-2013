ypsnarf-2013
============

ypsnarf version published in Phrack #46, updated for modern systems.

To compile on OpenIndiana (and hopefully other Solaris's), try

    cc  -o ypsnarf ypsnarf.c -lnsl -lrpcsvc

On Linux, try 

    cc -D__LINUX__  -o ypsnarf ypsnarf.c -lnsl -lrpcsvc

I've verified that RPC requests are made without the program crashing, but I'd
welcome problem reports or feedback from anyone who uses this successfully.

TODO: The BSD's. Will probably need much more work; the rpc bootparam stuff looks
stubby on the BSD's. The man pages aren't a lot of help for this stuff it seems.

CHANGES:

Old code seems to want bp_address to have a member also named bp_address. Not sure why lol.
Changed this to bp_address_u.

Linux has different types with different names for ypresp_key_val members.
Solaris used a generic "datum" type with length and pointer to data. Linux
uses different types for different members. Members are ordered differently
too. So the types are incompatible, but more or less equivalent. lol.

This program used bool, which really needs `<stdbool.h>` in C99. Added for
Linux (Sun Studio seems to have supported bool as an extension).

Also Linux uses xdrproc_t instead of xdr_proc. Look equivalent.
