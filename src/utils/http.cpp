#include "http.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>

HttpResponse HTTP::get(const QUrl &url)
{
    HttpResponse response;

    QNetworkAccessManager manager;
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");

    QNetworkReply *reply = manager.get(request);

    // Stack-allocated local event loop (safe, temporary, cleans up automatically)
    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        response.data = reply->readAll();
        response.statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    } else {
        response.errorString = reply->errorString();
        response.statusCode = 400;
    }

    reply->deleteLater();
    return response;
}
