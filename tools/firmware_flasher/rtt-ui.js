const $=id=>document.getElementById(id);
let connected=false,busy=false,paused=false,readIndex=-1;
const fields=['fast','retract','kp','ratio'];
function render(){for(const id of ['connect','tuning','back'])$(id).disabled=busy||connected;for(const id of ['read','disconnect'])$(id).disabled=busy||!connected;$('write').disabled=busy||!connected||readIndex!==Number($('profile').value);$('profile').disabled=busy;for(const f of fields)$(f).disabled=busy||!connected;}
function log(text){if(paused)return;const p=$('rtt-log');p.textContent=(p.textContent+text).slice(-100000);p.scrollTop=p.scrollHeight;}
async function action(fn){if(busy)return;busy=true;render();try{await fn();}catch(e){$('result').textContent=e.message;log(e.message+'\n');}finally{busy=false;render();}}
function values(v){fields.forEach((f,i)=>$(f).value=Number(v[i].toPrecision(7)));}
$('connect').onclick=()=>action(async()=>{const info=await flasher.rtt('connect');connected=true;$('state').textContent='已连接 · 固件 '+info.firmware;readIndex=-1;});
$('disconnect').onclick=()=>action(async()=>{await flasher.rtt('disconnect');connected=false;readIndex=-1;$('state').textContent='已断开，恢复默认';});
$('read').onclick=()=>action(async()=>{const index=Number($('profile').value);values(await flasher.rtt('read',[index]));readIndex=index;$('result').textContent='已读取';});
$('write').onclick=()=>action(async()=>{values(await flasher.rtt('write',[Number($('profile').value),fields.map(f=>Number($(f).value))]));$('result').textContent='固件已确认，临时参数已应用';});
$('profile').onchange=()=>{readIndex=-1;fields.forEach(f=>$(f).value='');render();};
$('tuning').onclick=()=>{if(confirm('请先停止治疗。将烧录主机 Tuning 调试固件，继续？'))action(async()=>{await flasher.rtt('flash');$('result').textContent='调试固件烧录成功，可连接 RTT';});};
$('back').onclick=()=>flasher.rtt('back');
$('clear').onclick=()=>$('rtt-log').textContent='';$('pause').onclick=()=>{paused=!paused;$('pause').textContent=paused?'继续显示':'暂停显示';};
flasher.onRtt((event,data)=>{if(event==='log')log(data);if(event==='state'&&!data){connected=false;readIndex=-1;$('state').textContent='未连接';render();}});render();
