#include "http.h"

#include <core/command.h>

#include <QBuffer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>

HttpResponse HTTP::get(const QUrl &url)
{
    return get(url, ConsoleStreams::null());
}

HttpResponse HTTP::get(const QUrl &url, const ConsoleStreams &console)
{
    QBuffer buffer;
    if (!buffer.open(QIODevice::WriteOnly)) {
        return { .statusCode = 500, .errorString = buffer.errorString() };
    }

    HttpResponse response = get(url, &buffer, console);
    response.data = buffer.data();
    return response;
}

HttpResponse HTTP::get(const QUrl &url, QIODevice *device)
{
    return get(url, device, ConsoleStreams::null());
}

HttpResponse HTTP::get(const QUrl &url, QIODevice *device, const ConsoleStreams &console)
{
    HttpResponse response;

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = manager.get(request);

    // Stack-allocated local event loop (safe, temporary, cleans up automatically)
    QEventLoop loop;

    console.out << "downloading: " << url.fileName() << Qt::endl;
    QObject::connect(reply, &QNetworkReply::downloadProgress, device, [console](qint64 down, qint64 total) {
        if (total > 0) {
            qint64 percent = (down * 100) / total;

            // \r returns cursor to start of line, Qt::flush forces immediate terminal render
            console.out << "\rdownloaded (" << percent << "%): " << down << " / " << total << " bytes" << Qt::flush;

            // Print a final newline when the download completes so the next log starts fresh
            if (down >= total) {
                console.out << "\rdownloaded" << Qt::endl;
            }
        } else if (down > 0) {
            // Fallback if total size is unknown (-1 or 0)
            console.out << "\rdownloaded: " << down << " bytes (unknown total)" << Qt::flush;
        }
    });

    QObject::connect(reply, &QIODevice::readyRead, device, [device, reply] { device->write(reply->readAll()); });

    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    } else {
        response.errorString = reply->errorString();
        response.statusCode = 400;
    }

    reply->deleteLater();
    return response;
}
