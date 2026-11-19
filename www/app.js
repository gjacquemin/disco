
'use strict';
const $=id=>document.getElementById(id);let busy=false,playing=false,dirty=false,polling=false,picked=[],maxBytes=128*1024*1024,connected=null,reloading=false,maxHtmlBytes=1024*1024;
function message(t,error=false){$('status').textContent=t;$('status').classList.toggle('error',error);}
function controls(){document.querySelectorAll('button,input,select').forEach(e=>e.disabled=busy||connected!==true);document.querySelectorAll('[data-idle]').forEach(e=>e.disabled=busy||playing||connected!==true);$('stop').disabled=busy||!playing||connected!==true;$('fileLock').hidden=!playing;}
function lock(v){busy=v;controls();}
async function json(url){const c=new AbortController(),t=setTimeout(()=>c.abort(),4000);try{const r=await fetch(url,{cache:'no-store',signal:c.signal});if(!r.ok)throw Error(await r.text());return await r.json();}finally{clearTimeout(t);}}
async function api(url,headers={}){const c=new AbortController(),timer=setTimeout(()=>c.abort(),10000);try{const r=await fetch(url,{method:'POST',signal:c.signal,headers:{'X-Manage':'1',...headers}});const t=await r.text();if(!r.ok)throw Error(t);return t;}finally{clearTimeout(timer);}}
function clock(ms){const s=Math.floor(Math.max(0,ms||0)/1000);return Math.floor(s/60)+':'+String(s%60).padStart(2,'0');}
let progressTrack='',progressValue=0;
function smoothProgress(d){const total=d.durationMs||0,value=Math.max(0,Math.min(d.elapsedMs||0,total)),fill=$('playFill');const instant=!d.playing||d.name!==progressTrack||value<progressValue;fill.style.transitionDuration=instant?'0s':'2.5s';fill.style.width=(total?100*value/total:0)+'%';progressTrack=d.playing?d.name:'';progressValue=value;}
async function refreshPlayer(updateTrack=true){const d=await json('/api/player');playing=d.playing;smoothProgress(d);$('playerState').textContent=(playing?'En lecture : '+d.name:'À l’arrêt')+(d.error?' — '+d.error:'');if(updateTrack)$('track').value=playing?d.name:'';$('elapsed').textContent=clock(d.elapsedMs);$('duration').textContent=clock(d.durationMs);$('playProgress').max=d.durationMs||1;$('playProgress').value=Math.min(d.elapsedMs||0,d.durationMs||0);if(!dirty){$('volume').value=d.volume;$('volumeValue').textContent=d.volume+' %';}controls();}
async function refreshUrn(){const d=await json('/api/urn');$('mode').value=d.mode;$('urnPanel').hidden=d.mode!=='urn';$('queue').replaceChildren();if(d.mode!=='urn')return;const q=d.queue||[];$('urnInfo').textContent=!d.ready?'Urne indisponible':q.length?q.length+' à venir / '+d.total:d.total?'Urne vide · mélange au prochain allumage':'Aucun WAV compatible';q.forEach(name=>{const li=document.createElement('li');li.textContent=name;$('queue').append(li);});}
async function refreshFiles(){const d=await json('/api/files');maxBytes=d.maxBytes;$('limit').textContent=Math.round(maxBytes/1024/1024)+' Mio maximum par fichier';$('list').replaceChildren();$('track').replaceChildren(new Option('Choisir un morceau…',''));d.files.sort((a,b)=>a.name.localeCompare(b.name,'fr')).forEach(f=>{const li=document.createElement('li'),name=document.createElement('span'),size=document.createElement('span');name.className='filename';name.textContent=f.name;size.className='size';size.textContent=(f.size/1024/1024).toFixed(2)+' Mio';li.append(name,size);for(const [label,fn]of [['Renommer',()=>renameFile(f.name)],['Supprimer',()=>deleteFile(f.name)]]){const b=document.createElement('button');b.textContent=label;b.dataset.idle='';b.onclick=fn;if(label==='Supprimer')b.className='danger';li.append(b);}$('list').append(li);$('track').add(new Option(f.name,f.name));});if(!d.files.length)$('list').textContent='Aucun fichier WAV.';if(d.truncated)message('Liste limitée aux 1024 premiers fichiers.');controls();}
async function refresh(){await refreshFiles();await refreshUrn();await refreshPlayer();await refreshWebFiles();}
async function action(fn){if(busy||connected!==true)return;lock(true);try{await fn();}catch(e){message(e.message,true);}finally{lock(false);}}
$('track').onchange=()=>{const name=$('track').value;if(name)action(async()=>{try{message(await api('/api/play',{'X-File-Name':encodeURIComponent(name)}));}finally{await refreshPlayer();}});};
$('stop').onclick=()=>action(async()=>{message(await api('/api/stop'));await refreshPlayer();});
$('volume').oninput=()=>{dirty=true;$('volumeValue').textContent=$('volume').value+' %';};
$('volume').onchange=()=>action(async()=>{try{message(await api('/api/volume',{'X-Volume':$('volume').value}));}finally{dirty=false;await refreshPlayer();}});
$('mode').onchange=()=>action(async()=>{try{message(await api('/api/mode',{'X-Mode':$('mode').value}));}finally{await refreshUrn();}});
$('resetUrn').onclick=()=>action(async()=>{message(await api('/api/urn/reset'));await refreshUrn();});
async function renameFile(name){if(busy||playing)return;const next=prompt('Nouveau nom (avec .wav)',name);if(next===null||next===name)return;await action(async()=>{message(await api('/api/rename',{'X-File-Name':encodeURIComponent(name),'X-New-Name':encodeURIComponent(next.trim())}));await refresh();});}
async function deleteFile(name){if(busy||playing||!confirm('Supprimer « '+name+' » ?'))return;await action(async()=>{message(await api('/api/delete',{'X-File-Name':encodeURIComponent(name)}));await refresh();});}
function choose(files){picked=Array.from(files);$('picked').textContent=picked.length?picked.length+' fichier(s) : '+picked.map(f=>f.name).join(', '):'Aucun fichier sélectionné';}
$('files').onchange=()=>choose($('files').files);
for(const type of ['dragenter','dragover'])$('drop').addEventListener(type,e=>{e.preventDefault();if(!busy&&!playing)$('drop').classList.add('over');});
for(const type of ['dragleave','drop'])$('drop').addEventListener(type,e=>{e.preventDefault();$('drop').classList.remove('over');});
$('drop').addEventListener('drop',e=>{if(busy||playing){message('Appuyer sur STOP avant de téléverser.',true);return;}choose(e.dataTransfer.files);});
window.addEventListener('dragover',e=>e.preventDefault());window.addEventListener('drop',e=>e.preventDefault());
function upload(file,route='/api/upload',progress='uploadProgress'){return new Promise((resolve,reject)=>{const x=new XMLHttpRequest();x.open('POST',route);x.timeout=15*60*1000;x.setRequestHeader('Content-Type','application/octet-stream');x.setRequestHeader('X-Manage','1');x.setRequestHeader('X-File-Name',encodeURIComponent(file.name));x.upload.onprogress=e=>{if(e.lengthComputable)$(progress).value=e.loaded/e.total*100;};x.onload=()=>x.status>=200&&x.status<300?resolve():reject(Error(x.responseText||'Échec du transfert'));x.onerror=()=>reject(Error('Connexion interrompue'));x.ontimeout=()=>reject(Error('Délai de transfert dépassé'));x.send(file);});}
$('send').onclick=()=>{if(playing)return;if(!picked.length){message('Choisir au moins un WAV.');return;}action(async()=>{$('uploadProgress').hidden=false;const results=[];try{for(const f of picked){message('Téléversement : '+f.name);$('uploadProgress').value=0;try{if(!/\.wav$/i.test(f.name)||f.size===0)throw Error('Fichier WAV requis');if(f.size>maxBytes)throw Error('Limite dépassée');await upload(f);results.push('✓ '+f.name);}catch(e){results.push('✗ '+f.name+' : '+e.message);}}await refresh();message(results.join('\n'),results.some(x=>x.startsWith('✗')));}finally{$('uploadProgress').hidden=true;$('files').value='';choose([]);}});};
$('refresh').onclick=()=>action(async()=>{await refresh();message('Actualisé.');});
function setConnection(ok){
 const previous=connected;connected=ok;
 $('connection').classList.toggle('online',ok);$('connectionText').textContent=ok?'Connecté':'Déconnecté';
 if(previous!==ok){
  controls();
  if(!ok){playing=false;dirty=false;$('playerState').textContent='Lecteur déconnecté — attente de reconnexion…';$('track').value='';$('elapsed').textContent='—';$('duration').textContent='—';$('playProgress').value=0;smoothProgress({playing:false});message('Connexion perdue. La page sera rechargée au retour du lecteur.',true);controls();}
  else if(previous===false&&!reloading){reloading=true;message('Connexion rétablie. Rechargement…');location.reload();}
 }
}
async function checkConnection(){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),3000);
 try{const r=await fetch('/api/ping',{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error('ping');const d=await r.json();if(d.disco!==true)throw Error('Réponse inattendue');maxHtmlBytes=d.maxHtmlBytes||1024*1024;$('htmlLimit').textContent=Math.round(maxHtmlBytes/1024)+' Kio maximum par fichier.';setConnection(true);return true;}
 catch(e){setConnection(false);return false;}finally{clearTimeout(timer);}
}
async function refreshWebFiles(){const d=await json('/api/ui/files');$('webFiles').replaceChildren();for(const f of d.files){const li=document.createElement('li'),name=document.createElement('span'),size=document.createElement('span'),del=document.createElement('button');name.className='filename';name.textContent=f.name;size.className='size';size.textContent=(f.size/1024).toFixed(1)+' Kio';del.textContent='Supprimer';del.className='danger';del.dataset.idle='';del.onclick=()=>{if(confirm('Supprimer '+f.name+' du dossier /www ?'))action(async()=>{message(await api('/api/ui/delete',{'X-File-Name':encodeURIComponent(f.name)}));await refreshWebFiles();});};li.append(name,size,del);$('webFiles').append(li);}if(!d.files.length)$('webFiles').textContent='Dossier /www vide.';if(d.truncated)message('Liste limitée à 256 fichiers Web.');controls();}
$('refreshWeb').onclick=()=>action(refreshWebFiles);
$('uploadHtml').onclick=()=>{const files=Array.from($('htmlFile').files);if(!files.length){message('Choisir des fichiers Web.',true);return;}action(async()=>{$('htmlProgress').hidden=false;const results=[];try{files.sort((a,b)=>(a.name.toLowerCase()==='index.html')-(b.name.toLowerCase()==='index.html'));for(const f of files){try{if(!/\.(html|css|js|png|jpe?g|gif|webp|svg|ico|json|woff2)$/i.test(f.name)||!f.size||f.size>maxHtmlBytes)throw Error('Format, taille ou fichier vide invalide');message('Téléversement : '+f.name);$('htmlProgress').value=0;await upload(f,'/api/ui/upload','htmlProgress');results.push('✓ '+f.name);}catch(e){results.push('✗ '+f.name+' : '+e.message);break;}}await refreshWebFiles();message(results.join('\n')+'\nOuvrir l’interface pour voir les changements.',results.some(x=>x.startsWith('✗')));}finally{$('htmlProgress').hidden=true;$('htmlFile').value='';}});};
$('resetHtml').onclick=()=>{if(!confirm('Supprimer /www/index.html et revenir à la page embarquée ? Les autres fichiers sont conservés.'))return;action(async()=>{message(await api('/api/ui/reset'));location.assign('/');});};
window.addEventListener('offline',()=>setConnection(false));
async function monitor(){if(busy||polling||reloading)return;polling=true;try{if(await checkConnection()&&!reloading)await refreshPlayer(document.activeElement!==$('track'));}catch(e){message('Lecteur joignable, mais actualisation impossible : '+e.message,true);}finally{polling=false;}}
// The single-threaded ESP HTTP server receives uploads synchronously: do not
// interpret its occupied connection as an outage. Resume monitoring afterwards.
setInterval(monitor,1500);
(async()=>{polling=true;try{if(await checkConnection())await action(async()=>{await refresh();message('Prêt. Écoute sur la sortie jack.');});}finally{polling=false;}})();
document.addEventListener('visibilitychange',()=>{if(!document.hidden)monitor();});
window.addEventListener('pageshow',()=>monitor());
