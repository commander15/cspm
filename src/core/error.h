#ifndef ERROR_H
#define ERROR_H

#include "error_codes.h"

#include <QString>

class Error
{
public:
    static QString unknowError() { return errorString(CSPM_ERR_UNKNOWN); }
    static QString errorString(int code);
};

#endif // ERROR_H
