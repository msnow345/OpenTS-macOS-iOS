# Win32 measurement shim

Throwaway scaffolding for the native arm64 portability spike. The headers here
declare only the types and macros the tree needs to get past `#include`, so a
native compile reports the Win32 API surface as undeclared identifiers instead
of stopping at the first missing header. Nothing here implements Win32, and
nothing here is a portability layer. It exists to size the port.
