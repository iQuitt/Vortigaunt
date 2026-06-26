#pragma once

#include <windows.h>
#include <shobjidl.h>
#include <thumbcache.h>


extern const CLSID CLSID_DtxThumbnailProvider;

class CDtxThumbnailProvider : public IInitializeWithStream, public IInitializeWithFile, public IThumbnailProvider
{
public:
    CDtxThumbnailProvider();
    virtual ~CDtxThumbnailProvider();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

    // IInitializeWithStream
    IFACEMETHODIMP Initialize(IStream* pStream, DWORD grfMode);

    // IInitializeWithFile
    IFACEMETHODIMP Initialize(LPCWSTR pszFilePath, DWORD grfMode);

    // IThumbnailProvider
    IFACEMETHODIMP GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha);

private:
    long m_cRef;
    IStream* m_pStream;
    wchar_t m_szFilePath[MAX_PATH];
};

class CDtxThumbnailProviderFactory : public IClassFactory
{
public:
    CDtxThumbnailProviderFactory();
    virtual ~CDtxThumbnailProviderFactory();

    // IUnknown
    IFACEMETHODIMP QueryInterface(REFIID riid, void** ppv);
    IFACEMETHODIMP_(ULONG) AddRef();
    IFACEMETHODIMP_(ULONG) Release();

    // IClassFactory
    IFACEMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv);
    IFACEMETHODIMP LockServer(BOOL fLock);

private:
    long m_cRef;
};
