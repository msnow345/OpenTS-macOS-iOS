#pragma once
#include <windows.h>

#define STDMETHOD(m) virtual HRESULT m
#define STDMETHOD_(t, m) virtual t m
#define STDMETHODIMP HRESULT
#define STDMETHODIMP_(t) t
#define PURE = 0
#define DECLARE_INTERFACE(i) struct i
#define DECLARE_INTERFACE_(i, b) struct i : public b
#define THIS_
#define THIS
#define interface struct

struct IUnknown {
    virtual HRESULT QueryInterface(REFIID riid, void **ppv) = 0;
    virtual ULONG AddRef() = 0;
    virtual ULONG Release() = 0;
};
typedef IUnknown *LPUNKNOWN;

#define EXTERN_C extern "C"
#define STDMETHODCALLTYPE
#define STDAPICALLTYPE
#define STDAPI extern "C" HRESULT
#define STDAPI_(t) extern "C" t
#define DECLSPEC_UUID(x)
#define DECLSPEC_NOVTABLE
#define MIDL_INTERFACE(x) struct
#define EXTERN_GUID(n, ...) extern "C" const GUID n

typedef struct tagSTATSTG {
    LPWSTR pwcsName; DWORD type; ULARGE_INTEGER cbSize;
    FILETIME mtime, ctime, atime;
    DWORD grfMode, grfLocksSupported;
    CLSID clsid; DWORD grfStateBits, reserved;
} STATSTG;

struct ISequentialStream : public IUnknown {
    virtual HRESULT Read(void *pv, ULONG cb, ULONG *pcbRead) = 0;
    virtual HRESULT Write(const void *pv, ULONG cb, ULONG *pcbWritten) = 0;
};

struct IStream : public ISequentialStream {
    virtual HRESULT Seek(LARGE_INTEGER, DWORD, ULARGE_INTEGER *) = 0;
    virtual HRESULT SetSize(ULARGE_INTEGER) = 0;
    virtual HRESULT CopyTo(IStream *, ULARGE_INTEGER, ULARGE_INTEGER *, ULARGE_INTEGER *) = 0;
    virtual HRESULT Commit(DWORD) = 0;
    virtual HRESULT Revert() = 0;
    virtual HRESULT LockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) = 0;
    virtual HRESULT UnlockRegion(ULARGE_INTEGER, ULARGE_INTEGER, DWORD) = 0;
    virtual HRESULT Stat(STATSTG *, DWORD) = 0;
    virtual HRESULT Clone(IStream **) = 0;
};
typedef IStream *LPSTREAM;

typedef unsigned char boolean;

struct IPersist : public IUnknown {
    virtual HRESULT GetClassID(CLSID *pClassID) = 0;
};

struct IPersistStream : public IPersist {
    virtual HRESULT IsDirty() = 0;
    virtual HRESULT Load(IStream *pStm) = 0;
    virtual HRESULT Save(IStream *pStm, BOOL fClearDirty) = 0;
    virtual HRESULT GetSizeMax(ULARGE_INTEGER *pcbSize) = 0;
};

struct IPropertySetStorage : public IUnknown {};
struct IClassFactory : public IUnknown {
    virtual HRESULT CreateInstance(IUnknown *pUnkOuter, REFIID riid, void **ppv) = 0;
    virtual HRESULT LockServer(BOOL fLock) = 0;
};


