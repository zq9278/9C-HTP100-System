const $=id=>document.getElementById(id);let busy=false,isConnected=false,monitoring=false,monitorTimer=null;
const labels={title:'J-Link \u538b\u529b\u5b9e\u65f6\u8c03\u53c2',connect:'\u8fde\u63a5 J-Link',disconnect:'\u505c\u6b62\u8c03\u8bd5\u5e76\u6062\u590d\u9ed8\u8ba4',fastLabel:'\u63a8\u8fdb\u901f\u5ea6',retractLabel:'\u56de\u9000\u901f\u5ea6',ratioLabel:'\u51cf\u901f\u6bd4\u4f8b',write:'\u53d1\u9001\u53c2\u6570',note:'\u53d1\u9001\u540e\u7acb\u5373\u751f\u6548\uff0c\u4e0d\u4fdd\u5b58\u3002\u6b63\u5e38\u65ad\u5f00\u6216\u5173\u95ed\u65f6\u6062\u590d\u9ed8\u8ba4\u3002'};
for(const [id,text] of Object.entries(labels))$(id).textContent=text;
function state(connected){isConnected=connected;if(!connected)stopMonitor();$('monitor').disabled=!connected;$('connect').disabled=connected;$('flash').disabled=connected;$('write').disabled=!connected;$('read').disabled=!connected;$('disconnect').disabled=!connected;$('status').textContent=connected?'J-Link \u5df2\u8fde\u63a5':'J-Link \u672a\u8fde\u63a5';}
const action=fn=>async()=>{if(busy)return;busy=true;$('profile').disabled=true;document.querySelectorAll('button').forEach(b=>b.dataset.wasDisabled=b.disabled);document.querySelectorAll('button').forEach(b=>b.disabled=true);try{await fn();}catch(e){$('log').textContent=e.message;}finally{busy=false;$('profile').disabled=false;document.querySelectorAll('button').forEach(b=>b.disabled=b.dataset.wasDisabled==='true');state(isConnected);}};
function stopMonitor(){monitoring=false;clearTimeout(monitorTimer);$('monitor').textContent='开启实时监视';$('monitorStatus').textContent='未监视';}
function format(v){return Number(v.toFixed(2));}
async function pollMonitor(){if(!monitoring||!isConnected)return;if(busy){monitorTimer=setTimeout(pollMonitor,500);return;}try{
 const t=await tuner.telemetry();if(!monitoring||!isConnected)return;
 const stages=['推进','推进','P 控制（接近 / 保压）','回退'];const active=t.active&&!t.recovering&&t.valid;
 let reason='';if(active&&t.stage===2){const p=t.profile[2]*t.error;reason=t.holdStarted&&Math.abs(t.error)<=3?'死区：误差不超过 3 mmHg':Math.abs(p)>=t.limit?'P 输出已达到限幅':'P 输出未达到限幅';}else if(active)reason='当前阶段不使用 Kp';
 $('telemetry').textContent=active?`阶段：${stages[t.stage]||'未知'}　目标：${format(t.target)} mmHg　实际档位：${[150,250,350,450,550][t.index]}\n压力：滤波 ${format(t.pressure)} / 原始 ${format(t.raw)} mmHg　误差：${format(t.error)} mmHg\n速度指令：${format(t.speed)}　当前 Kp：${format(t.profile[2])}　P 计算值：${t.stage===2?format(t.profile[2]*t.error):'—'}\nP 上限：±${format(t.limit)}　推进上限：${format(t.profile[0])}　回退速度：${format(t.profile[1])}　减速比例：${format(t.profile[3])}\n${reason}`:'压力控制未运行、正在恢复或没有有效采样；数据暂不可用。';
 $('monitorStatus').textContent='最近读取：'+new Date().toLocaleTimeString();
 }catch(e){stopMonitor();$('log').textContent='实时监视已停止：'+e.message;return;}if(monitoring)monitorTimer=setTimeout(pollMonitor,2000);}
$('monitor').onclick=()=>{if(monitoring){stopMonitor();return;}if(!confirm('实时监视会约每 2 秒暂停设备执行以读取数据，可能影响压力控制。请仅在无负载台架调试时使用。是否开启？'))return;monitoring=true;$('monitor').textContent='停止实时监视';pollMonitor();};
$('flash').onclick=action(async()=>{await tuner.flash();$('log').textContent='固件烧录并校验成功，设备已复位。';});
$('connect').onclick=action(async()=>{await tuner.connect();state(true);});
$('disconnect').onclick=action(async()=>{await tuner.disconnect();state(false);$('log').textContent='\u9ed8\u8ba4\u53c2\u6570\u5df2\u6062\u590d';});
$('read').onclick=action(async()=>{const values=await tuner.read(+$('profile').value);['fast','retract','kp','ratio'].forEach((name,i)=>$(name).value=values[i]);$('log').textContent='当前档位参数读取成功';});
$('write').onclick=action(async()=>{const names=['fast','retract','kp','ratio'];if(names.some(n=>$(n).value.trim()===''))throw Error('\u8bf7\u586b\u5199\u56db\u4e2a\u53c2\u6570');await tuner.write(+$('profile').value,names.map(n=>+$(n).value));$('log').textContent='\u53c2\u6570\u53d1\u9001\u5e76\u6821\u9a8c\u6210\u529f';});
tuner.onLog(s=>$('log').textContent=s);tuner.onConnection(state);state(false);
