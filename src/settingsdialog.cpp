#include "settingsdialog.h"

#include "skillmanager.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonParseError>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkProxyFactory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPushButton>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTabWidget>
#include <QTimer>
#include <QTextEdit>
#include <QUrl>
#include <QUrlQuery>
#include <QVersionNumber>
#include <QVBoxLayout>
#include <QUuid>

namespace {

QJsonObject channelToJson(const ApiChannel &channel)
{
    return QJsonObject{
        {QStringLiteral("id"), channel.id},
        {QStringLiteral("display_name"), channel.displayName},
        {QStringLiteral("base_url"), channel.apiBase},
        {QStringLiteral("api_key"), channel.apiKey},
        {QStringLiteral("model"), channel.model}
    };
}

bool validChannel(const ApiChannel &channel, QString *errorMessage)
{
    if (channel.displayName.trimmed().isEmpty()
        || channel.apiBase.trimmed().isEmpty()
        || channel.apiKey.trimmed().isEmpty()
        || channel.model.trimmed().isEmpty()) {
        *errorMessage = QStringLiteral("显示名、Base URL、API Key 和模型名均不能为空。");
        return false;
    }

    const QUrl url(channel.apiBase.trimmed());
    if (!url.isValid()
        || (url.scheme() != QStringLiteral("https")
            && url.scheme() != QStringLiteral("http"))
        || url.host().isEmpty()) {
        *errorMessage = QStringLiteral("Base URL 必须是有效的 HTTP 或 HTTPS 地址。");
        return false;
    }
    return true;
}

class ChannelEditorDialog : public QDialog
{
public:
    explicit ChannelEditorDialog(const ApiChannel &channel, bool isNew,
                                 QWidget *parent = nullptr)
        : QDialog(parent),
          displayNameEdit_(new QLineEdit(channel.displayName, this)),
          apiBaseEdit_(new QLineEdit(channel.apiBase, this)),
          apiKeyEdit_(new QLineEdit(channel.apiKey, this)),
          modelEdit_(new QLineEdit(channel.model, this))
    {
        setWindowTitle(isNew ? QStringLiteral("添加通道") : QStringLiteral("编辑通道"));
        apiKeyEdit_->setEchoMode(QLineEdit::Password);

        auto *form = new QFormLayout;
        form->addRow(QStringLiteral("显示名:"), displayNameEdit_);
        form->addRow(QStringLiteral("Base URL:"), apiBaseEdit_);
        form->addRow(QStringLiteral("API Key:"), apiKeyEdit_);
        form->addRow(QStringLiteral("模型名:"), modelEdit_);

        auto *buttons = new QDialogButtonBox(
            QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
        connect(buttons, &QDialogButtonBox::accepted, this, [this]() {
            QString errorMessage;
            if (!validChannel(this->channel(), &errorMessage)) {
                QMessageBox::warning(this, QStringLiteral("通道信息无效"), errorMessage);
                return;
            }
            accept();
        });
        connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

        auto *layout = new QVBoxLayout(this);
        layout->addLayout(form);
        layout->addWidget(buttons);
        resize(480, 210);
    }

    ApiChannel channel() const
    {
        ApiChannel result;
        result.displayName = displayNameEdit_->text().trimmed();
        result.apiBase = apiBaseEdit_->text().trimmed();
        result.apiKey = apiKeyEdit_->text().trimmed();
        result.model = modelEdit_->text().trimmed();
        return result;
    }

private:
    QLineEdit *displayNameEdit_;
    QLineEdit *apiBaseEdit_;
    QLineEdit *apiKeyEdit_;
    QLineEdit *modelEdit_;
};

QString testChannelConnection(const ApiChannel &channel)
{
    QString validationError;
    if (!validChannel(channel, &validationError))
        return validationError;

    QString baseUrl = channel.apiBase.trimmed();
    while (baseUrl.endsWith(QLatin1Char('/')))
        baseUrl.chop(1);
    const QUrl url(baseUrl + QStringLiteral("/chat/completions"));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      QStringLiteral("application/json"));
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("Authorization",
                         QByteArrayLiteral("Bearer ") + channel.apiKey.toUtf8());
    request.setTransferTimeout(20000);

    const QJsonObject body{
        {QStringLiteral("model"), channel.model},
        {QStringLiteral("messages"), QJsonArray{
            QJsonObject{
                {QStringLiteral("role"), QStringLiteral("user")},
                {QStringLiteral("content"), QStringLiteral("请只回复：连接成功")}
            }
        }},
        {QStringLiteral("temperature"), 0}
    };

    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.post(
        request, QJsonDocument(body).toJson(QJsonDocument::Compact));
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    timeout.start(22000);
    loop.exec();

    const QByteArray responseBody = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const int statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (timedOut)
        return QStringLiteral("连接超时（22 秒）。");

    QJsonParseError parseError;
    const QJsonDocument response =
        QJsonDocument::fromJson(responseBody, &parseError);
    if (networkError != QNetworkReply::NoError) {
        QString detail = networkErrorText;
        if (response.isObject()) {
            const QString serverMessage =
                response.object().value(QStringLiteral("error")).toObject()
                    .value(QStringLiteral("message")).toString();
            if (!serverMessage.isEmpty())
                detail = serverMessage;
        }
        return QStringLiteral("网络请求失败（HTTP %1）：%2")
            .arg(statusCode).arg(detail);
    }

    if (statusCode < 200 || statusCode >= 300) {
        QString detail = QStringLiteral("服务器返回非成功状态。");
        if (response.isObject()) {
            const QJsonObject error =
                response.object().value(QStringLiteral("error")).toObject();
            const QString serverMessage =
                error.value(QStringLiteral("message")).toString();
            if (!serverMessage.isEmpty())
                detail = serverMessage;
        }
        return QStringLiteral("连接失败（HTTP %1）：%2")
            .arg(statusCode).arg(detail);
    }

    if (parseError.error != QJsonParseError::NoError || !response.isObject())
        return QStringLiteral("服务器返回了无法解析的 JSON：%1")
            .arg(parseError.errorString());
    const QJsonArray choices =
        response.object().value(QStringLiteral("choices")).toArray();
    if (choices.isEmpty()
        || !choices.first().toObject().value(QStringLiteral("message")).isObject()) {
        return QStringLiteral("请求已成功，但响应中缺少 choices[0].message。");
    }
    return QString();
}

struct HttpResponse
{
    QByteArray body;
    QString error;
    int statusCode = 0;
};

HttpResponse getHttpResponseOnce(const QUrl &url)
{
    QNetworkRequest request(url);
    request.setRawHeader("Accept", "application/json");
    request.setRawHeader("User-Agent", "WorkBuddy");
    request.setTransferTimeout(20000);

    QNetworkProxyFactory::setUseSystemConfiguration(true);
    QNetworkAccessManager manager;
    QNetworkReply *reply = manager.get(request);
    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    bool timedOut = false;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timeout, &QTimer::timeout, &loop, [&]() {
        timedOut = true;
        reply->abort();
        loop.quit();
    });
    timeout.start(22000);
    loop.exec();

    HttpResponse result;
    result.body = reply->readAll();
    result.statusCode =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (timedOut) {
        result.error = QStringLiteral("请求超时（22 秒）。");
    } else if (reply->error() != QNetworkReply::NoError) {
        QString detail = reply->errorString();
        QJsonParseError parseError;
        const QJsonDocument response =
            QJsonDocument::fromJson(result.body, &parseError);
        if (response.isObject()) {
            const QJsonObject error =
                response.object().value(QStringLiteral("error")).toObject();
            const QString message = error.value(QStringLiteral("message")).toString();
            if (!message.isEmpty())
                detail = message;
        }
        result.error = QStringLiteral("网络请求失败（HTTP %1）：%2")
                           .arg(result.statusCode).arg(detail);
    } else if (result.statusCode < 200 || result.statusCode >= 300) {
        QString detail = QStringLiteral("服务器返回非成功状态。");
        QJsonParseError parseError;
        const QJsonDocument response =
            QJsonDocument::fromJson(result.body, &parseError);
        if (response.isObject()) {
            const QJsonObject error =
                response.object().value(QStringLiteral("error")).toObject();
            const QString message =
                error.value(QStringLiteral("message")).toString();
            if (!message.isEmpty())
                detail = message;
        }
        result.error = QStringLiteral("请求失败（HTTP %1）：%2")
                           .arg(result.statusCode).arg(detail);
    }
    reply->deleteLater();
    return result;
}

HttpResponse getHttpResponse(const QUrl &url)
{
    HttpResponse response = getHttpResponseOnce(url);
    if (response.error.isEmpty()
        || url.host().compare(QStringLiteral("raw.githubusercontent.com"),
                              Qt::CaseInsensitive) != 0) {
        return response;
    }

    const QUrl mirrorUrl(
        QStringLiteral("https://ghproxy.net/") + url.toString(QUrl::FullyEncoded));
    HttpResponse mirrorResponse = getHttpResponseOnce(mirrorUrl);
    if (mirrorResponse.error.isEmpty())
        return mirrorResponse;

    mirrorResponse.error = QStringLiteral("%1；镜像重试失败：%2")
                               .arg(response.error, mirrorResponse.error);
    return mirrorResponse;
}

bool validSkillPath(const QString &path)
{
    if (path.isEmpty() || path.startsWith(QLatin1Char('/'))
        || path.contains(QLatin1Char('\\')) || path.contains(QLatin1Char(':'))) {
        return false;
    }
    const QStringList parts = path.split(QLatin1Char('/'));
    for (const QString &part : parts) {
        if (part.isEmpty() || part == QStringLiteral(".")
            || part == QStringLiteral("..")) {
            return false;
        }
    }
    return true;
}

bool validRemoteSkillName(const QString &name)
{
    static const QRegularExpression namePattern(
        QStringLiteral("^[A-Za-z0-9][A-Za-z0-9._-]{0,63}$"));
    return namePattern.match(name).hasMatch();
}

bool isRemoteVersionNewer(const QString &remote, const QString &local)
{
    QString remoteVersion = remote.trimmed();
    QString localVersion = local.trimmed();
    if (remoteVersion.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        remoteVersion.remove(0, 1);
    if (localVersion.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        localVersion.remove(0, 1);
    const QVersionNumber remoteNumber =
        QVersionNumber::fromString(remoteVersion);
    const QVersionNumber localNumber =
        QVersionNumber::fromString(localVersion);
    if (remoteNumber.isNull())
        return false;
    if (localNumber.isNull())
        return true;
    return QVersionNumber::compare(remoteNumber, localNumber) > 0;
}

QString sourceLabel(const QString &source)
{
    if (source == QStringLiteral("builtin"))
        return QStringLiteral("内置");
    if (source == QStringLiteral("github"))
        return QStringLiteral("GitHub");
    if (source == QStringLiteral("experience"))
        return QStringLiteral("经验沉淀");
    return QStringLiteral("已导入");
}

void showSkillScript(const QStringList &scriptPaths, QWidget *parent)
{
    QDialog viewer(parent);
    viewer.setWindowTitle(QStringLiteral("查看技能脚本"));
    auto *scriptList = new QListWidget(&viewer);
    auto *content = new QTextEdit(&viewer);
    content->setReadOnly(true);
    auto *layout = new QHBoxLayout(&viewer);
    layout->addWidget(scriptList, 1);
    layout->addWidget(content, 3);
    for (const QString &path : scriptPaths) {
        auto *item = new QListWidgetItem(QFileInfo(path).fileName(), scriptList);
        item->setData(Qt::UserRole, path);
    }
    QObject::connect(scriptList, &QListWidget::currentItemChanged,
                     &viewer, [content](QListWidgetItem *current) {
        if (!current) {
            content->clear();
            return;
        }
        QFile file(current->data(Qt::UserRole).toString());
        if (!file.open(QIODevice::ReadOnly)) {
            content->setPlainText(QStringLiteral("读取脚本失败：%1")
                                      .arg(file.errorString()));
            return;
        }
        if (file.size() > 1024 * 1024) {
            content->setPlainText(QStringLiteral("脚本超过 1MB，未在查看器中打开。"));
            return;
        }
        content->setPlainText(QString::fromUtf8(file.readAll()));
    });
    if (scriptList->count() > 0)
        scriptList->setCurrentRow(0);
    viewer.resize(850, 520);
    viewer.exec();
}

} // namespace

SettingsDialog::SettingsDialog(const AppConfig &config, QWidget *parent)
    : QDialog(parent),
      config_(config),
      channelList_(new QListWidget(this)),
      skillList_(new QListWidget(this)),
      remoteSkillList_(new QListWidget(this)),
      skillStatusLabel_(new QLabel(this)),
      skillRepoEdit_(new QLineEdit(config_.skillRepo, this)),
      fontSizeSpinBox_(new QSpinBox(this))
{
    setWindowTitle(QStringLiteral("设置"));
    setModal(true);

    auto *tabs = new QTabWidget(this);
    auto *channelPage = new QWidget(tabs);
    auto *channelLayout = new QVBoxLayout(channelPage);
    auto *channelButtons = new QHBoxLayout;
    auto *addButton = new QPushButton(QStringLiteral("添加"), this);
    auto *editButton = new QPushButton(QStringLiteral("编辑"), this);
    auto *deleteButton = new QPushButton(QStringLiteral("删除"), this);
    auto *testButton = new QPushButton(QStringLiteral("测试连接"), this);
    channelButtons->addWidget(addButton);
    channelButtons->addWidget(editButton);
    channelButtons->addWidget(deleteButton);
    channelButtons->addWidget(testButton);

    fontSizeSpinBox_->setRange(10, 24);
    fontSizeSpinBox_->setValue(config_.fontSize);
    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("聊天字体大小:"), fontSizeSpinBox_);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("完成"));
    connect(buttons, &QDialogButtonBox::accepted,
            this, &SettingsDialog::saveAndAccept);
    connect(addButton, &QPushButton::clicked,
            this, &SettingsDialog::addChannel);
    connect(editButton, &QPushButton::clicked,
            this, &SettingsDialog::editChannel);
    connect(deleteButton, &QPushButton::clicked,
            this, &SettingsDialog::deleteChannel);
    connect(testButton, &QPushButton::clicked,
            this, &SettingsDialog::testChannel);
    connect(channelList_, &QListWidget::itemDoubleClicked,
            this, [this]() { editChannel(); });

    channelLayout->addWidget(new QLabel(
        QStringLiteral("所有通道更改会立即保存。切换当前通道请使用主窗口顶部下拉框。"),
        channelPage));
    channelLayout->addWidget(channelList_, 1);
    channelLayout->addLayout(channelButtons);
    channelLayout->addLayout(form);
    tabs->addTab(channelPage, QStringLiteral("API 通道"));

    auto *skillPage = new QWidget(tabs);
    auto *skillLayout = new QVBoxLayout(skillPage);
    skillStatusLabel_->setWordWrap(true);
    skillLayout->addWidget(new QLabel(
        QStringLiteral("技能文件位于 skills/；导入后会校验 SKILL.md，不会执行技能脚本。"),
        skillPage));
    auto *repoLayout = new QHBoxLayout;
    repoLayout->addWidget(new QLabel(QStringLiteral("GitHub 仓库:"), skillPage));
    repoLayout->addWidget(skillRepoEdit_, 1);
    auto *saveRepoButton = new QPushButton(QStringLiteral("保存仓库"), skillPage);
    repoLayout->addWidget(saveRepoButton);
    skillLayout->addLayout(repoLayout);
    auto *repoHelp = new QLabel(
        QStringLiteral("仓库需提供 skills/index.json；默认 OWNER/REPOSITORY 是占位地址，请替换为你的仓库。"),
        skillPage);
    repoHelp->setWordWrap(true);
    skillLayout->addWidget(repoHelp);
    skillLayout->addWidget(skillList_, 1);
    skillLayout->addWidget(skillStatusLabel_);
    auto *skillButtons = new QHBoxLayout;
    auto *refreshSkillsButton = new QPushButton(QStringLiteral("刷新"), skillPage);
    auto *editSkillButton = new QPushButton(QStringLiteral("用记事本编辑"), skillPage);
    auto *importSkillButton = new QPushButton(QStringLiteral("导入技能"), skillPage);
    auto *importMenu = new QMenu(importSkillButton);
    importMenu->addAction(QStringLiteral("从 ZIP 导入"), this,
                          &SettingsDialog::importSkillFromZip);
    importMenu->addAction(QStringLiteral("从文件夹导入"), this,
                          &SettingsDialog::importSkillFromFolder);
    importSkillButton->setMenu(importMenu);
    auto *deleteSkillButton = new QPushButton(QStringLiteral("删除技能"), skillPage);
    skillButtons->addWidget(importSkillButton);
    skillButtons->addWidget(deleteSkillButton);
    skillButtons->addWidget(refreshSkillsButton);
    skillButtons->addWidget(editSkillButton);
    skillLayout->addLayout(skillButtons);
    skillLayout->addWidget(new QLabel(QStringLiteral("GitHub 可安装/可更新技能："),
                                      skillPage));
    skillLayout->addWidget(remoteSkillList_, 1);
    auto *remoteButtons = new QHBoxLayout;
    auto *checkUpdatesButton = new QPushButton(QStringLiteral("检查更新"), skillPage);
    auto *installSkillsButton = new QPushButton(QStringLiteral("安装所选"), skillPage);
    remoteButtons->addStretch();
    remoteButtons->addWidget(checkUpdatesButton);
    remoteButtons->addWidget(installSkillsButton);
    skillLayout->addLayout(remoteButtons);
    connect(saveRepoButton, &QPushButton::clicked,
            this, &SettingsDialog::saveSkillRepository);
    connect(refreshSkillsButton, &QPushButton::clicked,
            this, &SettingsDialog::refreshSkills);
    connect(editSkillButton, &QPushButton::clicked,
            this, &SettingsDialog::editSkill);
    connect(deleteSkillButton, &QPushButton::clicked,
            this, &SettingsDialog::deleteSkill);
    connect(checkUpdatesButton, &QPushButton::clicked,
            this, &SettingsDialog::checkSkillUpdates);
    connect(installSkillsButton, &QPushButton::clicked,
            this, &SettingsDialog::installSelectedSkills);
    connect(skillRepoEdit_, &QLineEdit::editingFinished,
            this, &SettingsDialog::saveSkillRepository);
    connect(skillList_, &QListWidget::itemDoubleClicked,
            this, [this]() { editSkill(); });
    tabs->addTab(skillPage, QStringLiteral("技能管理"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(tabs, 1);
    refreshChannels();
    refreshSkills();
    layout->addWidget(buttons);
    resize(760, 700);
}

AppConfig SettingsDialog::config() const
{
    AppConfig result = config_;
    result.fontSize = fontSizeSpinBox_->value();
    result.skillRepo = skillRepoEdit_->text().trimmed();
    return result;
}

QString SettingsDialog::configFilePath()
{
    return QStringLiteral("C:/MyBuddy/config.json");
}

bool SettingsDialog::saveConfig(const AppConfig &values, QString *errorMessage)
{
    AppConfig saved = values;
    if (saved.channels.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("至少需要保留一个 API 通道。");
        return false;
    }

    int selectedIndex = -1;
    QJsonArray channels;
    for (int i = 0; i < saved.channels.size(); ++i) {
        ApiChannel &channel = saved.channels[i];
        channel.id = channel.id.trimmed();
        if (channel.id.isEmpty())
            channel.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        QString validationError;
        if (!validChannel(channel, &validationError)) {
            if (errorMessage)
                *errorMessage = validationError;
            return false;
        }
        for (int j = 0; j < i; ++j) {
            if (saved.channels[j].id == channel.id) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("通道 ID 重复，无法保存配置。");
                return false;
            }
        }
        if (channel.id == saved.selectedChannelId)
            selectedIndex = i;
        channels.append(channelToJson(channel));
    }
    if (selectedIndex < 0)
        selectedIndex = 0;
    saved.selectedChannelId = saved.channels.at(selectedIndex).id;
    saved.apiBase = saved.channels.at(selectedIndex).apiBase;
    saved.apiKey = saved.channels.at(selectedIndex).apiKey;
    saved.model = saved.channels.at(selectedIndex).model;

    const QJsonObject object{
        {QStringLiteral("api_base"), saved.apiBase},
        {QStringLiteral("api_key"), saved.apiKey},
        {QStringLiteral("model"), saved.model},
        {QStringLiteral("skill_repo"), saved.skillRepo},
        {QStringLiteral("channels"), channels},
        {QStringLiteral("selected_channel_id"), saved.selectedChannelId},
        {QStringLiteral("font_size"), saved.fontSize},
        {QStringLiteral("total_tokens"), static_cast<qint64>(saved.totalTokens)},
        {QStringLiteral("search_count"), static_cast<qint64>(saved.searchCount)},
        {QStringLiteral("computer_control_enabled"), saved.computerControlEnabled},
        {QStringLiteral("web_search_enabled"), saved.webSearchEnabled},
        {QStringLiteral("task_mode_enabled"), saved.taskModeEnabled}
    };
    const QString path = configFilePath();
    const QString backupPath = path + QStringLiteral(".bak");
    if (QFileInfo::exists(path)) {
        QFile source(path);
        if (!source.open(QIODevice::ReadOnly)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("无法读取现有配置以备份：%1")
                                    .arg(source.errorString());
            return false;
        }
        const QByteArray previousConfig = source.readAll();
        source.close();
        QSaveFile backup(backupPath);
        if (!backup.open(QIODevice::WriteOnly)
            || backup.write(previousConfig) != previousConfig.size()
            || !backup.commit()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("无法写入配置备份：%1")
                                    .arg(backup.errorString());
            return false;
        }
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Indented);
    if (file.write(json) != json.size() || !file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    return true;
}

bool SettingsDialog::loadConfig(AppConfig *config, QString *errorMessage)
{
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("配置文件格式无效：%1")
                                .arg(parseError.errorString());
        }
        return false;
    }

    const QJsonObject object = document.object();
    AppConfig loaded;
    if (object.contains(QStringLiteral("api_base"))
        && !object.value(QStringLiteral("api_base")).isString()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("配置项 api_base 必须是字符串。");
        return false;
    }
    if (object.contains(QStringLiteral("api_key"))
        && !object.value(QStringLiteral("api_key")).isString()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("配置项 api_key 必须是字符串。");
        return false;
    }
    if (object.contains(QStringLiteral("model"))
        && !object.value(QStringLiteral("model")).isString()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("配置项 model 必须是字符串。");
        return false;
    }
    if (object.contains(QStringLiteral("skill_repo"))
        && !object.value(QStringLiteral("skill_repo")).isString()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("配置项 skill_repo 必须是字符串。");
        return false;
    }
    const QJsonValue fontSize = object.value(QStringLiteral("font_size"));
    if (!fontSize.isUndefined()
        && (!fontSize.isDouble()
            || fontSize.toDouble() < 10
            || fontSize.toDouble() > 24
            || fontSize.toDouble() != static_cast<int>(fontSize.toDouble()))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("配置项 font_size 必须是 10 到 24 之间的整数。");
        return false;
    }

    loaded.apiBase = object.value(QStringLiteral("api_base"))
                         .toString(loaded.apiBase).trimmed();
    loaded.apiKey = object.value(QStringLiteral("api_key")).toString().trimmed();
    loaded.model = object.value(QStringLiteral("model"))
                       .toString(loaded.model).trimmed();
    loaded.skillRepo = object.value(QStringLiteral("skill_repo"))
                           .toString(loaded.skillRepo).trimmed();
    loaded.fontSize = fontSize.isUndefined()
        ? loaded.fontSize : fontSize.toInt();
    const QJsonValue totalTokens = object.value(QStringLiteral("total_tokens"));
    const QJsonValue searchCount = object.value(QStringLiteral("search_count"));
    if ((!totalTokens.isUndefined() && !totalTokens.isDouble())
        || (!searchCount.isUndefined() && !searchCount.isDouble())
        || totalTokens.toDouble() < 0 || searchCount.toDouble() < 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("配置项 total_tokens 和 search_count 必须是非负数字。");
        return false;
    }
    loaded.totalTokens = static_cast<quint64>(totalTokens.toDouble());
    loaded.searchCount = static_cast<quint64>(searchCount.toDouble());
    const QJsonValue computerControl =
        object.value(QStringLiteral("computer_control_enabled"));
    if (!computerControl.isUndefined() && !computerControl.isBool()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("配置项 computer_control_enabled 必须是布尔值。");
        return false;
    }
    loaded.computerControlEnabled = computerControl.isUndefined()
        ? true : computerControl.toBool();
    const QJsonValue webSearch = object.value(QStringLiteral("web_search_enabled"));
    if (!webSearch.isUndefined() && !webSearch.isBool()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("配置项 web_search_enabled 必须是布尔值。");
        return false;
    }
    loaded.webSearchEnabled = webSearch.isUndefined() ? false : webSearch.toBool();
    const QJsonValue taskMode = object.value(QStringLiteral("task_mode_enabled"));
    if (!taskMode.isUndefined() && !taskMode.isBool()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("配置项 task_mode_enabled 必须是布尔值。");
        return false;
    }
    loaded.taskModeEnabled = taskMode.isUndefined() ? false : taskMode.toBool();

    const QJsonValue channelValues = object.value(QStringLiteral("channels"));
    if (!channelValues.isUndefined()) {
        if (!channelValues.isArray()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("配置项 channels 必须是数组。");
            return false;
        }
        const QJsonArray channelArray = channelValues.toArray();
        for (const QJsonValue &value : channelArray) {
            if (!value.isObject()) {
                if (errorMessage)
                    *errorMessage = QStringLiteral("通道配置必须是对象。");
                return false;
            }
            const QJsonObject entry = value.toObject();
            const QStringList stringKeys{
                QStringLiteral("id"), QStringLiteral("display_name"),
                QStringLiteral("base_url"), QStringLiteral("api_key"),
                QStringLiteral("model")
            };
            for (const QString &key : stringKeys) {
                if (!entry.value(key).isString()) {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("通道配置项 %1 必须是字符串。").arg(key);
                    return false;
                }
            }
            ApiChannel channel;
            channel.id = entry.value(QStringLiteral("id")).toString().trimmed();
            channel.displayName =
                entry.value(QStringLiteral("display_name")).toString().trimmed();
            channel.apiBase = entry.value(QStringLiteral("base_url")).toString().trimmed();
            channel.apiKey = entry.value(QStringLiteral("api_key")).toString().trimmed();
            channel.model = entry.value(QStringLiteral("model")).toString().trimmed();
            QString validationError;
            if (channel.id.isEmpty() || !validChannel(channel, &validationError)) {
                if (errorMessage)
                    *errorMessage = channel.id.isEmpty()
                        ? QStringLiteral("通道 ID 不能为空。") : validationError;
                return false;
            }
            for (const ApiChannel &existing : loaded.channels) {
                if (existing.id == channel.id) {
                    if (errorMessage)
                        *errorMessage = QStringLiteral("通道 ID 重复。");
                    return false;
                }
            }
            loaded.channels.append(channel);
        }
        if (loaded.channels.isEmpty()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("配置中至少需要一个 API 通道。");
            return false;
        }
        const QJsonValue selectedId = object.value(QStringLiteral("selected_channel_id"));
        if (!selectedId.isUndefined() && !selectedId.isString()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("配置项 selected_channel_id 必须是字符串。");
            return false;
        }
        loaded.selectedChannelId = selectedId.toString();
        int selectedIndex = -1;
        for (int i = 0; i < loaded.channels.size(); ++i) {
            if (loaded.channels.at(i).id == loaded.selectedChannelId) {
                selectedIndex = i;
                break;
            }
        }
        if (selectedIndex < 0) {
            selectedIndex = 0;
            loaded.selectedChannelId = loaded.channels.first().id;
        }
        loaded.apiBase = loaded.channels.at(selectedIndex).apiBase;
        loaded.apiKey = loaded.channels.at(selectedIndex).apiKey;
        loaded.model = loaded.channels.at(selectedIndex).model;
    } else if (!loaded.apiBase.isEmpty() && !loaded.apiKey.isEmpty()
               && !loaded.model.isEmpty()) {
        ApiChannel legacy;
        legacy.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        legacy.displayName = QUrl(loaded.apiBase).host();
        if (legacy.displayName.isEmpty())
            legacy.displayName = QStringLiteral("默认通道");
        legacy.apiBase = loaded.apiBase;
        legacy.apiKey = loaded.apiKey;
        legacy.model = loaded.model;
        loaded.channels.append(legacy);
        loaded.selectedChannelId = legacy.id;
    }
    *config = loaded;
    return true;
}

void SettingsDialog::saveAndAccept()
{
    config_.fontSize = fontSizeSpinBox_->value();
    config_.skillRepo = skillRepoEdit_->text().trimmed();
    QString errorMessage;
    if (!saveConfig(config_, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("保存失败"),
                              QStringLiteral("无法保存配置文件：%1").arg(errorMessage));
        return;
    }
    accept();
}

int SettingsDialog::selectedChannelIndex() const
{
    if (!channelList_->currentItem())
        return -1;
    return channelList_->currentItem()->data(Qt::UserRole).toInt();
}

void SettingsDialog::refreshChannels()
{
    channelList_->clear();
    int selectedRow = 0;
    for (int i = 0; i < config_.channels.size(); ++i) {
        const ApiChannel &channel = config_.channels.at(i);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1/%2").arg(channel.displayName, channel.model),
            channelList_);
        item->setData(Qt::UserRole, i);
        if (channel.id == config_.selectedChannelId)
            selectedRow = i;
    }
    if (channelList_->count() > 0)
        channelList_->setCurrentRow(selectedRow);
}

bool SettingsDialog::saveChannels(QString *errorMessage)
{
    if (!SettingsDialog::saveConfig(config_, errorMessage))
        return false;
    return true;
}

void SettingsDialog::addChannel()
{
    ApiChannel newChannel;
    newChannel.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    ChannelEditorDialog editor(newChannel, true, this);
    if (editor.exec() != QDialog::Accepted)
        return;
    newChannel = editor.channel();
    newChannel.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    config_.channels.append(newChannel);
    QString errorMessage;
    if (!saveChannels(&errorMessage)) {
        config_.channels.removeLast();
        QMessageBox::critical(this, QStringLiteral("保存失败"), errorMessage);
        return;
    }
    refreshChannels();
    channelList_->setCurrentRow(channelList_->count() - 1);
}

void SettingsDialog::editChannel()
{
    const int index = selectedChannelIndex();
    if (index < 0 || index >= config_.channels.size())
        return;
    const ApiChannel previous = config_.channels.at(index);
    ChannelEditorDialog editor(previous, false, this);
    if (editor.exec() != QDialog::Accepted)
        return;
    ApiChannel updated = editor.channel();
    updated.id = previous.id;
    config_.channels[index] = updated;
    QString errorMessage;
    if (!saveChannels(&errorMessage)) {
        config_.channels[index] = previous;
        QMessageBox::critical(this, QStringLiteral("保存失败"), errorMessage);
        return;
    }
    refreshChannels();
    channelList_->setCurrentRow(index);
}

void SettingsDialog::deleteChannel()
{
    const int index = selectedChannelIndex();
    if (index < 0 || index >= config_.channels.size())
        return;
    if (config_.channels.size() <= 1) {
        QMessageBox::warning(this, QStringLiteral("无法删除"),
                             QStringLiteral("至少保留一个通道，不能删除最后一个通道。"));
        return;
    }
    const ApiChannel channel = config_.channels.at(index);
    const QString previousSelectedChannelId = config_.selectedChannelId;
    if (QMessageBox::question(
            this, QStringLiteral("确认删除"),
            QStringLiteral("确定删除通道“%1/%2”吗？")
                .arg(channel.displayName, channel.model),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }

    config_.channels.removeAt(index);
    if (config_.selectedChannelId == channel.id)
        config_.selectedChannelId = config_.channels.first().id;
    QString errorMessage;
    if (!saveChannels(&errorMessage)) {
        config_.channels.insert(index, channel);
        config_.selectedChannelId = previousSelectedChannelId;
        QMessageBox::critical(this, QStringLiteral("保存失败"), errorMessage);
        return;
    }
    refreshChannels();
}

void SettingsDialog::testChannel()
{
    const int index = selectedChannelIndex();
    if (index < 0 || index >= config_.channels.size())
        return;
    const QString error = testChannelConnection(config_.channels.at(index));
    if (error.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("连接成功"),
                                 QStringLiteral("真实 API 请求成功，通道可用。"));
    } else {
        QMessageBox::warning(this, QStringLiteral("连接失败"), error);
    }
}

void SettingsDialog::refreshSkills()
{
    skillList_->clear();
    QVector<Skill> skills;
    QString errorMessage;
    const bool loaded = SkillManager::loadSkills(&skills, &errorMessage);
    for (const Skill &skill : skills) {
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  v%2  [%3]\n%4")
                .arg(skill.name, skill.version, sourceLabel(skill.source),
                     skill.description),
            skillList_);
        item->setData(Qt::UserRole, skill.filePath);
        item->setData(Qt::UserRole + 1, skill.source);
        item->setToolTip(skill.description);
    }

    if (!loaded) {
        skillStatusLabel_->setStyleSheet(QStringLiteral("color: #b00020;"));
        skillStatusLabel_->setText(QStringLiteral("部分技能读取失败：%1")
                                       .arg(errorMessage));
    } else if (skills.isEmpty()) {
        skillStatusLabel_->setStyleSheet(QStringLiteral("color: #777;"));
        skillStatusLabel_->setText(QStringLiteral("未找到技能。"));
    } else {
        skillStatusLabel_->setStyleSheet(QStringLiteral("color: #18794e;"));
        skillStatusLabel_->setText(QStringLiteral("已加载 %1 个技能。")
                                       .arg(skills.size()));
    }
}

void SettingsDialog::editSkill()
{
    if (!skillList_->currentItem()) {
        QMessageBox::information(this, QStringLiteral("技能管理"),
                                 QStringLiteral("请先选择一个技能。"));
        return;
    }
    const QString filePath =
        skillList_->currentItem()->data(Qt::UserRole).toString();
    if (!QFileInfo::exists(filePath)) {
        QMessageBox::critical(this, QStringLiteral("打开失败"),
                              QStringLiteral("技能文件不存在：%1").arg(filePath));
        return;
    }
    if (!QProcess::startDetached(QStringLiteral("notepad.exe"), {filePath})) {
        QMessageBox::critical(this, QStringLiteral("打开失败"),
                              QStringLiteral("无法启动记事本编辑技能文件。"));
    }
}

void SettingsDialog::importSkillFromZip()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("选择技能 ZIP 文件"), QString(),
        QStringLiteral("ZIP 文件 (*.zip)"));
    if (path.isEmpty())
        return;

    QStringList installedNames;
    QString errorMessage;
    if (!SkillManager::importSkillZip(path, &installedNames, &errorMessage)) {
        if (!installedNames.isEmpty()) {
            refreshSkills();
            errorMessage += QStringLiteral("\n已部分导入：%1")
                                .arg(installedNames.join(QStringLiteral("、")));
        }
        QMessageBox::warning(this, QStringLiteral("导入失败"), errorMessage);
        return;
    }
    refreshSkills();
    QMessageBox::information(
        this, QStringLiteral("导入成功"),
        QStringLiteral("已导入技能：%1").arg(installedNames.join(QStringLiteral("、"))));
}

void SettingsDialog::importSkillFromFolder()
{
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择包含 SKILL.md 的技能文件夹"));
    if (directory.isEmpty())
        return;

    QStringList installedNames;
    QString errorMessage;
    if (!SkillManager::importSkillDirectory(
            directory, QStringLiteral("imported"), {}, &installedNames,
            &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("导入失败"), errorMessage);
        return;
    }
    refreshSkills();
    QMessageBox::information(
        this, QStringLiteral("导入成功"),
        QStringLiteral("已导入技能：%1").arg(installedNames.join(QStringLiteral("、"))));
}

void SettingsDialog::deleteSkill()
{
    QListWidgetItem *item = skillList_->currentItem();
    if (!item) {
        QMessageBox::information(this, QStringLiteral("技能管理"),
                                 QStringLiteral("请先选择一个技能。"));
        return;
    }
    if (item->data(Qt::UserRole + 1).toString() == QStringLiteral("builtin")) {
        QMessageBox::warning(this, QStringLiteral("无法删除"),
                             QStringLiteral("内置技能不能删除。"));
        return;
    }
    const QString filePath = item->data(Qt::UserRole).toString();
    if (QMessageBox::question(
            this, QStringLiteral("确认删除"),
            QStringLiteral("确定删除技能“%1”及其全部文件吗？")
                .arg(item->text().section(QLatin1Char(' '), 0, 0)),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    QString errorMessage;
    if (!SkillManager::removeSkill(filePath, &errorMessage)) {
        QMessageBox::critical(this, QStringLiteral("删除失败"), errorMessage);
        return;
    }
    refreshSkills();
    QMessageBox::information(this, QStringLiteral("删除成功"),
                             QStringLiteral("所选技能已删除。"));
}

void SettingsDialog::saveSkillRepository()
{
    const QString previousRepository = config_.skillRepo;
    config_.skillRepo = skillRepoEdit_->text().trimmed();
    QString errorMessage;
    if (!saveConfig(config_, &errorMessage)) {
        config_.skillRepo = previousRepository;
        skillRepoEdit_->setText(previousRepository);
        QMessageBox::critical(
            this, QStringLiteral("保存失败"),
            QStringLiteral("无法保存技能仓库地址：%1").arg(errorMessage));
    }
}

bool SettingsDialog::repositoryLocation(QString *owner, QString *repository,
                                        QString *errorMessage) const
{
    const QUrl url(skillRepoEdit_->text().trimmed());
    QStringList parts = url.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
    if (!url.isValid() || url.scheme() != QStringLiteral("https")
        || url.host().compare(QStringLiteral("github.com"), Qt::CaseInsensitive) != 0
        || !url.userInfo().isEmpty() || !url.query().isEmpty()
        || !url.fragment().isEmpty() || parts.size() != 2) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "请填写有效的 GitHub 仓库 HTTPS 地址，例如 https://github.com/owner/repository。");
        return false;
    }
    if (parts.last().endsWith(QStringLiteral(".git"), Qt::CaseInsensitive))
        parts.last().chop(4);
    static const QRegularExpression segmentPattern(
        QStringLiteral("^[A-Za-z0-9_.-]+$"));
    if (!segmentPattern.match(parts.at(0)).hasMatch()
        || !segmentPattern.match(parts.at(1)).hasMatch()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("仓库地址包含无效的所有者或仓库名称。");
        return false;
    }
    if (owner)
        *owner = parts.at(0);
    if (repository)
        *repository = parts.at(1);
    return true;
}

void SettingsDialog::checkSkillUpdates()
{
    saveSkillRepository();
    QString owner;
    QString repository;
    QString errorMessage;
    if (!repositoryLocation(&owner, &repository, &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("仓库地址无效"), errorMessage);
        return;
    }

    QUrl indexUrl;
    indexUrl.setScheme(QStringLiteral("https"));
    indexUrl.setHost(QStringLiteral("raw.githubusercontent.com"));
    indexUrl.setPath(QStringLiteral("/%1/%2/main/skills/index.json")
                         .arg(owner, repository));
    HttpResponse response = getHttpResponse(indexUrl);
    QString branch = QStringLiteral("main");
    if (response.statusCode == 404) {
        indexUrl.setPath(QStringLiteral("/%1/%2/master/skills/index.json")
                             .arg(owner, repository));
        response = getHttpResponse(indexUrl);
        branch = QStringLiteral("master");
    }
    if (!response.error.isEmpty()) {
        QMessageBox::warning(
            this, QStringLiteral("检查更新失败"),
            QStringLiteral("%1\n请确认仓库已公开且存在 skills/index.json。")
                .arg(response.error));
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
        QJsonDocument::fromJson(response.body, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        QMessageBox::warning(
            this, QStringLiteral("索引无效"),
            QStringLiteral("skills/index.json 不是有效的 JSON 对象：%1")
                .arg(parseError.errorString()));
        return;
    }
    const QJsonValue skillsValue =
        document.object().value(QStringLiteral("skills"));
    if (!skillsValue.isArray() && !skillsValue.isObject()) {
        QMessageBox::warning(
            this, QStringLiteral("索引无效"),
            QStringLiteral("索引必须包含 skills 数组或对象。"));
        return;
    }

    QVector<Skill> installedSkills;
    QString loadError;
    SkillManager::loadSkills(&installedSkills, &loadError);
    remoteSkillList_->clear();
    QSet<QString> seenNames;
    QStringList invalidEntries;
    const auto addRemoteSkill = [this, &installedSkills, &seenNames,
                                 &invalidEntries, &branch, &owner, &repository](
                                    const QString &name, const QJsonObject &entry) {
        const QString normalizedName = name.toCaseFolded();
        if (!validRemoteSkillName(name) || seenNames.contains(normalizedName)) {
            invalidEntries.append(name);
            return;
        }
        const QJsonValue versionValue = entry.value(QStringLiteral("version"));
        const QString version = versionValue.isString()
            ? versionValue.toString().trimmed()
            : versionValue.isDouble()
                ? QString::number(versionValue.toDouble(), 'g', 12) : QString();
        if (version.isEmpty()) {
            invalidEntries.append(name);
            return;
        }
        RemoteSkill remote;
        remote.name = name;
        remote.version = version;
        remote.branch = entry.value(QStringLiteral("branch")).toString(branch);
        const QJsonValue filesValue = entry.value(QStringLiteral("files"));
        if (!filesValue.isUndefined() && !filesValue.isArray()) {
            invalidEntries.append(name);
            return;
        }
        for (const QJsonValue &file : filesValue.toArray()) {
            if (!file.isString()) {
                invalidEntries.append(name);
                return;
            }
            remote.files.append(file.toString());
        }
        if (remote.branch.isEmpty()
            || remote.branch.contains(QStringLiteral(".."))
            || !QRegularExpression(QStringLiteral("^[A-Za-z0-9._/-]+$"))
                    .match(remote.branch).hasMatch()) {
            invalidEntries.append(name);
            return;
        }
        seenNames.insert(normalizedName);
        const Skill *local = nullptr;
        for (const Skill &skill : installedSkills) {
            if (skill.name.compare(name, Qt::CaseInsensitive) == 0) {
                local = &skill;
                break;
            }
        }
        if (local && !isRemoteVersionNewer(version, local->version))
            return;

        const QString label = local
            ? QStringLiteral("可更新：%1 → %2").arg(local->version, version)
            : QStringLiteral("可安装：%1").arg(version);
        auto *item = new QListWidgetItem(
            QStringLiteral("%1  %2").arg(name, label), remoteSkillList_);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        item->setData(Qt::UserRole, QJsonDocument(QJsonObject{
            {QStringLiteral("name"), remote.name},
            {QStringLiteral("version"), remote.version},
            {QStringLiteral("branch"), remote.branch},
            {QStringLiteral("files"), QJsonArray::fromStringList(remote.files)}
        }).toJson(QJsonDocument::Compact));
        item->setToolTip(QStringLiteral("来自 %1/%2").arg(owner, repository));
    };

    if (skillsValue.isArray()) {
        for (const QJsonValue &value : skillsValue.toArray()) {
            if (!value.isObject()) {
                invalidEntries.append(QStringLiteral("(非对象条目)"));
                continue;
            }
            const QJsonObject entry = value.toObject();
            addRemoteSkill(entry.value(QStringLiteral("name")).toString(), entry);
        }
    } else {
        const QJsonObject entries = skillsValue.toObject();
        for (auto it = entries.constBegin(); it != entries.constEnd(); ++it) {
            if (!it.value().isObject()) {
                invalidEntries.append(it.key());
                continue;
            }
            addRemoteSkill(it.key(), it.value().toObject());
        }
    }
    if (remoteSkillList_->count() == 0) {
        skillStatusLabel_->setStyleSheet(QStringLiteral("color: #777;"));
        skillStatusLabel_->setText(invalidEntries.isEmpty()
            ? QStringLiteral("仓库索引有效；当前没有可安装或可更新的技能。")
            : QStringLiteral("没有可安装或更新项；索引中有无效条目：%1")
                  .arg(invalidEntries.join(QStringLiteral("、"))));
    } else {
        skillStatusLabel_->setStyleSheet(
            invalidEntries.isEmpty() ? QStringLiteral("color: #18794e;")
                                     : QStringLiteral("color: #9a6700;"));
        skillStatusLabel_->setText(
            QStringLiteral("发现 %1 个可安装或更新技能。%2")
                .arg(remoteSkillList_->count())
                .arg(invalidEntries.isEmpty()
                    ? QString()
                    : QStringLiteral("已跳过无效条目：%1")
                          .arg(invalidEntries.join(QStringLiteral("、")))));
    }
}

bool SettingsDialog::downloadRemoteSkill(const RemoteSkill &skill,
                                         const QString &owner,
                                         const QString &repository,
                                         QString *stagedDirectory,
                                         QString *errorMessage)
{
    if (stagedDirectory)
        stagedDirectory->clear();
    QStringList files = skill.files;
    if (files.isEmpty()) {
        QUrl treeUrl;
        treeUrl.setScheme(QStringLiteral("https"));
        treeUrl.setHost(QStringLiteral("api.github.com"));
        treeUrl.setPath(QStringLiteral("/repos/%1/%2/git/trees/%3")
                            .arg(owner, repository, skill.branch));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("recursive"), QStringLiteral("1"));
        treeUrl.setQuery(query);
        const HttpResponse treeResponse = getHttpResponse(treeUrl);
        if (!treeResponse.error.isEmpty()) {
            if (errorMessage)
                *errorMessage = treeResponse.error;
            return false;
        }
        QJsonParseError parseError;
        const QJsonDocument treeDocument =
            QJsonDocument::fromJson(treeResponse.body, &parseError);
        if (parseError.error != QJsonParseError::NoError
            || !treeDocument.isObject()) {
            if (errorMessage)
                *errorMessage = QStringLiteral("GitHub 文件树响应无效：%1")
                                    .arg(parseError.errorString());
            return false;
        }
        const QString prefix = QStringLiteral("skills/%1/").arg(skill.name);
        const QJsonArray entries =
            treeDocument.object().value(QStringLiteral("tree")).toArray();
        for (const QJsonValue &value : entries) {
            const QJsonObject entry = value.toObject();
            const QString path = entry.value(QStringLiteral("path")).toString();
            if (entry.value(QStringLiteral("type")).toString()
                    == QStringLiteral("blob")
                && entry.value(QStringLiteral("mode")).toString()
                    != QStringLiteral("120000")
                && path.startsWith(prefix)) {
                files.append(path.mid(prefix.size()));
            }
        }
    }

    QStringList normalizedFiles;
    const QString indexedPrefix = QStringLiteral("skills/%1/").arg(skill.name);
    const QString namedPrefix = skill.name + QLatin1Char('/');
    for (QString file : files) {
        if (file.startsWith(indexedPrefix))
            file.remove(0, indexedPrefix.size());
        else if (file.startsWith(namedPrefix))
            file.remove(0, namedPrefix.size());
        if (!validSkillPath(file)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("技能索引包含不安全的文件路径：%1").arg(file);
            return false;
        }
        if (!normalizedFiles.contains(file))
            normalizedFiles.append(file);
    }
    if (normalizedFiles.size() > 200
        || !normalizedFiles.contains(QStringLiteral("SKILL.md"))) {
        if (errorMessage)
            *errorMessage = QStringLiteral(
                "技能文件清单无效：必须包含 SKILL.md，且不超过 200 个文件。");
        return false;
    }

    const QString skillsRoot = SkillManager::skillsDirectoryPath();
    if (!QDir().mkpath(skillsRoot)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法创建 skills 目录。");
        return false;
    }
    const QString stagePath = QDir(skillsRoot).filePath(
        QStringLiteral(".github-%1").arg(
            QUuid::createUuid().toString(QUuid::WithoutBraces)));
    if (!QDir().mkpath(stagePath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法创建 GitHub 技能暂存目录。");
        return false;
    }

    qint64 totalBytes = 0;
    for (const QString &file : normalizedFiles) {
        QUrl rawUrl;
        rawUrl.setScheme(QStringLiteral("https"));
        rawUrl.setHost(QStringLiteral("raw.githubusercontent.com"));
        rawUrl.setPath(QStringLiteral("/%1/%2/%3/skills/%4/%5")
                           .arg(owner, repository, skill.branch, skill.name, file));
        const HttpResponse response = getHttpResponse(rawUrl);
        if (!response.error.isEmpty()) {
            QDir(stagePath).removeRecursively();
            if (errorMessage)
                *errorMessage = QStringLiteral("下载 %1 失败：%2")
                                    .arg(file, response.error);
            return false;
        }
        totalBytes += response.body.size();
        if (totalBytes > 100 * 1024 * 1024) {
            QDir(stagePath).removeRecursively();
            if (errorMessage)
                *errorMessage = QStringLiteral("技能总大小超出 100MB 限制。");
            return false;
        }
        const QString targetPath = QDir(stagePath).filePath(file);
        if (!QDir().mkpath(QFileInfo(targetPath).absolutePath())) {
            QDir(stagePath).removeRecursively();
            if (errorMessage)
                *errorMessage = QStringLiteral("无法创建技能子目录：%1").arg(file);
            return false;
        }
        QSaveFile target(targetPath);
        if (!target.open(QIODevice::WriteOnly)
            || target.write(response.body) != response.body.size()
            || !target.commit()) {
            const QString writeError = target.errorString();
            QDir(stagePath).removeRecursively();
            if (errorMessage)
                *errorMessage = QStringLiteral("保存技能文件失败（%1）：%2")
                                    .arg(file, writeError);
            return false;
        }
    }
    if (stagedDirectory)
        *stagedDirectory = stagePath;
    return true;
}

bool SettingsDialog::reviewScripts(const QString &skillDirectory)
{
    QStringList scripts;
    const QString scriptsDirectory =
        QDir(skillDirectory).filePath(QStringLiteral("scripts"));
    if (QDir(scriptsDirectory).exists()) {
        QDirIterator iterator(scriptsDirectory, QDir::Files,
                              QDirIterator::Subdirectories);
        while (iterator.hasNext()) {
            iterator.next();
            scripts.append(iterator.filePath());
        }
    }
    if (scripts.isEmpty())
        return true;

    for (;;) {
        QMessageBox prompt(this);
        prompt.setIcon(QMessageBox::Warning);
        prompt.setWindowTitle(QStringLiteral("技能包含脚本"));
        prompt.setText(QStringLiteral("该技能包含脚本，建议先查看内容。"));
        QPushButton *viewButton =
            prompt.addButton(QStringLiteral("查看脚本"), QMessageBox::ActionRole);
        QPushButton *installButton =
            prompt.addButton(QStringLiteral("仍然安装"), QMessageBox::AcceptRole);
        prompt.addButton(QStringLiteral("取消"), QMessageBox::RejectRole);
        prompt.setDefaultButton(qobject_cast<QPushButton *>(prompt.buttons().last()));
        prompt.exec();
        if (prompt.clickedButton() == viewButton) {
            showSkillScript(scripts, this);
            continue;
        }
        return prompt.clickedButton() == installButton;
    }
}

void SettingsDialog::installSelectedSkills()
{
    QString owner;
    QString repository;
    QString errorMessage;
    if (!repositoryLocation(&owner, &repository, &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("仓库地址无效"), errorMessage);
        return;
    }

    QVector<Skill> installedSkills;
    QString loadError;
    SkillManager::loadSkills(&installedSkills, &loadError);
    QStringList results;
    int installedCount = 0;
    for (int i = 0; i < remoteSkillList_->count(); ++i) {
        QListWidgetItem *item = remoteSkillList_->item(i);
        if (item->checkState() != Qt::Checked)
            continue;
        QJsonParseError parseError;
        const QJsonDocument entry =
            QJsonDocument::fromJson(item->data(Qt::UserRole).toByteArray(),
                                    &parseError);
        if (parseError.error != QJsonParseError::NoError || !entry.isObject()) {
            results.append(QStringLiteral("%1：技能信息无效").arg(item->text()));
            continue;
        }
        const QJsonObject object = entry.object();
        RemoteSkill remote;
        remote.name = object.value(QStringLiteral("name")).toString();
        remote.version = object.value(QStringLiteral("version")).toString();
        remote.branch = object.value(QStringLiteral("branch")).toString();
        for (const QJsonValue &file : object.value(QStringLiteral("files")).toArray())
            remote.files.append(file.toString());

        QString stagedDirectory;
        if (!downloadRemoteSkill(remote, owner, repository,
                                 &stagedDirectory, &errorMessage)) {
            results.append(QStringLiteral("%1：%2").arg(remote.name, errorMessage));
            continue;
        }
        QFile skillFile(QDir(stagedDirectory).filePath(QStringLiteral("SKILL.md")));
        if (!skillFile.open(QIODevice::ReadOnly)) {
            QDir(stagedDirectory).removeRecursively();
            results.append(QStringLiteral("%1：无法读取 SKILL.md").arg(remote.name));
            continue;
        }
        const QString skillContent = QString::fromUtf8(skillFile.readAll());
        const QRegularExpression namePattern(
            QStringLiteral("(?:^|\\n)name\\s*:\\s*[\"']?([^\"'\\r\\n]+)"),
            QRegularExpression::CaseInsensitiveOption);
        const QRegularExpressionMatch nameMatch = namePattern.match(skillContent);
        if (!nameMatch.hasMatch()
            || nameMatch.captured(1).trimmed().compare(
                   remote.name, Qt::CaseInsensitive) != 0) {
            QDir(stagedDirectory).removeRecursively();
            results.append(QStringLiteral("%1：SKILL.md 名称与仓库索引不一致")
                               .arg(remote.name));
            continue;
        }
        if (!reviewScripts(stagedDirectory)) {
            QDir(stagedDirectory).removeRecursively();
            results.append(QStringLiteral("%1：用户取消安装").arg(remote.name));
            continue;
        }

        QString replaceSkillFilePath;
        for (const Skill &local : installedSkills) {
            if (local.name.compare(remote.name, Qt::CaseInsensitive) == 0) {
                replaceSkillFilePath = local.filePath;
                break;
            }
        }
        QStringList installedNames;
        const bool imported = SkillManager::importSkillDirectory(
            stagedDirectory, QStringLiteral("github"), remote.version,
            &installedNames, &errorMessage, replaceSkillFilePath);
        QDir(stagedDirectory).removeRecursively();
        if (!imported) {
            results.append(QStringLiteral("%1：%2").arg(remote.name, errorMessage));
            continue;
        }
        ++installedCount;
        results.append(QStringLiteral("%1：已安装 v%2")
                            .arg(remote.name, remote.version));
    }
    refreshSkills();
    QMessageBox::information(
        this, QStringLiteral("技能同步结果"),
        installedCount > 0
            ? QStringLiteral("成功安装或更新 %1 个技能。\n\n%2")
                  .arg(installedCount).arg(results.join(QLatin1Char('\n')))
            : QStringLiteral("没有技能安装成功。\n\n%1")
                  .arg(results.isEmpty()
                      ? QStringLiteral("请先勾选至少一个技能。")
                      : results.join(QLatin1Char('\n'))));
}
