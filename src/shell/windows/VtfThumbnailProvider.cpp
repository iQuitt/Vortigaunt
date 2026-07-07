#include "VtfThumbnailProvider.h"

#include "VTFLib.h"

#include <shlobj.h> // SHChangeNotify
#include <shlwapi.h>
#include <string>
#include <vector>
#include <fstream>

#pragma comment(lib, "shlwapi.lib")

// CLSID: {8D2F7A31-4B6C-4E95-9A47-C1E52D80B6F3}
const CLSID CLSID_VtfThumbnailProvider = { 0x8d2f7a31, 0x4b6c, 0x4e95, { 0x9a, 0x47, 0xc1, 0xe5, 0x2d, 0x80, 0xb6, 0xf3 } };

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

static bool DecodeVtfToDibPixels(const uint8_t* data, size_t size, UINT cx,
                                 std::vector<unsigned int>& pixels, int& w, int& h)
{
    VTFLib::CVTFFile vtf;
    if (!vtf.Load((const vlVoid*)data, (vlUInt)size))
        return false;

    const vlUInt uiMipCount = vtf.GetMipmapCount();
    vlUInt uiMip = 0;
    if (cx > 0)
    {
        while (uiMip + 1 < uiMipCount &&
               (vtf.GetWidth() >> (uiMip + 1)) >= cx &&
               (vtf.GetHeight() >> (uiMip + 1)) >= cx)
        {
            ++uiMip;
        }
    }

    vlUInt uiWidth = 0, uiHeight = 0, uiDepth = 0;
    VTFLib::CVTFFile::ComputeMipmapDimensions(
        vtf.GetWidth(), vtf.GetHeight(), vtf.GetDepth(),
        uiMip, uiWidth, uiHeight, uiDepth);

    if (uiWidth == 0 || uiHeight == 0)
        return false;

    vlByte* lpData = vtf.GetData(0, 0, 0, uiMip);
    if (lpData == nullptr)
        return false;

    std::vector<vlByte> rgba(static_cast<size_t>(uiWidth) * uiHeight * 4);
    if (!VTFLib::CVTFFile::ConvertToRGBA8888(lpData, rgba.data(), uiWidth, uiHeight, vtf.GetFormat()))
        return false;

    // RGBA -> BGRA (DIB byte order)
    pixels.resize(static_cast<size_t>(uiWidth) * uiHeight);
    for (size_t i = 0; i < pixels.size(); ++i)
    {
        const vlByte r = rgba[i * 4 + 0];
        const vlByte g = rgba[i * 4 + 1];
        const vlByte b = rgba[i * 4 + 2];
        const vlByte a = rgba[i * 4 + 3];
        pixels[i] = (static_cast<unsigned int>(a) << 24) |
                    (static_cast<unsigned int>(r) << 16) |
                    (static_cast<unsigned int>(g) << 8) |
                    static_cast<unsigned int>(b);
    }

    w = static_cast<int>(uiWidth);
    h = static_cast<int>(uiHeight);
    return true;
}


CVtfThumbnailProvider::CVtfThumbnailProvider() : m_cRef(1), m_pStream(nullptr)
{
    m_szFilePath[0] = L'\0';
    InterlockedIncrement(&g_cDllRefs);
}

CVtfThumbnailProvider::~CVtfThumbnailProvider()
{
    if (m_pStream)
    {
        m_pStream->Release();
    }
    InterlockedDecrement(&g_cDllRefs);
}

// IUnknown
IFACEMETHODIMP CVtfThumbnailProvider::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] = {
        QITABENT(CVtfThumbnailProvider, IInitializeWithStream),
        QITABENT(CVtfThumbnailProvider, IInitializeWithFile),
        QITABENT(CVtfThumbnailProvider, IThumbnailProvider),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) CVtfThumbnailProvider::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) CVtfThumbnailProvider::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

// IInitializeWithStream
IFACEMETHODIMP CVtfThumbnailProvider::Initialize(IStream* pStream, DWORD grfMode)
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
IFACEMETHODIMP CVtfThumbnailProvider::Initialize(LPCWSTR pszFilePath, DWORD grfMode)
{
    if (pszFilePath == NULL)
        return E_INVALIDARG;

    wcscpy_s(m_szFilePath, ARRAYSIZE(m_szFilePath), pszFilePath);
    return S_OK;
}

// IThumbnailProvider
IFACEMETHODIMP CVtfThumbnailProvider::GetThumbnail(UINT cx, HBITMAP* phbmp, WTS_ALPHATYPE* pdwAlpha)
{
    if (phbmp == NULL || pdwAlpha == NULL)
        return E_INVALIDARG;

    *phbmp = NULL;
    *pdwAlpha = WTSAT_UNKNOWN;

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
            decodeSuccess = DecodeVtfToDibPixels(buffer.data(), buffer.size(), cx, pixels, w, h);
        }
    }
    else if (m_szFilePath[0] != L'\0')
    {
        std::ifstream file(m_szFilePath, std::ios::binary);
        if (file.is_open())
        {
            std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
                                        std::istreambuf_iterator<char>());
            if (!buffer.empty())
            {
                decodeSuccess = DecodeVtfToDibPixels(buffer.data(), buffer.size(), cx, pixels, w, h);
            }
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
    memcpy(pBits, pixels.data(), static_cast<size_t>(w) * h * sizeof(unsigned int));

    *phbmp = hbmp;
    *pdwAlpha = WTSAT_ARGB; // Use alpha channel transparency

    return S_OK;
}



CVtfThumbnailProviderFactory::CVtfThumbnailProviderFactory() : m_cRef(1)
{
    InterlockedIncrement(&g_cDllRefs);
}

CVtfThumbnailProviderFactory::~CVtfThumbnailProviderFactory()
{
    InterlockedDecrement(&g_cDllRefs);
}

// IUnknown
IFACEMETHODIMP CVtfThumbnailProviderFactory::QueryInterface(REFIID riid, void** ppv)
{
    static const QITAB qit[] = {
        QITABENT(CVtfThumbnailProviderFactory, IClassFactory),
        { 0 }
    };
    return QISearch(this, qit, riid, ppv);
}

IFACEMETHODIMP_(ULONG) CVtfThumbnailProviderFactory::AddRef()
{
    return InterlockedIncrement(&m_cRef);
}

IFACEMETHODIMP_(ULONG) CVtfThumbnailProviderFactory::Release()
{
    ULONG cRef = InterlockedDecrement(&m_cRef);
    if (cRef == 0)
    {
        delete this;
    }
    return cRef;
}

// IClassFactory
IFACEMETHODIMP CVtfThumbnailProviderFactory::CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppv)
{
    if (pUnkOuter != NULL)
        return CLASS_E_NOAGGREGATION;

    CVtfThumbnailProvider* pProv = new (std::nothrow) CVtfThumbnailProvider();
    if (!pProv)
        return E_OUTOFMEMORY;

    HRESULT hr = pProv->QueryInterface(riid, ppv);
    pProv->Release();
    return hr;
}

IFACEMETHODIMP CVtfThumbnailProviderFactory::LockServer(BOOL fLock)
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

    if (IsEqualCLSID(rclsid, CLSID_VtfThumbnailProvider))
    {
        CVtfThumbnailProviderFactory* pFactory = new (std::nothrow) CVtfThumbnailProviderFactory();
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
    HRESULT hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{8D2F7A31-4B6C-4E95-9A47-C1E52D80B6F3}", NULL, L"Vortigaunt VTF Thumbnail Provider");
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{8D2F7A31-4B6C-4E95-9A47-C1E52D80B6F3}\\InprocServer32", NULL, szModule);
    }
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{8D2F7A31-4B6C-4E95-9A47-C1E52D80B6F3}\\InprocServer32", L"ThreadingModel", L"Apartment");
    }
    if (SUCCEEDED(hr))
    {
        // Disable process isolation to run in-process or avoid stream-only sandbox blockages
        hr = CreateRegistryDwordValue(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{8D2F7A31-4B6C-4E95-9A47-C1E52D80B6F3}", L"DisableProcessIsolation", 1);
    }

    // Register file association under HKCU
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\.vtf\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", NULL, L"{8D2F7A31-4B6C-4E95-9A47-C1E52D80B6F3}");
    }
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\.vtf", L"PerceivedType", L"image");
    }

    // Register uppercase as well for compatibility
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\.VTF\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}", NULL, L"{8D2F7A31-4B6C-4E95-9A47-C1E52D80B6F3}");
    }
    if (SUCCEEDED(hr))
    {
        hr = CreateRegistryKey(HKEY_CURRENT_USER, L"Software\\Classes\\.VTF", L"PerceivedType", L"image");
    }

    // Notify shell that association has changed
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return hr;
}

STDAPI DllUnregisterServer()
{
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\.vtf\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\.VTF\\ShellEx\\{e357fccd-a995-4576-b01f-234630154e96}");

    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{8D2F7A31-4B6C-4E95-9A47-C1E52D80B6F3}\\InprocServer32");
    RegDeleteKeyW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{8D2F7A31-4B6C-4E95-9A47-C1E52D80B6F3}");

    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);

    return S_OK;
}
