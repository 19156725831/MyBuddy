# 技能仓库格式

在技能管理页填写 GitHub 仓库 HTTPS 地址后，WorkBuddy 会读取仓库中的
`skills/index.json`。索引支持以下格式：

```json
{
  "skills": [
    {
      "name": "photo-organizer",
      "version": "1.1.0",
      "branch": "main",
      "files": [
        "SKILL.md",
        "scripts/check-photos.ps1"
      ]
    }
  ]
}
```

仓库中的技能文件放在 `skills/<name>/` 下。`branch` 可省略（默认使用索引所在分支）；
`files` 可省略，省略时 WorkBuddy 会通过 GitHub 文件树读取技能目录。技能必须含
符合 WorkBuddy 格式的 `SKILL.md`，且其 YAML `name` 必须与索引一致。

下载技能包含 `scripts/` 时，WorkBuddy 会先提示查看脚本；脚本不会自动执行。
设置里的 `https://github.com/OWNER/REPOSITORY` 只是占位地址，请替换成实际仓库。
