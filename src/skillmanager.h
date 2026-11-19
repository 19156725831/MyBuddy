#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

struct Skill
{
    QString name;
    QString description;
    QString version;
    QString source;
    QStringList triggerKeywords;
    QString filePath;
    QString content;
};

class SkillManager
{
public:
    static QString skillsDirectoryPath();
    static bool loadSkills(QVector<Skill> *skills, QString *errorMessage = nullptr);
    static QVector<Skill> matchingSkills(const QVector<Skill> &skills,
                                         const QString &userInput);
    static bool importSkillDirectory(const QString &sourceDirectory,
                                     const QString &source,
                                     const QString &version,
                                     QStringList *installedNames,
                                     QString *errorMessage,
                                     const QString &replaceSkillFilePath = QString());
    static bool importSkillZip(const QString &zipPath,
                               QStringList *installedNames,
                               QString *errorMessage);
    static bool saveSkillDraft(const QString &content, QString *savedName,
                               QString *errorMessage);
    static bool removeSkill(const QString &skillFilePath, QString *errorMessage);
};
