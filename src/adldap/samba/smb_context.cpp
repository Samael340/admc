#include "smb_context.h"

#include <libsmbclient.h>

SMBContext::SMBContext()
    : smb_ctx_ptr(createContext(), freeContext) {

}

bool SMBContext::is_valid() const {
    return bool(smb_ctx_ptr);
}

int SMBContext::smbcGetxattr(const char *fname, const char *name, const void *value, size_t size)
{
    return smbc_getFunctionGetxattr(smb_ctx_ptr.get())(smb_ctx_ptr.get(), fname, name, value, size);
}


SMBCCTX *SMBContext::createContext() {
    SMBCCTX* newContext = smbc_new_context();

    if (newContext) {
        // smbc_setDebug(newContext, SMB_DEBUG_LEVEL);
        smbc_setOptionUseKerberos(newContext, true);
        smbc_setOptionFallbackAfterKerberos(newContext, true);

        if (smbc_init_context(newContext) == nullptr) {
            smbc_free_context(newContext, SMB_FREE_EVEN_IF_BUSY);
            newContext = nullptr;
        }
    }

    return newContext;
}

void SMBContext::freeContext(SMBCCTX *context) {
    if (context) {
        smbc_free_context(context, SMB_FREE_EVEN_IF_BUSY);
    }
}
