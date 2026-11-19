#include "aiclient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

AIClient::AIClient(QObject *parent)
    : QObject(parent),
      networkManager_(new QNetworkAccessManager(this))
{
}

void AIClient::setApiKey(const QString &apiKey)
{
    apiKey_ = apiKey;
}

void AIClient::cancelActiveRequest()
{
    if (activeReply_)
        activeReply_->abort();
    const auto replies = activeImageReplies_.values();
    for (QNetworkReply *reply : replies)
        reply->abort();
}

bool AIClient::hasActiveRequest() const
{
    return activeReply_ != nullptr || !activeImageReplies_.isEmpty();
}

void AIClient::sendMessage(const QString &apiBase, const QString &apiKey,
                           const QString &model, const QJsonArray &messages,
                           const QJsonArray &tools)
{
    if (activeReply_) {
        emit requestFailed(QStringLiteral("已有请求正在处理中。"));
        emit requestFinished();
        return;
    }

    QString normalizedBase = apiBase.trimmed();
    while (normalizedBase.endsWith(QLatin1Char('/')))
        normalizedBase.chop(1);
    const QString endpoint = normalizedBase + QStringLiteral("/chat/completions");
    const QUrl url(endpoint);
    if (!url.isValid()
        || (url.scheme() != QStringLiteral("https")
            && url.scheme() != QStringLiteral("http"))
        || url.host().isEmpty()) {
        emit requestFailed(QStringLiteral("API Base URL 无效，请在设置中检查地址。"));
        emit requestFinished();
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization",
                         QByteArrayLiteral("Bearer ") + apiKey.toUtf8());

    QJsonObject body{
        {QStringLiteral("model"), model},
        {QStringLiteral("messages"), messages},
        {QStringLiteral("temperature"), 0.7}
    };
    if (!tools.isEmpty()) {
        body.insert(QStringLiteral("tools"), tools);
        body.insert(QStringLiteral("tool_choice"), QStringLiteral("auto"));
    }

    activeReply_ = networkManager_->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(activeReply_, &QNetworkReply::finished, this, [this]() {
        QNetworkReply *reply = activeReply_;
        activeReply_ = nullptr;
        handleReply(reply);
    });
}

quint64 AIClient::requestEmbedding(const QString &apiBase, const QString &apiKey,
                                   const QString &text)
{
    const quint64 requestId = nextEmbeddingRequestId_++;
    QString normalizedBase = apiBase.trimmed();
    while (normalizedBase.endsWith(QLatin1Char('/')))
        normalizedBase.chop(1);
    const QUrl url(normalizedBase + QStringLiteral("/embeddings"));
    if (!url.isValid()
        || (url.scheme() != QStringLiteral("https")
            && url.scheme() != QStringLiteral("http"))
        || url.host().isEmpty()) {
        QTimer::singleShot(0, this, [this, requestId]() {
            emit embeddingFailed(
                requestId,
                QStringLiteral("Embedding API Base URL 无效，请在设置中检查地址。"));
        });
        return requestId;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization",
                         QByteArrayLiteral("Bearer ") + apiKey.toUtf8());
    request.setTransferTimeout(60000);

    const QJsonObject body{
        {QStringLiteral("model"), QStringLiteral("embedding-3")},
        {QStringLiteral("input"), text}
    };
    QNetworkReply *reply = networkManager_->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        const QByteArray responseBody = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
        if (networkError != QNetworkReply::NoError
            || statusCode < 200 || statusCode >= 300
            || parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            QString details = networkError != QNetworkReply::NoError
                ? networkErrorText : parseError.errorString();
            if (document.isObject()) {
                const QString serverMessage =
                    document.object().value(QStringLiteral("error")).toObject()
                        .value(QStringLiteral("message")).toString();
                if (!serverMessage.isEmpty())
                    details = serverMessage;
            }
            emit embeddingFailed(requestId,
                                 QStringLiteral("Embedding 请求失败（HTTP %1）：%2")
                                     .arg(statusCode).arg(details));
            return;
        }

        const QJsonArray data =
            document.object().value(QStringLiteral("data")).toArray();
        const QJsonArray embedding = data.isEmpty()
            ? QJsonArray{}
            : data.first().toObject().value(QStringLiteral("embedding")).toArray();
        if (embedding.isEmpty()) {
            emit embeddingFailed(requestId,
                                 QStringLiteral("Embedding 响应中缺少有效的向量。"));
            return;
        }
        for (const QJsonValue &value : embedding) {
            if (!value.isDouble()) {
                emit embeddingFailed(requestId,
                                     QStringLiteral("Embedding 响应包含非数字向量元素。"));
                return;
            }
        }
        emit embeddingReceived(requestId, embedding);
    });
    return requestId;
}

quint64 AIClient::requestImageDescription(const QString &apiBase, const QString &apiKey,
                                          const QString &imageDataUrl)
{
    const quint64 requestId = nextImageDescriptionRequestId_++;
    QString normalizedBase = apiBase.trimmed();
    while (normalizedBase.endsWith(QLatin1Char('/')))
        normalizedBase.chop(1);
    const QUrl url(normalizedBase + QStringLiteral("/chat/completions"));
    if (!url.isValid()
        || (url.scheme() != QStringLiteral("https")
            && url.scheme() != QStringLiteral("http"))
        || url.host().isEmpty()) {
        QTimer::singleShot(0, this, [this, requestId]() {
            emit imageDescriptionFailed(
                requestId, QStringLiteral("视觉 API Base URL 无效。"));
        });
        return requestId;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization",
                         QByteArrayLiteral("Bearer ") + apiKey.toUtf8());
    request.setTransferTimeout(60000);

    const QJsonArray content{
        QJsonObject{
            {QStringLiteral("type"), QStringLiteral("text")},
            {QStringLiteral("text"), QStringLiteral("请用中文准确描述这张图片中的主要内容、文字和场景。")}
        },
        QJsonObject{
            {QStringLiteral("type"), QStringLiteral("image_url")},
            {QStringLiteral("image_url"), QJsonObject{
                {QStringLiteral("url"), imageDataUrl}
            }}
        }
    };
    const QJsonObject body{
        {QStringLiteral("model"), QStringLiteral("glm-5.3-flash")},
        {QStringLiteral("messages"), QJsonArray{
            QJsonObject{
                {QStringLiteral("role"), QStringLiteral("user")},
                {QStringLiteral("content"), content}
            }
        }},
        {QStringLiteral("temperature"), 0.2}
    };

    QNetworkReply *reply = networkManager_->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    activeImageReplies_.insert(requestId, reply);
    connect(reply, &QNetworkReply::finished, this, [this, reply, requestId]() {
        const QByteArray responseBody = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        activeImageReplies_.remove(requestId);
        reply->deleteLater();

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
        if (networkError != QNetworkReply::NoError
            || statusCode < 200 || statusCode >= 300
            || parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            QString details = networkError != QNetworkReply::NoError
                ? networkErrorText : parseError.errorString();
            if (document.isObject()) {
                const QString serverMessage =
                    document.object().value(QStringLiteral("error")).toObject()
                        .value(QStringLiteral("message")).toString();
                if (!serverMessage.isEmpty())
                    details = serverMessage;
            }
            emit imageDescriptionFailed(
                requestId, QStringLiteral("图片视觉分析失败（HTTP %1）：%2")
                               .arg(statusCode).arg(details));
            return;
        }

        const QJsonArray choices =
            document.object().value(QStringLiteral("choices")).toArray();
        const QString description = choices.isEmpty()
            ? QString()
            : choices.first().toObject()
                  .value(QStringLiteral("message")).toObject()
                  .value(QStringLiteral("content")).toString().trimmed();
        if (description.isEmpty()) {
            emit imageDescriptionFailed(
                requestId, QStringLiteral("视觉 API 响应中没有图片描述。"));
            return;
        }
        emit imageDescriptionReceived(requestId, description);
    });
    return requestId;
}

void AIClient::searchWeb(const QString &query, const QString &apiBase)
{
    if (QUrl(apiBase).host() != QStringLiteral("open.bigmodel.cn")) {
        emit searchFailed(QStringLiteral("当前 API Base URL 不支持内置联网搜索。"));
        return;
    }

    QNetworkRequest request(QUrl(QStringLiteral(
        "https://open.bigmodel.cn/api/paas/v4/web_search")));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    request.setRawHeader("Authorization",
                         QByteArrayLiteral("Bearer ") + apiKey_.toUtf8());
    request.setTransferTimeout(15000);

    const QJsonObject body{
        {QStringLiteral("search_query"), query},
        {QStringLiteral("search_engine"), QStringLiteral("search_std")},
        {QStringLiteral("count"), 5},
        {QStringLiteral("search_intent"), false}
    };

    QNetworkReply *reply = networkManager_->post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray responseBody = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const QString networkErrorText = reply->errorString();
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
        if (networkError != QNetworkReply::NoError
            || parseError.error != QJsonParseError::NoError
            || !document.isObject()) {
            QString details = networkError != QNetworkReply::NoError
                ? networkErrorText
                : parseError.errorString();
            if (document.isObject()) {
                const QString serverMessage =
                    document.object().value(QStringLiteral("error")).toObject()
                        .value(QStringLiteral("message")).toString();
                if (!serverMessage.isEmpty())
                    details = serverMessage;
            }
            emit searchFailed(QStringLiteral("联网搜索失败（HTTP %1）：%2")
                                  .arg(statusCode).arg(details));
            return;
        }

        const QJsonValue results = document.object().value(QStringLiteral("search_result"));
        if (!results.isArray()) {
            emit searchFailed(QStringLiteral("搜索响应中缺少 search_result。"));
            return;
        }
        emit searchCompleted(results.toArray());
    });
}

void AIClient::requestBalance(const QString &apiBase)
{
    if (QUrl(apiBase).host() != QStringLiteral("open.bigmodel.cn"))
        return;

    QNetworkRequest request(QUrl(QStringLiteral(
        "https://open.bigmodel.cn/api/paas/v4/users/me/balance")));
    request.setRawHeader("Authorization",
                         QByteArrayLiteral("Bearer ") + apiKey_.toUtf8());

    QNetworkReply *reply = networkManager_->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        const QByteArray responseBody = reply->readAll();
        const QNetworkReply::NetworkError networkError = reply->error();
        const int statusCode =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        reply->deleteLater();
        if (networkError != QNetworkReply::NoError || statusCode < 200 || statusCode >= 300)
            return;

        QJsonParseError parseError;
        const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject())
            return;

        const QJsonObject root = document.object();
        if (root.contains(QStringLiteral("success"))
            && !root.value(QStringLiteral("success")).toBool()) {
            return;
        }
        QJsonObject data = root.value(QStringLiteral("data")).toObject();
        if (data.isEmpty() && root.value(QStringLiteral("data")).isArray()) {
            const QJsonArray rows = root.value(QStringLiteral("data")).toArray();
            if (!rows.isEmpty())
                data = rows.first().toObject();
        }

        const QStringList balanceKeys{
            QStringLiteral("available_balance"),
            QStringLiteral("cash_balance"),
            QStringLiteral("balance")
        };
        for (const QString &key : balanceKeys) {
            const QJsonValue value = data.value(key).isUndefined()
                ? root.value(key) : data.value(key);
            if (!value.isUndefined() && !value.isNull()) {
                emit balanceReceived(value.toVariant().toString());
                return;
            }
        }
    });
}

void AIClient::handleReply(QNetworkReply *reply)
{
    const QByteArray responseBody = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(responseBody, &parseError);
    if (networkError != QNetworkReply::NoError) {
        QString details = networkErrorText;
        if (document.isObject()) {
            const QJsonObject error = document.object().value(QStringLiteral("error")).toObject();
            const QString serverMessage = error.value(QStringLiteral("message")).toString();
            if (!serverMessage.isEmpty())
                details = serverMessage;
        }
        emit requestFailed(QStringLiteral("请求失败（HTTP %1）：%2")
                               .arg(statusCode).arg(details));
        emit requestFinished();
        return;
    }

    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        emit requestFailed(QStringLiteral("服务器返回了无法解析的 JSON：%1")
                               .arg(parseError.errorString()));
        emit requestFinished();
        return;
    }

    const QJsonObject root = document.object();
    const QJsonArray choices = root.value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty() || !choices.first().isObject()) {
        emit requestFailed(QStringLiteral("服务器响应中缺少 choices[0]。"));
        emit requestFinished();
        return;
    }

    const QJsonObject message =
        choices.first().toObject().value(QStringLiteral("message")).toObject();
    const QJsonValue contentValue = message.value(QStringLiteral("content"));
    const QJsonValue toolCallsValue = message.value(QStringLiteral("tool_calls"));
    if (!contentValue.isString() && !toolCallsValue.isArray()) {
        emit requestFailed(QStringLiteral("服务器响应中缺少有效的 message.content。"));
        emit requestFinished();
        return;
    }

    const QJsonObject usage = root.value(QStringLiteral("usage")).toObject();
    emit messageReceived(message,
                         usage.value(QStringLiteral("prompt_tokens")).toInt(),
                         usage.value(QStringLiteral("completion_tokens")).toInt());
    emit requestFinished();
}
