/*
 * Copyright (C) 2016 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef _ERROR_CODE_H_
#define _ERROR_CODE_H_

enum ErrorCode {
    kNoError = -1,
    kLowBattery = 20,
    kZipVerificationFailure,
    kZipOpenFailure,
    kBootreasonInBlacklist
};

enum CauseCode {
    kNoCause = -1,
    kArgsParsingFailure = 100,
    kStashCreationFailure,
    kFileOpenFailure,
    kLseekFailure,
    kFreadFailure,
    kFwriteFailure,
    kFsyncFailure,
    kLibfecFailure,
    kFileGetPropFailure,
    kFileRenameFailure,
    kSymlinkFailure,
    kSetMetadataFailure,
    kTune2FsFailure,
    kRebootFailure,
    // update_binary aborted at load time because a shared library it depends
    // on could not be resolved by the dynamic linker.
    kSharedLibLoadFailure,
    // Special case of the above: the missing/mismatched library is an OpenSSL
    // library (libcrypto/libssl), i.e. the recovery image and update_binary
    // were built against incompatible OpenSSL sonames.
    kOpensslCompatFailure,
    kVendorFailure = 200
};

enum UncryptErrorCode {
    kUncryptNoError = -1,
    kUncryptErrorPlaceholder = 50,
    kUncryptTimeoutError = 100,
    kUncryptFileRemoveError,
    kUncryptFileOpenError,
    kUncryptSocketOpenError,
    kUncryptSocketWriteError,
    kUncryptSocketListenError,
    kUncryptSocketAcceptError,
    kUncryptFstabReadError,
    kUncryptFileStatError,
    kUncryptBlockOpenError,
    kUncryptIoctlError,
    kUncryptReadError,
    kUncryptWriteError,
    kUncryptFileSyncError,
    kUncryptFileCloseError,
    kUncryptFileRenameError,
    kUncryptPackageMissingError,
};

// ----------------------------------------------------------------------------
// Human-readable, stable string tokens for the codes above.
//
// These tokens are the on-disk contract written to the detailed OTA status
// cookie (see DETAILED_OTA_STATUS in install.cpp). They are intentionally
// SCREAMING_SNAKE and stable; keep them in sync when adding a new enum value.
// Any code not explicitly mapped falls back to "UNKNOWN_ERROR".
// ----------------------------------------------------------------------------

static inline const char* error_code_to_token(int code) {
    switch (code) {
        case kNoError:                 return "NONE";
        case kLowBattery:              return "LOW_BATTERY";
        case kZipVerificationFailure:  return "ZIP_VERIFICATION_FAILURE";
        case kZipOpenFailure:          return "ZIP_OPEN_FAILURE";
        case kBootreasonInBlacklist:   return "BOOTREASON_IN_BLACKLIST";
        default:                       return "UNKNOWN_ERROR";
    }
}

static inline const char* cause_code_to_token(int code) {
    switch (code) {
        case kNoCause:                 return "NONE";
        case kArgsParsingFailure:      return "ARGS_PARSING_FAILURE";
        case kStashCreationFailure:    return "STASH_CREATION_FAILURE";
        case kFileOpenFailure:         return "FILE_OPEN_FAILURE";
        case kLseekFailure:            return "LSEEK_FAILURE";
        case kFreadFailure:            return "FREAD_FAILURE";
        case kFwriteFailure:           return "FWRITE_FAILURE";
        case kFsyncFailure:            return "FSYNC_FAILURE";
        case kLibfecFailure:           return "LIBFEC_FAILURE";
        case kFileGetPropFailure:      return "FILE_GET_PROP_FAILURE";
        case kFileRenameFailure:       return "FILE_RENAME_FAILURE";
        case kSymlinkFailure:          return "SYMLINK_FAILURE";
        case kSetMetadataFailure:      return "SET_METADATA_FAILURE";
        case kTune2FsFailure:          return "TUNE2FS_FAILURE";
        case kRebootFailure:           return "REBOOT_FAILURE";
        case kSharedLibLoadFailure:    return "SHARED_LIB_LOAD_ERROR";
        case kOpensslCompatFailure:    return "OPENSSL_COMPAT_ERROR";
        case kVendorFailure:           return "VENDOR_FAILURE";
        default:                       return "UNKNOWN_ERROR";
    }
}

static inline const char* uncrypt_error_code_to_token(int code) {
    switch (code) {
        case kUncryptNoError:             return "NONE";
        case kUncryptErrorPlaceholder:    return "UNCRYPT_ERROR_PLACEHOLDER";
        case kUncryptTimeoutError:        return "UNCRYPT_TIMEOUT_ERROR";
        case kUncryptFileRemoveError:     return "UNCRYPT_FILE_REMOVE_ERROR";
        case kUncryptFileOpenError:       return "UNCRYPT_FILE_OPEN_ERROR";
        case kUncryptSocketOpenError:     return "UNCRYPT_SOCKET_OPEN_ERROR";
        case kUncryptSocketWriteError:    return "UNCRYPT_SOCKET_WRITE_ERROR";
        case kUncryptSocketListenError:   return "UNCRYPT_SOCKET_LISTEN_ERROR";
        case kUncryptSocketAcceptError:   return "UNCRYPT_SOCKET_ACCEPT_ERROR";
        case kUncryptFstabReadError:      return "UNCRYPT_FSTAB_READ_ERROR";
        case kUncryptFileStatError:       return "UNCRYPT_FILE_STAT_ERROR";
        case kUncryptBlockOpenError:      return "UNCRYPT_BLOCK_OPEN_ERROR";
        case kUncryptIoctlError:          return "UNCRYPT_IOCTL_ERROR";
        case kUncryptReadError:           return "UNCRYPT_READ_ERROR";
        case kUncryptWriteError:          return "UNCRYPT_WRITE_ERROR";
        case kUncryptFileSyncError:       return "UNCRYPT_FILE_SYNC_ERROR";
        case kUncryptFileCloseError:      return "UNCRYPT_FILE_CLOSE_ERROR";
        case kUncryptFileRenameError:     return "UNCRYPT_FILE_RENAME_ERROR";
        case kUncryptPackageMissingError: return "UNCRYPT_PACKAGE_MISSING_ERROR";
        default:                          return "UNKNOWN_ERROR";
    }
}

#endif
