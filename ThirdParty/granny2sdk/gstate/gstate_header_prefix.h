// Note no idempotency guard here, since we need this to always include

#if defined(_MSC_VER) || defined (__GNUG__)
  #pragma pack(push)
  #pragma pack(4)
#endif

#if defined(_MSC_VER)
  #pragma warning(push)
  #pragma warning(disable : 4121 4127 4103 4100)
#endif
