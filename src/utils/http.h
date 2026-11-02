#ifndef HTTP_H
#define HTTP_H

#include <QUrl>

struct HttpResponse {
    int statusCode = 200;
    QByteArray data;
    QString errorString;

    bool success() const
    { return statusCode >= 200 && statusCode <= 299; }
};

class HTTP
{
public:
    static HttpResponse get(const QUrl &url);
};

#endif // HTTP_H
