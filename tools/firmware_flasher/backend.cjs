const fs = require('node:fs');
const path = require('node:path');
const os = require('node:os');
const crypto = require('node:crypto');
const {spawn} = require('node:child_process');
const ROOT = path.resolve(__dirname, '../..');
const ROLES = ['main', 'screen-new', 'screen-old', 'screen-nov11'];
const PRESETS = ['Release', 'Debug', 'Tuning'];
const LOADER = 'CLK@PF10_nCS@PB6_D0@PF8_D1@PF9_D2@PF7_D3@PF6';
const hash = file => crypto.createHash('sha256').update(fs.readFileSync(file)).digest('hex');

function validate(options) {
  if (!options || !ROLES.includes(options.role) || !PRESETS.includes(options.preset)) throw Error('请选择有效的板型和构建版本');
  if (options.serial && !/^\d{1,12}$/.test(options.serial)) throw Error('探针序列号只能填写数字');
  if (options.speed && !['1000', '4000', '8000'].includes(String(options.speed))) throw Error('无效的 SWD 速度');
}

function findJLink() {
  const candidates = [process.env.JLINK_COMMANDER];
  for (const base of [path.join(process.env.ProgramFiles || 'C:/Program Files', 'SEGGER'), path.join(process.env.LOCALAPPDATA || '', 'stm32cube/bundles/jlink-gdbserver')]) {
    if (fs.existsSync(base)) for (const dir of fs.readdirSync(base).sort().reverse()) {
      candidates.push(path.join(base, dir, 'JLink.exe'), path.join(base, dir, 'bin/JLink.exe'));
    }
  }
  for (const dir of (process.env.PATH || '').split(path.delimiter)) candidates.push(path.join(dir, process.platform === 'win32' ? 'JLink.exe' : 'JLinkExe'));
  return candidates.find(p => p && fs.existsSync(p)) || '';
}

function run(command, args, {cwd = ROOT, log = () => {}, timeout = 900000} = {}) {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, {cwd, windowsHide: true, shell: false, stdio: ['ignore', 'pipe', 'pipe']});
    let output = '', finished = false, timedOut = false;
    const timer = setTimeout(() => {timedOut = true; child.kill();}, timeout);
    const data = chunk => {const s = chunk.toString(); output = (output + s).slice(-300000); log(s);};
    child.stdout.on('data', data); child.stderr.on('data', data);
    child.once('error', error => {finished = true; clearTimeout(timer); reject(error);});
    child.once('close', code => {
      clearTimeout(timer); if (finished) return;
      if (timedOut) reject(Error('操作超时，未确认成功，请查看日志'));
      else if (code !== 0) reject(Error(`进程失败（${code}）\n${output.slice(-1800)}`));
      else resolve(output);
    });
  });
}

function commandScript(role) {
  if (!ROLES.includes(role)) throw Error('Invalid target');
  return ['ExitOnError 1', 'exec DisableAutoUpdateFW',
    ...(role === 'main' ? [] : [`exec DEVICE_SelectLoader BankAddr=0x90000000 Loader=${LOADER}`]),
    'connect', 'r', 'h', 'loadfile "firmware.hex"', 'r', 'g', 'q', ''].join('\n');
}

function commandArgs(options, script) {
  validate(options);
  return ['-NoGui', '1', '-ExitOnError', '1', '-AutoConnect', '0', '-Device', options.role === 'main' ? 'STM32G070CB' : 'STM32H750XB',
    '-If', 'SWD', '-Speed', String(options.speed || 1000), ...(options.serial ? ['-USB', options.serial] : []), '-CommandFile', script];
}

class Programmer {
  constructor({runner = run, root = ROOT, bundleRoot = null} = {}) {
    this.runner = runner; this.root = root; this.bundleRoot = bundleRoot; this.busy = false;
    this.bundle = bundleRoot ? JSON.parse(fs.readFileSync(path.join(bundleRoot, 'bundle.json'), 'utf8')) : null;
  }
  bundledFile(relative) {
    const file = path.resolve(this.bundleRoot, relative);
    if (!file.startsWith(path.resolve(this.bundleRoot) + path.sep)) throw Error('无效的软件包路径');
    return file;
  }
  verifyRuntime() {
    if (!this.bundle) return;
    if (!this.bundle.runtime || !this.bundle.runtime['jlink/JLink.exe']) throw Error('软件包缺少 J-Link 清单');
    for (const [file, expected] of Object.entries(this.bundle.runtime)) {
      if (hash(this.bundledFile(file)) !== expected) throw Error(`J-Link 文件损坏，请重新解压软件包：${file}`);
    }
  }
  async exclusive(fn) {
    if (this.busy) throw Error('当前任务尚未完成');
    this.busy = true;
    try {return await fn();} finally {this.busy = false;}
  }
  python() {return process.env.PYTHON || (process.platform === 'win32' ? 'python' : 'python3');}
  async inspect(options) {
    validate(options);
    if (this.bundle) {
      if (options.preset !== 'Release' && !(options.role === 'main' && options.preset === 'Tuning' && this.bundle.tuning)) throw Error('分发版只提供内置 Release 固件');
      const tuning = options.role === 'main' && options.preset === 'Tuning';
      const info = tuning ? this.bundle.tuning : this.bundle.images[options.role];
      if (!info || info.role !== options.role || info.preset !== options.preset || info.file !== (tuning ? 'main-tuning.hex' : options.role + '.hex')
          || info.lcd_panel !== (options.role === 'main' ? null : options.role.split('-')[1].toUpperCase())) throw Error('内置固件清单与板型不匹配');
      const file = this.bundledFile('firmware/' + info.file);
      if (hash(file) !== info.sha256) throw Error('内置固件损坏，请重新解压软件包');
      return {...info, path: file};
    }
    const output = await this.runner(this.python(), ['scripts/package_info.py', '--role', options.role, '--preset', options.preset], {cwd: this.root, timeout: 120000});
    const info = JSON.parse(output);
    if (info.role !== options.role || info.preset !== options.preset) throw Error('固件类型不匹配');
    return info;
  }
  async compile(options, log) {
    if (this.bundle) throw Error('分发版使用内置固件，不支持编译');
    validate(options);
    log(`开始构建：${options.role} / ${options.preset}\n`);
    await this.runner(this.python(), ['scripts/build.py', '--board', options.role, '--preset', options.preset], {cwd: this.root, log});
    return this.inspect(options);
  }
  build(options, log = () => {}) {return this.exclusive(() => this.compile(options, log));}
  flash(options, log = () => {}, progress = () => {}) {
    return this.exclusive(async () => {
      progress(1);
      validate(options);
      if (this.bundle && options.rebuild) throw Error('分发版不能重新编译，请使用内置固件');
      this.verifyRuntime();
      const executable = this.bundle ? this.bundledFile('jlink/JLink.exe') : options.jlink || findJLink();
      if (!executable || !fs.existsSync(executable)) throw Error('未找到 J-Link Commander，请安装 SEGGER J-Link 或选择 JLink.exe');
      const info = options.rebuild ? await this.compile(options, log) : await this.inspect(options);
      const folder = fs.mkdtempSync(path.join(os.tmpdir(), 'htp-flash-'));
      try {
        // Freeze exactly the validated bytes for this operation, even if a build runs elsewhere.
        const firmware = path.join(folder, 'firmware.hex'); fs.copyFileSync(info.path, firmware);
        if (hash(firmware) !== info.sha256) throw Error('固件在检查后发生变化，请重新构建');
        const script = path.join(folder, 'program.jlink'); fs.writeFileSync(script, commandScript(options.role), 'ascii');
        log(`开始下载并校验：${options.role}\nSHA-256 ${info.sha256}\n`);
        progress(2);
        let tail = '', downloading = false;
        const downloadLog = text => {
          tail = (tail + text).slice(-4096);
          if (!downloading && /Downloading file/i.test(tail)) { downloading = true; progress(3); }
          log(text);
        };
        const output = await this.runner(executable, commandArgs(options, script), {cwd: folder, log: downloadLog});
        if (!/Downloading file[^]*?\bO\.K\./i.test(output) || /(?:^|\n)\s*(?:ERROR\s*[:!]|Cannot connect|Failed to)/i.test(output)) throw Error('未收到完整下载成功回执，请检查 J-Link 日志');
        log('下载及 J-Link 内置校验完成，已请求复位运行。\n');
        return info;
      } finally {
        // Delete only the two known files in the unique directory we created.
        for (const name of ['firmware.hex', 'program.jlink']) fs.rmSync(path.join(folder, name), {force: true});
        try {fs.rmdirSync(folder);} catch {} // Preserve any extra diagnostic logs.
      }
    });
  }
}
module.exports = {Programmer, findJLink, validate, commandScript, commandArgs, ROOT, ROLES, PRESETS, LOADER};
