#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>
#include <QtGlobal>

class QNetworkAccessManager;
class QNetworkReply;

class AIClient : public QObject
{
    Q_OBJECT

public:
    explicit AIClient(QObject *parent = nullptr);

    void setApiKey(const QString &apiKey);
    void cancelActiveRequest();
    bool hasActiveRequest() const;
    void sendMessage(const QString &apiBase, const QString &apiKey,
                     const QString &model, const QJsonArray &messages,
                     const QJsonArray &tools = {});
    quint64 requestEmbedding(const QString &apiBase, const QString &apiKey,
                             const QString &text);
    quint64 requestImageDescription(const QString &apiBase, const QString &apiKey,
                                    const QString &imageDataUrl);
    void searchWeb(const QString &query, const QString &apiBase);
    void requestBalance(const QString &apiBase);

signals:
    void messageReceived(const QJsonObject &message, int promptTokens, int completionTokens);
    void requestFailed(const QString &message);
    void requestFinished();
    void searchCompleted(const QJsonArray &results);
    void searchFailed(const QString &message);
    void balanceReceived(const QString &balance);
    void embeddingReceived(quint64 requestId, const QJsonArray &embedding);
    void embeddingFailed(quint64 requestId, const QString &message);
    void imageDescriptionReceived(quint64 requestId, const QString &description);
    void imageDescriptionFailed(quint64 requestId, const QString &message);

private:
    void handleReply(QNetworkReply *reply);

    QNetworkAccessManager *networkManager_;
    QString apiKey_;
    QNetworkReply *activeReply_ = nullptr;
    QHash<quint64, QNetworkReply *> activeImageReplies_;
    quint64 nextEmbeddingRequestId_ = 1;
    quint64 nextImageDescriptionRequestId_ = 1;
};
