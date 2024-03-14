#ifndef SMBCONTEXT_H
#define SMBCONTEXT_H

#include <memory>

typedef struct _SMBCCTX SMBCCTX;

const int SMB_FREE_EVEN_IF_BUSY = 1;
const int SMB_DEBUG_LEVEL = 5;

class SMBContext
{
public:
    SMBContext();
    ~SMBContext() = default;

    SMBContext(const SMBContext&) = delete;
    SMBContext& operator=(const SMBContext&) = delete;

    SMBContext(SMBContext&&) = default;
    SMBContext& operator=(SMBContext&&) = default;

    bool is_valid() const;

    int smbcGetxattr(const char *fname, const char *name, const void *value, size_t size);

private:
    SMBCCTX* createContext();
    static void freeContext(SMBCCTX* context);

    std::unique_ptr<SMBCCTX, decltype(&SMBContext::freeContext)> smb_ctx_ptr;
};

#endif // SMBCONTEXT_H
