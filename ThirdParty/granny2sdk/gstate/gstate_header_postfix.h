// Note no idempotency guard here, since we need this to always include

#if defined(_MSC_VER) || defined (__GNUG__)
  #pragma pack(pop)
#endif

#if defined(_MSC_VER)
  #pragma warning(pop)
  #pragma warning(disable : 4103)
#endif
