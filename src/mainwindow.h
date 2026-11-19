#pragma once

#include "settingsdialog.h"
#include "skillmanager.h"

#include <QByteArray>
#include <QImage>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMainWindow>
#include <QStringList>
#include <QVector>
#include <functional>

class AIClient;
class QCloseEvent;
class QComboBox;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QScrollArea;
class QToolButton;
class QTextBrowser;
class QTextEdit;
class QWidget;
class KnowledgeBaseDialog;
class QNetworkAccessManager;
class QNetworkReply;
class QProcess;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);

    bool initialize();

private slots:
    void openSettings();
    void openActionLog();
    void sendCurrentMessage();
    void chooseImages();
    void clearSelectedImages();
    void openKnowledgeBase();
    void importKnowledgeDocuments(const QStringList &paths);
    void deleteKnowledgeDocument(const QString &documentId);
    void handleModelMessage(const QJsonObject &message, int promptTokens,
                            int completionTokens);
    void appendError(const QString &message);
    void showSearchResults(const QJsonArray &results);
    void searchFailed(const QString &message);

private:
    void applyConfig(const AppConfig &config);
    void loadCurrentHistory();
    void saveCurrentHistory();
    void renderCurrentHistory();
    QJsonObject buildSystemMessage() const;
    void appendChatHtml(const QString &html);
    void appendUserMessage(const QString &content);
    void appendImagePreviews();
    void updateImagePreview();
    void trimImageHistory();
    void sendQuestionToModel(const QString &question, const QJsonArray &searchResults);
    void finishQuestionWithKnowledge(const QJsonArray &searchResults,
                                     const QJsonArray &queryEmbedding);
    void updateKnowledgeSystemMessage();
    void processNextKnowledgeDocument();
    void startKnowledgeDocument(const QString &path, const QString &text);
    void requestNextKnowledgeChunk();
    void finishKnowledgeDocument();
    void finishKnowledgeImport();
    QJsonObject loadKnowledgeBase(QString *errorMessage = nullptr) const;
    bool saveKnowledgeBase(const QJsonObject &knowledgeBase,
                           QString *errorMessage = nullptr) const;
    void refreshKnowledgeDialog();
    QString historyFilePath() const;
    QString knowledgeBaseFilePath() const;
    void sendMessagesToModel();
    void processNextToolCall();
    void continueAfterTaskApproval();
    void stopTask();
    void pauseTaskAtLimit();
    void checkReminders();
    void updateTaskModeUi();
    void toggleTaskMode();
    void requestSkillDraft(const QString &taskResult);
    void showSkillDraftDialog(const QString &content);
    void executeToolCall(const QString &id, const QString &name,
                         const QJsonObject &arguments);
    void completeToolCall(const QString &id, const QString &name,
                          const QString &arguments, const QString &result);
    void runCommand(const QString &command,
                    const std::function<void(const QString &)> &completion);
    QJsonArray computerTools() const;
    bool needsTaskApproval(const QString &name, const QJsonObject &arguments,
                           QString *preview) const;
    bool requiresCriticalConfirmation(const QString &command) const;
    void executeHttpTool(const QString &id, const QString &name,
                         const QJsonObject &arguments);
    QJsonObject loadMemory(QString *errorMessage = nullptr) const;
    QString memorySummary() const;
    QString appendMemory(const QString &content);
    void saveSessionMemory();
    bool isHealthConversation(const QString &text) const;
    void closeEvent(QCloseEvent *event) override;
    void updateUsageStatus();
    void applyFontSize();
    void persistConfig();
    bool selectChannelByModel(const QString &model);
    void refreshSkills();
    QString actionLogFilePath() const;
    void appendActionLog(const QString &name, const QString &arguments,
                         const QString &result);
    bool saveTaskResult(const QString &content, QString *savedPath,
                        QString *errorMessage) const;
    void pauseTaskForToolFailures(const QString &failures);

    AppConfig config_;
    QVector<Skill> skills_;
    QJsonArray messages_;
    QString activeSkillContext_;
    QStringList activeSkillNames_;
    QString pendingQuestion_;
    QJsonArray pendingSearchResults_;
    QString taskId_;
    QJsonArray sessionTaskSummaries_;
    QJsonArray pendingToolCalls_;
    QJsonArray pendingVisualParts_;
    int pendingToolCallIndex_ = 0;
    int toolRounds_ = 0;
    int agentStepCount_ = 0;
    int taskToolCallCount_ = 0;
    int taskFailureRetryCount_ = 0;
    bool taskMode_ = false;
    bool taskModePreference_ = false;
    bool taskPlanning_ = false;
    bool taskFailureReflectionPending_ = false;
    bool taskRetryPending_ = false;
    bool taskPausedForFailureDecision_ = false;
    bool taskAuditInProgress_ = false;
    bool skillDraftRequestPending_ = false;
    bool taskStopping_ = false;
    bool awaitingTaskConfirmation_ = false;
    bool taskApprovalGranted_ = false;
    bool taskConfirmationBlocked_ = false;
    bool suppressCancelledRequestError_ = false;
    bool toolCallInProgress_ = false;
    bool knowledgeImportActive_ = false;
    bool currentKnowledgeDocumentFailed_ = false;
    quint64 queryEmbeddingRequestId_ = 0;
    quint64 importEmbeddingRequestId_ = 0;
    int currentKnowledgeFileIndex_ = 0;
    int currentKnowledgeChunkIndex_ = 0;
    QStringList pendingKnowledgeFiles_;
    QString currentKnowledgeText_;
    QString currentKnowledgeFileName_;
    QJsonObject currentKnowledgeDocument_;
    QJsonArray currentKnowledgeChunks_;
    QString knowledgeContext_;
    QString taskRecentAction_;
    QStringList taskFailureNotes_;
    QString pendingImageToolCallId_;
    QString pendingImageToolArguments_;
    quint64 pendingImageDescriptionRequestId_ = 0;
    QString approvedToolCallId_;
    QHash<QString, int> taskAttemptCounts_;
    QStringList approvedTaskScopes_;
    AIClient *aiClient_;
    QNetworkAccessManager *toolNetworkManager_;
    QNetworkReply *activeToolNetworkReply_ = nullptr;
    QProcess *activeToolProcess_ = nullptr;
    QComboBox *modelCombo_;
    QTextEdit *conversationEdit_;
    QTextEdit *inputEdit_;
    QPushButton *sendButton_;
    QPushButton *modeToggleButton_;
    QToolButton *webSearchButton_;
    QToolButton *computerControlButton_;
    QToolButton *fontIncreaseButton_;
    QToolButton *fontDecreaseButton_;
    QToolButton *searchResultsToggle_;
    QTextBrowser *searchResultsView_;
    QLabel *usageLabel_;
    QLabel *balanceLabel_;
    QLabel *agentStatusLabel_;
    QLabel *taskStatusLabel_;
    QPushButton *stopTaskButton_;
    QLabel *permissionLabel_;
    QToolButton *imageButton_;
    QPushButton *clearImagesButton_;
    QScrollArea *imagePreviewArea_;
    QWidget *imagePreviewContainer_;
    QHBoxLayout *imagePreviewLayout_;
    QPushButton *knowledgeBaseButton_;
    KnowledgeBaseDialog *knowledgeDialog_ = nullptr;

    struct ImageAttachment
    {
        QString fileName;
        QString mimeType;
        QByteArray bytes;
        QImage thumbnail;
    };
    QVector<ImageAttachment> selectedImages_;
};
