#!/bin/bash
cd "$(dirname "$0")" || exit 1
export PATH="$HOME/.local/pressure-tuner-node/bin:/opt/homebrew/bin:/usr/local/bin:$PATH"
# Reuse the local HTP100 toolchain when it is installed on this Mac.
if [[ -d "$HOME/.cache/htp100-build/venv/bin" ]]; then
  export PATH="$HOME/.cache/htp100-build/venv/bin:$PATH"
fi
for toolchain in "$HOME"/.cache/htp100-build/arm-gnu-toolchain-*-darwin-*-arm-none-eabi/bin; do
  if [[ -x "$toolchain/arm-none-eabi-gcc" ]]; then export PATH="$toolchain:$PATH"; break; fi
done
if [[ -z "${JLINK_GDB_SERVER:-}" ]]; then
  for server in "$HOME"/Applications/SEGGER/JLink*/JLinkGDBServerCLExe; do
    if [[ -x "$server" ]]; then export JLINK_GDB_SERVER="$server"; break; fi
  done
fi
unset ELECTRON_RUN_AS_NODE
if ! command -v npm >/dev/null; then
  echo '未找到 Node.js / npm。请先安装 Node.js。'
  read -r -p '按回车关闭窗口…' _
  exit 1
fi
if [[ ! -x node_modules/.bin/electron ]]; then
  npm ci || { echo '依赖安装失败。'; read -r -p '按回车关闭窗口…' _; exit 1; }
fi
npm start
result=$?
if [[ $result -ne 0 ]]; then read -r -p '启动失败，按回车关闭窗口…' _; fi
exit "$result"
