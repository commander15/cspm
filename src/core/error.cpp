#include "error.h"

#include <QString>

QString Error::errorString(int code)
{
    switch (code) {
    case CSPM_OK: return "Success";
    case CSPM_ERR_UNKNOWN: return "Unknown error occurred";
    case CSPM_ERR_INVALID_ARGUMENT: return "Invalid command-line argument";
    case CSPM_ERR_FILE_IO: return "File I/O error (read/write failed)";
    case CSPM_ERR_PERMISSION_DENIED: return "Permission denied";
    case CSPM_ERR_CONFIG_PARSE: return "Failed to parse configuration file";

    case CSPM_ERR_REGISTRY_UNREACHABLE: return "Registry is unreachable or offline";
    case CSPM_ERR_PACKAGE_NOT_FOUND: return "Package not found in registry";
    case CSPM_ERR_VERSION_NOT_FOUND: return "Specified package version not found";
    case CSPM_ERR_MANIFEST_PARSE: return "Failed to parse package manifest JSON";
    case CSPM_ERR_CHECKSUM_MISMATCH: return "Cryptographic checksum (SHA-256) verification failed";
    case CSPM_ERR_UNSUPPORTED_PLATFORM: return "Target platform or architecture is not supported by package";

    case CSPM_ERR_DOWNLOAD_FAILED: return "Download failed";
    case CSPM_ERR_HTTP_STATUS: return "HTTP error response received";

    case CSPM_ERR_EXTRACT_FAILED: return "Archive extraction failed (7z error)";
    case CSPM_ERR_UNSUPPORTED_FORMAT: return "Unsupported archive format";
    case CSPM_ERR_ARCHIVE_CORRUPT: return "Archive file is corrupt or incomplete";

    case CSPM_ERR_GIT_NOT_FOUND: return "Git executable not found in PATH";
    case CSPM_ERR_GIT_CLONE_FAILED: return "Git clone operation failed";
    case CSPM_ERR_GIT_CHECKOUT_FAILED: return "Git checkout/tag failed";

    case CSPM_ERR_CMAKE_CONFIG_FAILED: return "CMake configuration step failed";
    case CSPM_ERR_CMAKE_BUILD_FAILED: return "CMake build step failed";
    case CSPM_ERR_CMAKE_TEST_FAILED: return "CMake test suite execution failed";
    case CSPM_ERR_CMAKE_INSTALL_FAILED: return "CMake install step failed";
    case CSPM_ERR_NINJA_NOT_FOUND: return "Ninja build generator not found in PATH";

    default: return errorString(CSPM_ERR_UNKNOWN);
    }
}
