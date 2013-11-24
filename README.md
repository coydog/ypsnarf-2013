ypsnarf-2013
============

ypsnarf version published in Phrack #46, updated for modern systems.

To compile on OpenIndiana (and hopefully other Solaris's), try

    cc ypsnarf -lnsl -lrpcscv

Very untested.

TODO: Linux, the BSD's. Will probably need much more work; the rpc bootparam stuff looks
stubby on the BSD's. The man pages aren't a lot of help for this stuff it seems.

CHANGES:

Old code seems to want bp_address to have a member also named bp_address. Not sure why lol.
Changed this to bp_address_u.
