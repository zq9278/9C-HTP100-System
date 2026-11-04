const fs = require('node:fs');
const path = require('node:path');
function clean(value) {
  const p = value && typeof value === 'object' ? value : {}, out = {};
  if (['main','screen'].includes(p.target)) out.target = p.target;
  if (['screen-new','screen-old'].includes(p.panel)) out.panel = p.panel;
  if (['1000','4000','8000'].includes(String(p.speed))) out.speed = String(p.speed);
  if (typeof p.serial === 'string' && /^\d{0,12}$/.test(p.serial)) out.serial = p.serial;
  if (['Release','Debug','Tuning'].includes(p.preset)) out.preset = p.preset;
  if (typeof p.rebuild === 'boolean') out.rebuild = p.rebuild;
  if (typeof p.jlink === 'string' && p.jlink.length < 4096) out.jlink = p.jlink;
  return out;
}
function read(file) {try {return clean(JSON.parse(fs.readFileSync(file, 'utf8')));} catch {return {};}}
function save(file, value) {
  fs.mkdirSync(path.dirname(file), {recursive:true});
  fs.writeFileSync(file + '.tmp', JSON.stringify(clean(value)), 'utf8');
  fs.renameSync(file + '.tmp', file);
}
module.exports = {clean, read, save};
