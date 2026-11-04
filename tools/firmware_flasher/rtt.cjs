const {spawn} = require('node:child_process');
class Rtt {
  constructor(notify) {this.notify=notify;this.child=null;this.pending=new Map();this.id=0;this.connected=false;this.busy=false;this.buffer='';}
  async exclusive(fn) {if(this.busy) throw Error('RTT busy');this.busy=true;try{return await fn();}finally{this.busy=false;}}
  start(executable,args,env) {
    this.buffer='';const child=spawn(executable,args,{env,windowsHide:true,stdio:['pipe','pipe','pipe']});this.child=child;
    child.stdout.setEncoding('utf8');
    child.stdout.on('data',data=>{this.buffer+=data;if(this.buffer.length>1000000){child.kill();this.fail(Error('RTT output overflow'));return;}let i;while((i=this.buffer.indexOf('\n'))>=0){const line=this.buffer.slice(0,i);this.buffer=this.buffer.slice(i+1);let r;try{r=JSON.parse(line);}catch{this.notify('log',line);continue;}
      if(r.event==='log'){this.notify('log',r.text);continue;}
      if(r.event==='disconnected'){this.fail(Error(r.error));child.kill();continue;}
      const p=this.pending.get(r.id);if(!p)continue;clearTimeout(p.timer);this.pending.delete(r.id);r.error?p.reject(Error(r.error)):p.resolve(r.result);
    }});
    child.stderr.on('data',d=>this.notify('log',d.toString()));
    child.on('error',e=>this.fail(e));child.on('exit',()=>{if(this.child===child){this.child=null;this.fail(Error('RTT bridge exited'));}});
  }
  fail(error){for(const p of this.pending.values()){clearTimeout(p.timer);p.reject(error);}this.pending.clear();this.connected=false;this.notify('state',false);this.notify('log',error.message+'\n');}
  call(method,...args){return new Promise((resolve,reject)=>{
    if(!this.child?.stdin.writable){reject(Error('RTT not connected'));return;}
    const id=++this.id;const timer=setTimeout(()=>{this.pending.delete(id);reject(Error('RTT timeout'));this.child?.kill();},12000);
    this.pending.set(id,{resolve,reject,timer});this.child.stdin.write(JSON.stringify({id,method,args})+'\n',e=>{if(e){clearTimeout(timer);this.pending.delete(id);reject(e);}});
  });}
  async close(){try{if(this.child && this.connected)await this.call('disconnect');}finally{this.connected=false;this.child?.kill();this.notify('state',false);}}
}
module.exports={Rtt};
