#include "DtxThumbnailProvider.h"
#include "core/converters/DtxConverter.h"
#include <shlobj.h> // SHChangeNotify
#include <shlwapi.h>
#include <string>
#include <vector>

#pragma comment(lib, "shlwapi.lib")

// CLSID: {C4066FE0-59CE-452D-B9B2-E7B6C8A604EA}
const CLSID CLSID_DtxThumbnailProvider = { 0xc4066fe0, 0x59ce, 0x452d, { 0xb9, 0xb2, 0xe7, 0xb6, 0xc8, 0xa6, 0x4, 0xea } };

static HINSTANCE g_hInst = NULL;
static long g_cDllRefs = 0;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        g_hInst = hModule;
        DisableThreadLibraryCalls(hModule);
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


CDtxThumbnailProvider::CDtxThumbnailProvider() : m_cRef(1), m_pStream(nullptr)
{
    m_szFilePath[0] = L'\0';
    InterlockedIncrement(&g_cDllRefs);
}

CDtxThumbnailProvider::~CDtxThumbnailProvider()
{
    if (m_pStream)
    {
        m_pStream->Release();
    }
    InterlockedDecrement(&g_cDllRefs);
}

// IUnknown
IFACEMETHODIMP CDtxThumbnailProvider::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] = {
        QITABENT(CDtxThumbnailProvider, IInitializeWithStream),
        QITABENT(CDtxThumbnailProvider, IInitializeWithFile),
        QITABENT(CDtxThumbnailProvider, IThumbnailProvider),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) CDtxThumbnailProvider::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) CDtxThumbnailProvider::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

// IInitializeWithStream
IFACEMETHODIMP CDtxThumbnailProvider::Initialize(IStream* pStream, DWORD grfMode)
{
    if (pStream == NULL)
        return E_INVALIDARG;

    if (m_pStream)
    {
        m_pStream->Release();
        m_pStream = nullptr;
    }

    m_pStream = pStream;
    m_pStream->AddRef();
    return S_OK;
}

// IInitializeWithFile
IFACEMETHODIMP CDtxThumbnailProvider::Initialize(LPCWSTR pszFilePath, DWORD grfMode)
{
    if (pszFilePath == NULL)
        return E_INVALIDARG;

    wcscpy_s(m_szFilePath, ARRAYSIZE(m_szFilePath), pszFilePath);
    return S_OK;
}

// IThumbnailProvider
IFACEMETHODIMP CDtxThumbnailProvider::GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha)
{
    if (phbmp == NULL || pdwAlpha == NULL)
        return E_INVALIDARG;

    *phbmp = NULL;
    *pdwAlpha = WTSAT_UNKNOWN;

    DtxConverter converter;
    std::vector<unsigned int> pixels;
    int w = 0, h = 0;
    bool decodeSuccess = false;

    if (m_pStream != nullptr)
    {
        // Seek to the beginning of the stream
        LARGE_INTEGER liZero = { 0 };
        m_pStream->Seek(liZero, STREAM_SEEK_SET, NULL);

        std::vector<uint8_t> buffer;
        BYTE temp[4096];
        ULONG cbRead = 0;
        while (SUCCEEDED(m_pStream->Read(temp, sizeof(temp), &cbRead)) && cbRead > 0)
        {
            buffer.insert(buffer.end(), temp, temp + cbRead);
        }

        if (!buffer.empty())
        {
            decodeSuccess = converter.DecodeDTXBufferToRGBA(buffer.data(), buffer.size(), pixels, w, h);
        }
    }
    else if (m_szFilePath[0] != L'\0')
    {
        int size_needed = WideCharToMultiByte(CP_UTF8, 0, m_szFilePath, -1, NULL, 0, NULL, NULL);
        if (size_needed > 0)
        {
            std::string filePath(size_needed, 0);
            WideCharToMultiByte(CP_UTF8, 0, m_szFilePath, -1, &filePath[0], size_needed, NULL, NULL);
            if (!filePath.empty() && filePath.back() == '\0')
            {
                filePath.pop_back();
            }
            decodeSuccess = converter.DecodeDTXToRGBA(filePath, pixels, w, h);
        }
    }

    if (!decodeSuccess)
    {
        return E_FAIL;
    }

    if (w <= 0 || h <= 0 || pixels.empty())
    {
        return E_FAIL;
    }

    BITMAPINFO bmi = { 0 };
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h; 
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* pBits = nullptr;
    HBITMAP hbmp = CreateDIBSection(NULL, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    if (!hbmp)
    {
        return E_FAIL;
    }

    // Copy decoded pixels into DIB section
    memcpy(pBits, pixels.data(), w * h * sizeof(unsigned int));

    *phbmp = hbmp;
    *pdwAlpha = WTSAT_ARGB; // Use alpha channel transparency

    return S_OK;
}



CDtxThumbnailProviderFactory::CDtxThumbnailProviderFactory() : m_cRef(1)
{
    InterlockedIncrement(&g_cDllRefs);
}

CDtxThumbnailProviderFactory::~CDtxThumbnailProviderFactory()
{
    InterlockedDecrement(&g_cDllRefs);
}

// IUnknown
IFACEMETHODIMP CDtxThumbnailProviderFactory::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] = {
        QITABENT(CDtxThumbnailProviderFactory, IClassFactory),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) CDtxThumbnailProviderFactory::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) CDtxThumbnailProviderFactory::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

// IClassFactory
IFACEMETHODIMP CDtxThumbnailProviderFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv)
{
    if (pUnkOuter != NULL)
        return CLASS_E_NOAGGREGATION;

    CDtxThumbnailProvider* pProv = new (std::nothrow) CDtxThumbnailProvider();
    if (!pProv)
        return E_OUTOFMEMORY;

    HRESULT hr = pProv->QueryInterface(riid, ppv);
    pProv->Release();
    return hr;
}

IFACEMETHODIMP CDtxThumbnailProviderFactory::LockServer(BOOL fLock)
{
    if (fLock)
    {
        InterlockedIncrement(&g_cDllRefs);
    }
    else
    {
        InterlockedDecrement(&g_cDllRefs);
    }
    return S_OK;
}


STDAPI DllCanUnloadNow()
{
    return (g_cDllRefs == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv)
{
    if (ppv == NULL)
        return E_INVALIDARG;

    *ppv = NULL;

    if (IsEqualCLSID(rclsid, CLSID_DtxThumbnailProvider))
    {
        CDtxThumbnailProviderFactory* pFactory = new (std::nothrow) CDtxThumbnailProviderFactory();
        if (!pFactory)
            return E_OUTOFMEMORY;

        HRESULT hr = pFactory->QueryInterface(riid, ppv);
        pFactory->Release();
        return hr;
    }

    return CLASS_E_CLASSNOTAVAILABLE;
}

static HRESULT CreateRegistryKey(HKEY hKeyParent, LPCWSTR pszSubKey, LPCWSTR pszValueName, LPCWSTR pszValue)
{
    HKEY hKey = NULL;
    LSTATUS status = RegCreateKeyExW(hKeyParent, pszSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (status != ERROR_SUCCESS)
    {
        return HRESULT_FROM_WIN32(status);
    }
    if (pszValue != NULL)
    {
        status = RegSetValueExW(hKey, pszValueName, 0, REG_SZ, (BYTE*)pszValue, (DWORD)(wcslen(pszValue) + 1) * sizeof(wchar_t));
    }
    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(status);
}

static HRESULT CreateRegistryDwordValue(HKEY hKeyParent, LPCWSTR pszSubKey, LPCWSTR pszValueName, DWORD dwValue)
{
    HKEY hKey = NULL;
    LSTATUS status = RegCreateKeyExW(hKeyParent, pszSubKey, 0, NULL, REG_OPTION_NON_VOLATILE, KEY_WRITE, NULL, &hKey, NULL);
    if (status != ERROR_SUCCESS)
    {
        return HRESULT_FROM_WIN32(status);
    }
    status = RegSetValueExW(hKey, pszValueName, 0, REG_DWORD, (BYTE*)&dwValue, sizeof(dwValue));
    RegCloseKey(hKey);
    return HRESULT_FROM_WIN32(status);
}

STDAPI DllRegisterServer()
{
    wchar_t szModule[MAX_PATH];
    if (GetModuleFileNameW(g_hInst, szModule, ARRAYSIZE(szModule)) == 0)
    {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    // Register CLSID under HKCU to avoid requiring Administrator privileges
    HRESULT hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{C4066FE0-59CE-452D-B9B2-E7B6C8A604EA}", NULL, L"Vortigaunt DTX Thumbnail Provider");
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{C4066FE0-59CE-452D-B9B2-E7B6C8A604EA}\\InprocServer32", NULL, szModule);
    }
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{C4066FE0-59CE-452D-B9B2-E7B6C8A604EA}\\InprocServer32", L"ThreadingModel", L"Apartment");
    }
    if (SUCCEEDED(hr))
    {
        // Disable process isolation to run in-process or avoid stream-only sandbox blockages
        hr = CreateRegistryDwordValue(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{C4066FE0-59CE-452D-B9B2-E7B6C8A604EA}", L"DisableProcessIsolation", 1);
    }

    // Register file association under HKCU
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\.dtx\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", NULL, L"{C4066FE0-59CE-452D-B9B2-E7B6C8A604EA}");
    }
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\.dtx", L"PerceivedType", L"image");
    }

    // Register uppercase as well for compatibility
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\.DTX\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", NULL, L"{C4066FE0-59CE-452D-B9B2-E7B6C8A604EA}");
    }
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\.DTX", L"PerceivedType", L"image");
    }

    // Notify shell that association has changed
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return hr;
}

STDAPI DllUnregisterServer()
{
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\.dtx\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\.DTX\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}");

    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{C4066FE0-59CE-452D-B9B2-E7B6C8A604EA}\\InprocServer32");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{C4066FE0-59CE-452D-B9B2-E7B6C8A604EA}");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return S_OK;
}
