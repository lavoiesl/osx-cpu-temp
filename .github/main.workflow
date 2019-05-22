workflow "Pull Request Linter" {
  on = "pull_request"
  resolves = ["clang-format"]
}

action "clang-format" {
  uses = "docker://saschpe/clang-format"
  runs = "bin/lint.sh"
}
