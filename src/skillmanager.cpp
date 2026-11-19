#include "skillmanager.h"

#include "settingsdialog.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QUuid>

namespace {

bool safeRelativePath(const QString &path)
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

QString quotedPowerShellString(QString value)
{
    value.replace(QLatin1Char('\''), QStringLiteral("''"));
    return QStringLiteral("'%1'").arg(value);
}

bool writeSourceMetadata(const QString &directory, const QString &source,
                         const QString &version, QString *errorMessage)
{
    const QJsonObject metadata{
        {QStringLiteral("source"), source},
        {QStringLiteral("version"), version}
    };
    QSaveFile file(QDir(directory).filePath(QStringLiteral(".workbuddy.json")));
    if (!file.open(QIODevice::WriteOnly)) {
        *errorMessage = file.errorString();
        return false;
    }
    const QByteArray json =
        QJsonDocument(metadata).toJson(QJsonDocument::Compact);
    if (file.write(json) != json.size() || !file.commit()) {
        *errorMessage = file.errorString();
        return false;
    }
    return true;
}

QString uniqueSkillDirectoryName(const QString &requestedName)
{
    QString base = requestedName.toLower();
    base.replace(QRegularExpression(QStringLiteral("[^a-z0-9._-]+")),
                 QStringLiteral("-"));
    base.remove(QRegularExpression(QStringLiteral("^[.-]+|[.-]+$")));
    if (base.isEmpty())
        base = QStringLiteral("skill");
    if (base.size() > 64)
        base.truncate(64);

    QDir root(SkillManager::skillsDirectoryPath());
    QString candidate = base;
    int suffix = 2;
    while (root.exists(candidate)) {
        candidate = QStringLiteral("%1-%2").arg(base).arg(suffix++);
    }
    return candidate;
}

bool copySkillDirectoryContents(const QString &sourceDirectory,
                                const QString &destinationDirectory,
                                QString *errorMessage)
{
    const QString canonicalSource =
        QFileInfo(sourceDirectory).canonicalFilePath();
    if (canonicalSource.isEmpty()) {
        *errorMessage = QStringLiteral("无法解析导入文件夹的真实路径。");
        return false;
    }
    const QDir source(canonicalSource);
    QDirIterator iterator(canonicalSource,
                          QDir::AllEntries | QDir::Hidden | QDir::System
                              | QDir::NoDotAndDotDot,
                          QDirIterator::Subdirectories);
    int fileCount = 0;
    qint64 totalBytes = 0;
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo entry = iterator.fileInfo();
        if (entry.isSymLink()) {
            *errorMessage = QStringLiteral("技能包包含符号链接，拒绝导入：%1")
                                .arg(entry.fileName());
            return false;
        }
        const QString relativePath = source.relativeFilePath(entry.filePath());
        if (!safeRelativePath(relativePath)) {
            *errorMessage = QStringLiteral("技能包包含不安全的路径。");
            return false;
        }
        const QString target =
            QDir(destinationDirectory).filePath(relativePath);
        if (entry.isDir()) {
            if (!QDir().mkpath(target)) {
                *errorMessage = QStringLiteral("无法创建技能子目录：%1").arg(relativePath);
                return false;
            }
            continue;
        }
        if (!entry.isFile() || ++fileCount > 500) {
            *errorMessage = QStringLiteral("技能包文件数量超出 500 个限制。");
            return false;
        }
        totalBytes += entry.size();
        if (totalBytes > 100 * 1024 * 1024) {
            *errorMessage = QStringLiteral("技能包总大小超出 100MB 限制。");
            return false;
        }
        if (!QDir().mkpath(QFileInfo(target).absolutePath())
            || !QFile::copy(entry.filePath(), target)) {
            *errorMessage = QStringLiteral("复制技能文件失败：%1").arg(relativePath);
            return false;
        }
    }
    return true;
}

QString yamlScalar(QString value)
{
    value = value.trimmed();
    if (value.size() >= 2
        && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
            || (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\''))))) {
        value = value.mid(1, value.size() - 2);
    }
    return value;
}

bool parseSkill(const QString &filePath, const QString &content,
                Skill *skill, QString *errorMessage)
{
    const QStringList lines = content.split(QLatin1Char('\n'));
    if (lines.isEmpty() || lines.first().trimmed() != QStringLiteral("---")) {
        *errorMessage = QStringLiteral("缺少 YAML 头部起始标记 ---。");
        return false;
    }

    int headerEnd = -1;
    for (int i = 1; i < lines.size(); ++i) {
        if (lines.at(i).trimmed() == QStringLiteral("---")) {
            headerEnd = i;
            break;
        }
    }
    if (headerEnd < 0) {
        *errorMessage = QStringLiteral("缺少 YAML 头部结束标记 ---。");
        return false;
    }

    QString name;
    QString description;
    QString version;
    QString source;
    for (int i = 1; i < headerEnd; ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const int separator = line.indexOf(QLatin1Char(':'));
        if (separator <= 0)
            continue;
        const QString key = line.left(separator).trimmed();
        const QString value = yamlScalar(line.mid(separator + 1));
        if (key == QStringLiteral("name"))
            name = value;
        else if (key == QStringLiteral("description"))
            description = value;
        else if (key == QStringLiteral("version"))
            version = value;
        else if (key == QStringLiteral("source"))
            source = value;
    }
    if (name.isEmpty() || description.isEmpty()) {
        *errorMessage = QStringLiteral("YAML 头部必须包含非空的 name 和 description。");
        return false;
    }

    const QRegularExpression triggerPattern(
        QStringLiteral("(?:触发关键词|触发词|关键词|trigger\\s+keywords?)"
                       "\\s*[：:]\\s*([^。\\n]+)"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch triggerMatch = triggerPattern.match(description);
    if (!triggerMatch.hasMatch()) {
        *errorMessage = QStringLiteral("description 必须包含“触发关键词：...”列表。");
        return false;
    }

    QStringList triggers = triggerMatch.captured(1).split(
        QRegularExpression(QStringLiteral("[、，,;；|]+")), Qt::SkipEmptyParts);
    for (QString &trigger : triggers)
        trigger = trigger.trimmed();
    triggers.removeAll(QString());
    if (triggers.isEmpty()) {
        *errorMessage = QStringLiteral("description 中没有有效的触发关键词。");
        return false;
    }

    skill->name = name;
    skill->description = description;
    skill->version = version.isEmpty() ? QStringLiteral("0.0.0") : version;
    skill->source = source;
    skill->triggerKeywords = triggers;
    skill->filePath = QFileInfo(filePath).absoluteFilePath();
    skill->content = content;
    return true;
}

} // namespace

QString SkillManager::skillsDirectoryPath()
{
    return QDir(QFileInfo(SettingsDialog::configFilePath()).absolutePath())
        .filePath(QStringLiteral("skills"));
}

bool SkillManager::loadSkills(QVector<Skill> *skills, QString *errorMessage)
{
    skills->clear();
    if (errorMessage)
        errorMessage->clear();

    const QString rootPath = skillsDirectoryPath();
    if (!QDir().mkpath(rootPath)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法创建技能目录：%1").arg(rootPath);
        return false;
    }

    QStringList errors;
    const QDir root(rootPath);
    const QFileInfoList directories = root.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const QFileInfo &directory : directories) {
        const QString filePath =
            QDir(directory.filePath()).filePath(QStringLiteral("SKILL.md"));
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly)) {
            errors.append(QStringLiteral("%1：无法读取 SKILL.md（%2）")
                              .arg(directory.fileName(), file.errorString()));
            continue;
        }

        const QString content = QString::fromUtf8(file.readAll());
        Skill skill;
        QString parseError;
        if (!parseSkill(filePath, content, &skill, &parseError)) {
            errors.append(QStringLiteral("%1：%2")
                              .arg(directory.fileName(), parseError));
            continue;
        }
        const QString metadataPath =
            QDir(directory.filePath()).filePath(QStringLiteral(".workbuddy.json"));
        QFile metadataFile(metadataPath);
        if (QFileInfo::exists(metadataPath)) {
            if (!metadataFile.open(QIODevice::ReadOnly)) {
                errors.append(QStringLiteral("%1：无法读取来源元数据（%2）")
                                  .arg(directory.fileName(), metadataFile.errorString()));
                continue;
            }
            QJsonParseError metadataError;
            const QJsonDocument metadata =
                QJsonDocument::fromJson(metadataFile.readAll(), &metadataError);
            if (metadataError.error != QJsonParseError::NoError
                || !metadata.isObject()) {
                const QString detail =
                    metadataError.error == QJsonParseError::NoError
                    ? QStringLiteral("根节点必须是对象")
                    : metadataError.errorString();
                errors.append(QStringLiteral("%1：来源元数据格式无效（%2）")
                                  .arg(directory.fileName(), detail));
                continue;
            }
            const QJsonObject object = metadata.object();
            const QJsonValue sourceValue = object.value(QStringLiteral("source"));
            const QJsonValue versionValue = object.value(QStringLiteral("version"));
            if ((!sourceValue.isUndefined() && !sourceValue.isString())
                || (!versionValue.isUndefined() && !versionValue.isString())) {
                errors.append(QStringLiteral("%1：来源元数据字段必须是字符串。")
                                  .arg(directory.fileName()));
                continue;
            }
            const QString source = sourceValue.toString();
            const QString version = versionValue.toString();
            if (!source.isEmpty())
                skill.source = source;
            if (!version.isEmpty())
                skill.version = version;
            if (!skill.source.isEmpty()
                && skill.source != QStringLiteral("builtin")
                && skill.source != QStringLiteral("imported")
                && skill.source != QStringLiteral("github")
                && skill.source != QStringLiteral("experience")) {
                errors.append(QStringLiteral("%1：不支持的技能来源“%2”。")
                                  .arg(directory.fileName(), skill.source));
                continue;
            }
        }
        if (skill.source.isEmpty()) {
            skill.source = directory.fileName() == QStringLiteral("photo-organizer")
                ? QStringLiteral("builtin") : QStringLiteral("imported");
        }
        skills->append(skill);
    }

    if (!errors.isEmpty() && errorMessage)
        *errorMessage = errors.join(QLatin1Char('\n'));
    return errors.isEmpty();
}

QVector<Skill> SkillManager::matchingSkills(const QVector<Skill> &skills,
                                            const QString &userInput)
{
    QVector<Skill> matches;
    for (const Skill &skill : skills) {
        for (const QString &keyword : skill.triggerKeywords) {
            if (!keyword.isEmpty()
                && userInput.contains(keyword, Qt::CaseInsensitive)) {
                matches.append(skill);
                break;
            }
        }
    }
    return matches;
}

bool SkillManager::importSkillDirectory(const QString &sourceDirectory,
                                        const QString &source,
                                        const QString &version,
                                        QStringList *installedNames,
                                        QString *errorMessage,
                                        const QString &replaceSkillFilePath)
{
    if (errorMessage)
        errorMessage->clear();
    if (installedNames)
        installedNames->clear();

    const QFileInfo sourceInfo(sourceDirectory);
    if (!sourceInfo.isDir() || sourceInfo.isSymLink()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("请选择有效的技能文件夹。");
        return false;
    }
    const QString skillFilePath =
        QDir(sourceDirectory).filePath(QStringLiteral("SKILL.md"));
    QFile skillFile(skillFilePath);
    if (!skillFile.open(QIODevice::ReadOnly)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("技能文件夹缺少可读的 SKILL.md：%1")
                                .arg(skillFile.errorString());
        return false;
    }
    const QString skillContent = QString::fromUtf8(skillFile.readAll());
    Skill skill;
    QString validationError;
    if (!parseSkill(skillFilePath, skillContent, &skill, &validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }

    const QString skillsRoot = skillsDirectoryPath();
    if (!QDir().mkpath(skillsRoot)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法创建 skills 目录。");
        return false;
    }
    QTemporaryDir staging(QDir(skillsRoot).filePath(QStringLiteral(".import-XXXXXX")));
    if (!staging.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法创建技能暂存目录。");
        return false;
    }
    if (!copySkillDirectoryContents(sourceDirectory, staging.path(), &validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }

    const QString installedVersion =
        version.trimmed().isEmpty() ? skill.version : version.trimmed();
    if (!writeSourceMetadata(staging.path(), source, installedVersion,
                             &validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }
    QString targetPath;
    if (replaceSkillFilePath.isEmpty()) {
        targetPath = QDir(skillsRoot).filePath(uniqueSkillDirectoryName(skill.name));
    } else {
        const QString rootPath = QFileInfo(skillsRoot).canonicalFilePath();
        const QString existingFile =
            QFileInfo(replaceSkillFilePath).canonicalFilePath();
        const QFileInfo existingInfo(existingFile);
        if (rootPath.isEmpty() || existingFile.isEmpty()
            || existingInfo.fileName() != QStringLiteral("SKILL.md")
            || QFileInfo(existingInfo.absolutePath()).canonicalPath() != rootPath) {
            if (errorMessage)
                *errorMessage = QStringLiteral("更新目标必须是 skills 目录中的现有技能。");
            return false;
        }
        targetPath = existingInfo.absolutePath();
    }

    QString backupPath;
    if (!replaceSkillFilePath.isEmpty()) {
        backupPath = targetPath + QStringLiteral(".update-backup-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
        if (!QDir().rename(targetPath, backupPath)) {
            if (errorMessage)
                *errorMessage = QStringLiteral("无法暂存现有技能以安全更新。");
            return false;
        }
    }
    if (!QDir().rename(staging.path(), targetPath)) {
        const bool restored = backupPath.isEmpty()
            || QDir().rename(backupPath, targetPath);
        if (errorMessage)
            *errorMessage = restored
                ? QStringLiteral("无法将技能安装到：%1").arg(targetPath)
                : QStringLiteral("无法安装更新，且无法还原原技能；原文件保留在：%1")
                      .arg(backupPath);
        return false;
    }
    if (!backupPath.isEmpty())
        QDir(backupPath).removeRecursively();
    if (installedNames)
        installedNames->append(skill.name);
    return true;
}

bool SkillManager::importSkillZip(const QString &zipPath,
                                  QStringList *installedNames,
                                  QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    if (installedNames)
        installedNames->clear();
    const QFileInfo zipInfo(zipPath);
    if (!zipInfo.isFile() || zipInfo.suffix().compare(
            QStringLiteral("zip"), Qt::CaseInsensitive) != 0
        || zipInfo.size() > 100 * 1024 * 1024) {
        if (errorMessage)
            *errorMessage = QStringLiteral("请选择不超过 100MB 的 ZIP 技能包。");
        return false;
    }

    QTemporaryDir extracted;
    if (!extracted.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法创建 ZIP 解压暂存目录。");
        return false;
    }
    const QString script = QStringLiteral(
        "$ErrorActionPreference='Stop'; "
        "Add-Type -AssemblyName System.IO.Compression.FileSystem; "
        "$archive=[IO.Compression.ZipFile]::OpenRead(%1); "
        "$root=[IO.Path]::GetFullPath(%2); "
        "$prefix=$root+[IO.Path]::DirectorySeparatorChar; "
        "try { "
        "if($archive.Entries.Count -gt 1000){throw 'ZIP 文件数量超出 1000 个限制'}; "
        "$total=0L; foreach($entry in $archive.Entries){ "
        "$total += $entry.Length; if($total -gt 104857600){throw 'ZIP 解压大小超出 100MB 限制'}; "
        "$mode=($entry.ExternalAttributes -shr 16) -band 0xF000; "
        "if($mode -eq 0xA000){throw 'ZIP 不允许包含符号链接'}; "
        "$relative=$entry.FullName.Replace('/',[IO.Path]::DirectorySeparatorChar).Replace('\\',[IO.Path]::DirectorySeparatorChar); "
        "$target=[IO.Path]::GetFullPath([IO.Path]::Combine($root,$relative)); "
        "if(-not $target.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'ZIP 包含越界路径'}; "
        "if($entry.FullName.EndsWith('/') -or $entry.FullName.EndsWith('\\')){"
        "New-Item -ItemType Directory -Force -Path $target | Out-Null; continue}; "
        "$parent=[IO.Path]::GetDirectoryName($target); "
        "New-Item -ItemType Directory -Force -Path $parent | Out-Null; "
        "$entryStream=$entry.Open(); $output=[IO.File]::Open($target,[IO.FileMode]::CreateNew); "
        "try{$entryStream.CopyTo($output)}finally{$entryStream.Dispose();$output.Dispose()} "
        "} } finally {$archive.Dispose()}").arg(
            quotedPowerShellString(QFileInfo(zipPath).absoluteFilePath()),
            quotedPowerShellString(extracted.path()));
    QProcess process;
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(QStringLiteral("powershell.exe"),
                  {QStringLiteral("-NoProfile"),
                   QStringLiteral("-NonInteractive"),
                   QStringLiteral("-Command"), script});
    if (!process.waitForStarted(5000)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法启动 ZIP 解压器：%1")
                                .arg(process.errorString());
        return false;
    }
    if (!process.waitForFinished(60000)) {
        process.kill();
        process.waitForFinished();
        if (errorMessage)
            *errorMessage = QStringLiteral("ZIP 解压超时：%1")
                                .arg(QString::fromLocal8Bit(process.readAll()).trimmed());
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ZIP 解压失败：%1")
                                .arg(QString::fromLocal8Bit(process.readAll()).trimmed());
        return false;
    }

    QStringList skillDirectories;
    QDirIterator iterator(extracted.path(), QDir::Files,
                          QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        const QFileInfo entry = iterator.fileInfo();
        if (!entry.isSymLink()
            && entry.fileName().compare(QStringLiteral("SKILL.md"),
                                        Qt::CaseInsensitive) == 0) {
            const QString directory = entry.absolutePath();
            if (!skillDirectories.contains(directory, Qt::CaseInsensitive))
                skillDirectories.append(directory);
        }
    }
    if (skillDirectories.isEmpty()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ZIP 中没有找到 SKILL.md。");
        return false;
    }
    if (skillDirectories.size() > 20) {
        if (errorMessage)
            *errorMessage = QStringLiteral("ZIP 中技能数量超过 20 个限制。");
        return false;
    }

    QStringList failures;
    for (const QString &directory : skillDirectories) {
        QStringList names;
        QString importError;
        if (importSkillDirectory(directory, QStringLiteral("imported"), {},
                                 &names, &importError)) {
            if (installedNames)
                installedNames->append(names);
        } else {
            failures.append(QStringLiteral("%1：%2")
                                .arg(QFileInfo(directory).fileName(), importError));
        }
    }
    if (!failures.isEmpty() && errorMessage)
        *errorMessage = failures.join(QLatin1Char('\n'));
    return failures.isEmpty();
}

bool SkillManager::saveSkillDraft(const QString &content, QString *savedName,
                                  QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    QString skillContent = content.trimmed();
    const QRegularExpression markdownFence(
        QStringLiteral("^```(?:markdown|md)?\\s*\\n([\\s\\S]*?)\\n```$"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = markdownFence.match(skillContent);
    if (match.hasMatch())
        skillContent = match.captured(1);

    Skill skill;
    QString validationError;
    if (!parseSkill(QStringLiteral("generated/SKILL.md"), skillContent,
                    &skill, &validationError)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("技能草稿格式无效：%1").arg(validationError);
        return false;
    }
    const QString root = skillsDirectoryPath();
    if (!QDir().mkpath(root)) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法创建 skills 目录。");
        return false;
    }
    QTemporaryDir staging(QDir(root).filePath(QStringLiteral(".draft-XXXXXX")));
    if (!staging.isValid()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法创建技能草稿暂存目录。");
        return false;
    }
    QSaveFile file(QDir(staging.path()).filePath(QStringLiteral("SKILL.md")));
    if (!file.open(QIODevice::WriteOnly)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QByteArray bytes = skillContent.toUtf8();
    if (file.write(bytes) != bytes.size() || !file.commit()) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QString version = skill.version == QStringLiteral("0.0.0")
        ? QStringLiteral("1.0.0") : skill.version;
    if (!writeSourceMetadata(staging.path(), QStringLiteral("experience"),
                             version, &validationError)) {
        if (errorMessage)
            *errorMessage = validationError;
        return false;
    }
    const QString directoryName = uniqueSkillDirectoryName(skill.name);
    if (!QDir().rename(staging.path(), QDir(root).filePath(directoryName))) {
        if (errorMessage)
            *errorMessage = QStringLiteral("无法保存技能草稿：%1").arg(directoryName);
        return false;
    }
    if (savedName)
        *savedName = skill.name;
    return true;
}

bool SkillManager::removeSkill(const QString &skillFilePath,
                               QString *errorMessage)
{
    if (errorMessage)
        errorMessage->clear();
    const QString root = QFileInfo(skillsDirectoryPath()).canonicalFilePath();
    const QFileInfo skillFile(skillFilePath);
    const QString skillDirectory =
        QFileInfo(skillFile.absolutePath()).canonicalFilePath();
    if (root.isEmpty() || skillFile.fileName().compare(
            QStringLiteral("SKILL.md"), Qt::CaseInsensitive) != 0
        || skillFile.isSymLink() || skillDirectory.isEmpty()
        || QFileInfo(skillDirectory).isSymLink()
        || QFileInfo(skillDirectory).dir().canonicalPath() != root) {
        if (errorMessage)
            *errorMessage = QStringLiteral("拒绝删除 skills 目录之外的路径。");
        return false;
    }
    QFile metadataFile(
        QDir(skillDirectory).filePath(QStringLiteral(".workbuddy.json")));
    if (metadataFile.open(QIODevice::ReadOnly)) {
        QJsonParseError parseError;
        const QJsonDocument metadata =
            QJsonDocument::fromJson(metadataFile.readAll(), &parseError);
        metadataFile.close();
        if (parseError.error == QJsonParseError::NoError && metadata.isObject()
            && metadata.object().value(QStringLiteral("source")).toString()
                == QStringLiteral("builtin")) {
            if (errorMessage)
                *errorMessage = QStringLiteral("内置技能不可删除。");
            return false;
        }
    }
    if (QFileInfo(skillDirectory).fileName() == QStringLiteral("photo-organizer")) {
        if (errorMessage)
            *errorMessage = QStringLiteral("内置技能不可删除。");
        return false;
    }
    if (!QDir(skillDirectory).removeRecursively()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("删除技能文件夹失败。");
        return false;
    }
    return true;
}
