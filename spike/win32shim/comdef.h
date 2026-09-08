#pragma once
#include <unknwn.h>

// _com_ptr_t and the __declspec(uuid)/__uuidof machinery have no clang equivalent on a
// non-MSVC target. Declaring the template unconditionally lets the surface be counted.
template <class T> class _com_ptr_t {
public:
    _com_ptr_t() = default;
    T *operator->() const;
    operator T *() const;
    T **operator&();
};
#define _COM_SMARTPTR_TYPEDEF(iface, iid) typedef _com_ptr_t<iface> iface##Ptr
class _com_error {
public:
    HRESULT Error() const;
    const char *ErrorMessage() const;
};

_COM_SMARTPTR_TYPEDEF(IUnknown, 0);
_COM_SMARTPTR_TYPEDEF(IStream, 0);
_COM_SMARTPTR_TYPEDEF(IPersistStream, 0);
_COM_SMARTPTR_TYPEDEF(IPropertySetStorage, 0);
_COM_SMARTPTR_TYPEDEF(IClassFactory, 0);
