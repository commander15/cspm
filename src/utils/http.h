#ifndef HTTP_H
#define HTTP_H

#include <QUrl>

class ConsoleStreams;

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
    static HttpResponse get(const QUrl &url, const ConsoleStreams &console);
    static HttpResponse get(const QUrl &url, class QIODevice *device);
    static HttpResponse get(const QUrl &url, class QIODevice *device, const ConsoleStreams &console);
};

#endif // HTTP_H
