#ifdef __cplusplus
extern "C" {
#endif

typedef void* SHAMap;
SHAMap
SHAMapInit();  // defaults to STATE

void SHAMapFree(SHAMap);

void
SHAMapAddStateItem(SHAMap m, char const* key, char const* value, unsigned valueSize);

void
SHAMapAddTxItem(
    SHAMap m,
    char const* key,
    char const* value1,
    unsigned value1Size,
    char const* value2,
    unsigned value2Size
);

void
SHAMapUpdateStateItem(SHAMap m, char const* key, char const* value, unsigned valueSize);

void
SHAMapDeleteKey(SHAMap m, char const* key);

void
SHAMapGetHash256(SHAMap m, char* hash);

void
SHAMapTest();

#ifdef __cplusplus
}
#endif
