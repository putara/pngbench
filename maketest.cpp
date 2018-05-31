#include <stdio.h>
#include <windows.h>
#include <pshpack1.h>

typedef struct tagGRPICONDIRENTRY
{
    BYTE            bWidth;
    BYTE            bHeight;
    BYTE            bColorCount;
    BYTE            bReserved;
    WORD            wPlanes;
    WORD            wBitCount;
    DWORD           dwBytesInRes;
    WORD            nID;
} GRPICONDIRENTRY, *LPGRPICONDIRENTRY;

typedef struct tagGRPICONDIR
{
    WORD            idReserved;
    WORD            idType;
    WORD            idCount;
    GRPICONDIRENTRY idEntries[1];
} GRPICONDIR, *LPGRPICONDIR;

void* GetResource(
    __in HMODULE hmod,
    __in LPCTSTR type,
    __in LPCTSTR name,
    __in WORD langId,
    __out DWORD* pcb)
{
    HRSRC hrsrc = ::FindResourceEx(hmod, type, name, langId);
    if (hrsrc != NULL) {
        HGLOBAL hglob = ::LoadResource(hmod, hrsrc);
        if (hglob != NULL) {
            *pcb = ::SizeofResource(hmod, hrsrc);
            return ::LockResource(hglob);
        }
    }
    *pcb = 0;
    return NULL;
}

#define MAX_IMAGES      512
UINT image = 0;
UINT bytes[MAX_IMAGES];

BOOL CALLBACK EnumResNameProc(HMODULE hmod, LPCTSTR type, LPTSTR name, LPARAM lp)
{
    FILE* fp = reinterpret_cast<FILE*>(lp);
    DWORD cb;
    GRPICONDIR* grp = static_cast<GRPICONDIR*>(GetResource(hmod, type, name, 0, &cb));
    for (UINT i = 0; i < grp->idCount; i++) {
        if (grp->idEntries[i].bWidth != 0 || grp->idEntries[i].bHeight != 0) {
            continue;
        }
        unsigned char* bits = static_cast<unsigned char*>(GetResource(hmod, RT_ICON, MAKEINTRESOURCE(grp->idEntries[i].nID), 0, &cb));
        if (bits == NULL || memcmp(bits, "\x89PNG\r\n\x1a\n", 8) != 0) {
            continue;
        }
        bytes[image] = grp->idEntries[i].dwBytesInRes;
        fprintf(fp, "__declspec(align(16)) static unsigned char img_%03u[] = {", image);
        for (DWORD j = 0; j < grp->idEntries[i].dwBytesInRes; j++) {
            fprintf(fp, "%u,", bits[j]);
        }
        fprintf(fp, "};\r\n");
        if (++image >= MAX_IMAGES) {
            return FALSE;
        }
        break;
    }
    return TRUE;
}

const char* fname(const char* s)
{
    const char* f = s;
    while (*s) {
        if (*s == '/' || *s == '\\') {
            f = s + 1;
        }
        s++;
    }
    return f;
}

int main(int argc, char** argv)
{
    if (argc < 3) {
        return 1;
    }
    FILE* fp = fopen(argv[1], "wb");
    fprintf(fp, "#include \"%s\"\r\n#pragma data_seg(\".png\")\r\n", fname(argv[2]));
    HMODULE hmod1 = ::LoadLibraryEx(TEXT("imageres.dll"), NULL, LOAD_LIBRARY_AS_DATAFILE);
    ::EnumResourceNamesEx(hmod1, RT_GROUP_ICON, EnumResNameProc, reinterpret_cast<LPARAM>(fp), RESOURCE_ENUM_LN, 0);
    HMODULE hmod2 = ::LoadLibraryEx(TEXT("shell32.dll"), NULL, LOAD_LIBRARY_AS_DATAFILE);
    ::EnumResourceNamesEx(hmod2, RT_GROUP_ICON, EnumResNameProc, reinterpret_cast<LPARAM>(fp), RESOURCE_ENUM_LN, 0);
    fprintf(fp, "#pragma data_seg()\r\n");
    fprintf(fp, "const struct TESTIMAGE images[%u] = {", image);
    for (UINT i = 0; i < image; i++) {
        fprintf(fp, "{%u,img_%03u},", bytes[i], i);
    }
    fprintf(fp, "};\r\n#ifdef __cplusplus\r\n}\r\n#endif\r\n");
    fp = fopen(argv[2], "wb");
    fprintf(fp, "#ifdef __cplusplus\r\nextern \"C\" {\r\n#endif\r\nstruct TESTIMAGE {\r\n\tunsigned int size;\r\n\tconst unsigned char* data;\r\n};\r\nextern const struct TESTIMAGE images[%u];\r\n#ifdef __cplusplus\r\n}\r\n#endif\r\n", image);
    return 0;
}
