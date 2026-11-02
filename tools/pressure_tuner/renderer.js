const $=id=>document.getElementById(id);let connected=false,busy=false;
function render(){for(const id of ['read','write','disconnect'])$(id).disabled=busy||!connected;$('connect').disabled=$('flash').disabled=busy||connected;$('profile').disabled=busy;$('status').textContent=connected?'J-Link RTT 已连接':'未连接';}
function action(fn){return async()=>{if(busy)return;busy=true;render();try{await fn();}catch(e){$('log').textContent=e.message;}finally{busy=false;render();}};}
$('connect').onclick=action(async()=>{await tuner.connect();connected=true;});
$('disconnect').onclick=action(async()=>{await tuner.disconnect();connected=false;$('log').textContent='默认参数已恢复';});
$('flash').onclick=action(async()=>{await tuner.flash();$('log').textContent='Tuning 固件已烧录，请连接 RTT';});
const fields=['fast','retract','kp','ratio'];
$('read').onclick=action(async()=>{const values=await tuner.read(+$('profile').value);fields.forEach((name,i)=>$(name).value=values[i]);});
$('write').onclick=action(async()=>{if(fields.some(name=>$(name).value.trim()===''))throw Error('请填写全部四个参数');const values=await tuner.write(+$('profile').value,fields.map(name=>+$(name).value));fields.forEach((name,i)=>$(name).value=values[i]);$('log').textContent='固件已确认应用参数';});
tuner.onConnection(v=>{connected=v;render();});tuner.onLog(v=>$('log').textContent=v);render();
