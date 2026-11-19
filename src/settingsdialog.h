#pragma once

#include <QDialog>
#include <QString>
#include <QVector>

class QLineEdit;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;

struct RemoteSkill
{
    QString name;
    QString version;
    QString branch;
    QStringList files;
};

struct ApiChannel
{
    QString id;
    QString displayName;
    QString apiBase;
    QString apiKey;
    QString model;
};

struct AppConfig
{
    QString apiBase = QStringLiteral("https://open.bigmodel.cn/api/paas/v4");
    QString apiKey;
    QString model = QStringLiteral("glm-4.7-flash");
    QString skillRepo = QStringLiteral("https://github.com/OWNER/REPOSITORY");
    QVector<ApiChannel> channels;
    QString selectedChannelId;
    int fontSize = 12;
    quint64 totalTokens = 0;
    quint64 searchCount = 0;
    bool computerControlEnabled = true;
    bool webSearchEnabled = false;
    bool taskModeEnabled = false;
};

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const AppConfig &config, QWidget *parent = nullptr);

    AppConfig config() const;

    static QString configFilePath();
    static bool loadConfig(AppConfig *config, QString *errorMessage);
    static bool saveConfig(const AppConfig &config, QString *errorMessage);

private slots:
    void saveAndAccept();
    void addChannel();
    void editChannel();
    void deleteChannel();
    void testChannel();
    void refreshSkills();
    void editSkill();
    void importSkillFromZip();
    void importSkillFromFolder();
    void deleteSkill();
    void saveSkillRepository();
    void checkSkillUpdates();
    void installSelectedSkills();

private:
    bool saveChannels(QString *errorMessage);
    int selectedChannelIndex() const;
    void refreshChannels();
    bool repositoryLocation(QString *owner, QString *repository,
                            QString *errorMessage) const;
    bool downloadRemoteSkill(const RemoteSkill &skill, const QString &owner,
                             const QString &repository, QString *stagedDirectory,
                             QString *errorMessage);
    bool reviewScripts(const QString &skillDirectory);

    AppConfig config_;
    QListWidget *channelList_;
    QListWidget *skillList_;
    QListWidget *remoteSkillList_;
    QLabel *skillStatusLabel_;
    QLineEdit *skillRepoEdit_;
    QSpinBox *fontSizeSpinBox_;
};
