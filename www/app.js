
const actionIcons={move:'<path d="M21 9V6a2 2 0 0 0-2-2h-7L10 2H4a2 2 0 0 0-2 2v14a2 2 0 0 0 2 2h7 M12 14h10 M18 10l4 4-4 4"/>',first:'<path d="M20 8a8 8 0 1 0 0 8 M20 3v5h-5 M10 10l2-1v7 M10 16h4"/>',edit:'<path d="m16 3 5 5-12 12-6 1 1-6Z M14 5l5 5"/>',trash:'<path d="M3 6h18 M9 6V3h6v3 M5 6l1 15h12l1-15 M10 10v7 M14 10v7"/>',refresh:'<path d="M20 7a9 9 0 1 0 1 8 M20 3v5h-5"/>',shuffle:'<path d="m3 5 4 0 10 14h4 M17 15l4 4-4 4 M3 19h4L17 5h4 M17 1l4 4-4 4"/>'};
function iconButton(button,icon,label){button.classList.add('icon-button');button.title=label;button.setAttribute('aria-label',label);button.innerHTML='<svg viewBox="0 0 24 24" width="20" height="20" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">'+actionIcons[icon]+'</svg>';}

'use strict';
const $=id=>document.getElementById(id);let busy=false,playing=false,dirty=false,polling=false,picked=[],maxBytes=128*1024*1024,connected=null,reloading=false,maxHtmlBytes=1024*1024;
function message(t,error=false){$('status').textContent=t;$('status').classList.toggle('error',error);}
function controls(){document.querySelectorAll('button,input,select').forEach(e=>e.disabled=busy||connected!==true);document.querySelectorAll('[data-idle]').forEach(e=>e.disabled=busy||playing||connected!==true);$('stop').disabled=busy||!playing||connected!==true;$('fileLock').hidden=!playing;for(const id of ['renamePlaylist','deletePlaylist'])$(id).disabled=busy||playing||connected!==true||!$('playlist').value;}
function lock(v){busy=v;controls();}
async function json(url){const c=new AbortController(),t=setTimeout(()=>c.abort(),4000);try{const r=await fetch(url,{cache:'no-store',signal:c.signal});if(!r.ok)throw Error(await r.text());return await r.json();}finally{clearTimeout(t);}}
async function api(url,headers={}){const c=new AbortController(),timer=setTimeout(()=>c.abort(),10000);try{const r=await fetch(url,{method:'POST',signal:c.signal,headers:{'X-Manage':'1',...headers}});const t=await r.text();if(!r.ok)throw Error(t);return t;}finally{clearTimeout(timer);}}
function clock(ms){const s=Math.floor(Math.max(0,ms||0)/1000);return Math.floor(s/60)+':'+String(s%60).padStart(2,'0');}
let progressTrack='',progressValue=0;
function smoothProgress(d){const total=d.durationMs||0,value=Math.max(0,Math.min(d.elapsedMs||0,total)),fill=$('playFill');const instant=!d.playing||d.name!==progressTrack||value<progressValue;fill.style.transitionDuration=instant?'0s':'2.5s';fill.style.width=(total?100*value/total:0)+'%';progressTrack=d.playing?d.name:'';progressValue=value;}
async function refreshPlayer(updateTrack=true){const d=await json('/api/player');playing=d.playing;document.body.classList.toggle("disco-playing",!!playing);smoothProgress(d);$('playerState').textContent=(playing?'En lecture : '+d.name:'À l’arrêt')+(d.error?' — '+d.error:'');if(updateTrack)$('track').value=playing?d.name:'';$('elapsed').textContent=clock(d.elapsedMs);$('duration').textContent=clock(d.durationMs);$('playProgress').max=d.durationMs||1;$('playProgress').value=Math.min(d.elapsedMs||0,d.durationMs||0);if(!dirty){$('volume').value=d.volume;$('volumeValue').textContent=d.volume+' %';}controls();}
const trackDurations=new Map();
function updateQueueDurations(){document.querySelectorAll('#queue .track-duration').forEach(e=>{const ms=trackDurations.get(e.dataset.name);e.textContent=Number.isFinite(ms)?clock(ms):'—';});}
async function refreshUrn(){const d=await json('/api/urn');$('mode').value=d.mode;document.querySelectorAll('.urn-first').forEach(b=>b.hidden=d.mode!=='urn');$('urnPanel').hidden=d.mode!=='urn';$('queue').replaceChildren();if(d.mode!=='urn')return;const q=d.queue||[];$('urnInfo').textContent=!d.ready?'Urne indisponible':q.length?q.length+' à venir / '+d.total:d.total?'Urne vide · mélange au prochain allumage':'Aucun WAV compatible';q.forEach(name=>{const li=document.createElement('li'),title=document.createElement('span'),duration=document.createElement('span');title.className='queue-name';title.textContent=name;duration.className='track-duration';duration.dataset.name=name;li.append(title,duration);$('queue').append(li);});updateQueueDurations();}
async function refreshFiles(){const d=await json('/api/files');trackDurations.clear();d.files.forEach(f=>trackDurations.set(f.name,f.durationMs));updateQueueDurations();maxBytes=d.maxBytes;$('limit').textContent=Math.round(maxBytes/1024/1024)+' Mio maximum par fichier';$('list').replaceChildren();$('track').replaceChildren(new Option('Choisir un morceau…',''));d.files.sort((a,b)=>a.name.localeCompare(b.name,'fr')).forEach(f=>{const li=document.createElement('li'),name=document.createElement('span'),size=document.createElement('span');name.className='filename';name.textContent=f.name;size.className='size';size.textContent=(f.size/1024/1024).toFixed(2)+' Mio · '+(Number.isFinite(f.durationMs)?clock(f.durationMs):'—');const first=document.createElement('button');first.className='urn-first';first.hidden=$('mode').value!=='urn';first.dataset.idle='';iconButton(first,'first','Régénérer l’urne avec « '+f.name+' » en premier');first.onclick=()=>action(async()=>{try{message(await api('/api/urn/first',{'X-File-Name':encodeURIComponent(f.name)}));}finally{await refreshUrn();}});const actions=document.createElement('span');actions.className='file-actions';actions.append(first);li.append(name,size,actions);for(const [label,fn]of [['Déplacer',()=>moveFile(f.name)],['Renommer',()=>renameFile(f.name)],['Supprimer',()=>deleteFile(f.name)]]){if(label==='Déplacer'&&$('playlist').options.length<2)continue;const b=document.createElement('button');b.textContent=label;b.dataset.idle='';b.onclick=fn;if(label==='Supprimer')b.className='danger';iconButton(b,label==='Supprimer'?'trash':label==='Déplacer'?'move':'edit',label+' « '+f.name+' »');actions.append(b);}$('list').append(li);$('track').add(new Option(f.name,f.name));});if(!d.files.length)$('list').textContent='Aucun fichier WAV.';if(d.truncated)message('Liste limitée aux 1024 premiers fichiers.');controls();}
async function managePlaylist(op,name='',file=''){await action(async()=>{try{const r=await fetch('/api/playlist/manage',{method:'POST',headers:{'X-Manage':'1','Content-Type':'application/json'},body:JSON.stringify({op,name,file})});const t=await r.text();if(!r.ok)throw Error(t);message(t);}finally{await refresh();}});}
$('createPlaylist').onclick=()=>{const n=prompt('Nom de la nouvelle playlist');if(n)managePlaylist('create',n.trim());};
$('renamePlaylist').onclick=()=>{if(!$('playlist').value)return;const n=prompt('Nouveau nom de la playlist',$('playlist').value);if(n&&n!==$('playlist').value)managePlaylist('rename',n.trim());};
$('deletePlaylist').onclick=()=>{if($('playlist').value&&confirm('Supprimer définitivement la playlist « '+$('playlist').value+' » et tous ses morceaux ?'))managePlaylist('delete');};
function moveFile(file){const options=Array.from($('playlist').options).filter(o=>o.value!==$('playlist').value);if(!options.length){message('Créez une autre playlist pour déplacer ce morceau.');return;}const dialog=document.createElement('dialog'),label=document.createElement('label'),select=document.createElement('select'),ok=document.createElement('button'),cancel=document.createElement('button');label.textContent='Déplacer « '+file+' » vers :';label.append(select);options.forEach(o=>select.add(new Option(o.text,o.value)));ok.textContent='Déplacer';cancel.textContent='Annuler';dialog.append(label,ok,cancel);document.body.append(dialog);dialog.onclose=()=>dialog.remove();cancel.onclick=()=>dialog.close();ok.onclick=()=>{const destination=select.value;dialog.close();managePlaylist('move',destination,file);};dialog.showModal();}
async function refreshPlaylists(){const d=await json('/api/playlists');$('playlist').replaceChildren();d.playlists.sort((a,b)=>a.localeCompare(b,'fr')).forEach(n=>$('playlist').add(new Option(n||'Racine de la carte SD',n)));$('playlist').value=d.active;}
$('playlist').onchange=()=>action(async()=>{try{message(await api('/api/playlist',{'X-Playlist':$('playlist').value?'dir:'+encodeURIComponent($('playlist').value):'root'}));}finally{await refresh();}});
async function refresh(){await refreshPlaylists();await refreshUrn();await refreshFiles();await refreshPlayer();await refreshWebFiles();}
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
function transferBytes(n){return n>=1048576?(n/1048576).toLocaleString('fr-FR',{maximumFractionDigits:1})+' Mio':Math.round(n/1024).toLocaleString('fr-FR')+' Kio';}
function transferTime(seconds){const n=Math.max(1,Math.ceil(seconds));return n<60?n+' s':Math.floor(n/60)+' min '+String(n%60).padStart(2,'0')+' s';}
function upload(file,route='/api/upload',progress='uploadProgress',index=1,count=1){return new Promise((resolve,reject)=>{
 const x=new XMLHttpRequest(),started=performance.now(),panel=$(progress+'Panel');let loaded=0,total=file.size,finished=false,sent=false,timer;
 panel.hidden=false;panel.classList.remove('failed');$(progress+'Name').textContent='Fichier '+index+' / '+count+' — '+file.name;
 $(progress+'Note').textContent='Envoi vers la carte SD…';
 function render(){
  const seconds=(performance.now()-started)/1000,rate=seconds>=1?loaded/seconds:0,percent=total?Math.min(100,loaded/total*100):0;
  $(progress).value=percent;$(progress+'Percent').textContent=(percent>=100?'100':String(Math.floor(percent)))+' %';
  $(progress+'Speed').textContent='Débit moyen : '+(rate>0?transferBytes(rate)+'/s':'—');
  $(progress+'Eta').textContent=sent?'En attente du lecteur…':rate>0?'Reste ≈ '+transferTime((total-loaded)/rate):'Calcul du temps restant…';
  $(progress).setAttribute('aria-valuetext',$(progress+'Percent').textContent+' — '+$(progress+'Eta').textContent);
 }
 function finish(error){if(finished)return;finished=true;clearInterval(timer);
  if(!error){loaded=total;sent=true;}render();panel.classList.toggle('failed',!!error);
  $(progress+'Eta').textContent=error?'Transfert interrompu':'Terminé';
  $(progress+'Note').textContent=error?'Échec : '+error.message:transferBytes(total)+' — fichier enregistré par le lecteur.';
  $(progress).setAttribute('aria-valuetext',$(progress+'Percent').textContent+' — '+(error?'Échec':'Terminé'));
  error?reject(error):resolve();
 }
 render();timer=setInterval(render,1000);
 try{
  x.open('POST',route);x.timeout=15*60*1000;x.setRequestHeader('Content-Type','application/octet-stream');x.setRequestHeader('X-Manage','1');x.setRequestHeader('X-File-Name',encodeURIComponent(file.name));
  x.upload.onprogress=e=>{loaded=e.loaded;if(e.lengthComputable&&e.total>0)total=e.total;sent=total>0&&loaded>=total;render();if(sent)$(progress+'Note').textContent='Envoi terminé — attente de confirmation d’écriture sur la carte SD…';};
  x.upload.onload=()=>{loaded=total;sent=true;render();$(progress+'Note').textContent='Envoi terminé — attente de confirmation d’écriture sur la carte SD…';};
  x.onload=()=>finish(x.status>=200&&x.status<300?null:Error(x.responseText||'Échec du transfert'));
  x.onerror=()=>finish(Error('Connexion interrompue'));x.ontimeout=()=>finish(Error('Délai de transfert dépassé'));x.onabort=()=>finish(Error('Transfert annulé'));x.send(file);
 }catch(error){finish(error);}
});}
$('send').onclick=()=>{if(playing)return;if(!picked.length){message('Choisir au moins un WAV.');return;}action(async()=>{$('uploadProgressPanel').hidden=true;const results=[];const batch=[...picked];let fileIndex=0;try{for(const f of batch){fileIndex++;message('Téléversement '+fileIndex+' / '+batch.length+' : '+f.name);try{if(!/\.wav$/i.test(f.name)||f.size===0)throw Error('Fichier WAV requis');if(f.size>maxBytes)throw Error('Limite dépassée');await upload(f,'/api/upload','uploadProgress',fileIndex,batch.length);results.push('✓ '+f.name);}catch(e){results.push('✗ '+f.name+' : '+e.message);}}await refresh();message(results.join('\n'),results.some(x=>x.startsWith('✗')));}finally{$('files').value='';choose([]);}});};
function setConnection(ok){
 const previous=connected;connected=ok;
 $('connection').classList.toggle('online',ok);$('connectionText').textContent=ok?'Connecté':'Déconnecté';
 if(previous!==ok){
  controls();
  if(!ok){document.body.classList.remove("disco-playing");playing=false;dirty=false;$('playerState').textContent='Lecteur déconnecté — attente de reconnexion…';$('track').value='';$('elapsed').textContent='—';$('duration').textContent='—';$('playProgress').value=0;smoothProgress({playing:false});message('Connexion perdue. La page sera rechargée au retour du lecteur.',true);controls();}
  else if(previous===false&&!reloading){reloading=true;message('Connexion rétablie. Rechargement…');location.reload();}
 }
}
async function checkConnection(){
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),3000);
 try{const r=await fetch('/api/ping',{cache:'no-store',signal:controller.signal});if(!r.ok)throw Error('ping');const d=await r.json();if(d.disco!==true)throw Error('Réponse inattendue');maxHtmlBytes=d.maxHtmlBytes||1024*1024;$('htmlLimit').textContent=Math.round(maxHtmlBytes/1024)+' Kio maximum par fichier.';setConnection(true);return true;}
 catch(e){setConnection(false);return false;}finally{clearTimeout(timer);}
}
async function refreshWebFiles(){const d=await json('/api/ui/files');$('webFiles').replaceChildren();for(const f of d.files){const li=document.createElement('li'),name=document.createElement('span'),size=document.createElement('span'),del=document.createElement('button');name.className='filename';name.textContent=f.name;size.className='size';size.textContent=(f.size/1024).toFixed(1)+' Kio';del.textContent='Supprimer';del.className='danger';iconButton(del,'trash','Supprimer « '+f.name+' »');del.dataset.idle='';del.onclick=()=>{if(confirm('Supprimer '+f.name+' du dossier /www ?'))action(async()=>{message(await api('/api/ui/delete',{'X-File-Name':encodeURIComponent(f.name)}));await refreshWebFiles();});};li.append(name,size,del);$('webFiles').append(li);}if(!d.files.length)$('webFiles').textContent='Dossier /www vide.';if(d.truncated)message('Liste limitée à 256 fichiers Web.');controls();}
$('uploadHtml').onclick=()=>{const files=Array.from($('htmlFile').files);if(!files.length){message('Choisir des fichiers Web.',true);return;}action(async()=>{$('htmlProgressPanel').hidden=true;const results=[];let fileIndex=0;try{files.sort((a,b)=>(a.name.toLowerCase()==='index.html')-(b.name.toLowerCase()==='index.html'));for(const f of files){fileIndex++;try{if(!/\.(html|css|js|png|jpe?g|gif|webp|svg|ico|json|woff2)$/i.test(f.name)||!f.size||f.size>maxHtmlBytes)throw Error('Format, taille ou fichier vide invalide');message('Téléversement '+fileIndex+' / '+files.length+' : '+f.name);await upload(f,'/api/ui/upload','htmlProgress',fileIndex,files.length);results.push('✓ '+f.name);}catch(e){results.push('✗ '+f.name+' : '+e.message);break;}}await refreshWebFiles();message(results.join('\n')+'\nOuvrir l’interface pour voir les changements.',results.some(x=>x.startsWith('✗')));}finally{$('htmlFile').value='';}});};
$('resetHtml').onclick=()=>{if(!confirm('Supprimer /www/index.html et revenir à la page embarquée ? Les autres fichiers sont conservés.'))return;action(async()=>{message(await api('/api/ui/reset'));location.assign('/');});};
window.addEventListener('offline',()=>setConnection(false));
async function monitor(){if(busy||polling||reloading)return;polling=true;try{if(await checkConnection()&&!reloading)await refreshPlayer(document.activeElement!==$('track'));}catch(e){message('Lecteur joignable, mais actualisation impossible : '+e.message,true);}finally{polling=false;}}
// The single-threaded ESP HTTP server receives uploads synchronously: do not
// interpret its occupied connection as an outage. Resume monitoring afterwards.
iconButton($('resetUrn'),'refresh','Régénérer la liste de l’urne');
$('connection').onclick=()=>{if(connected===true&&!busy)location.reload();};
setInterval(monitor,1500);
(async()=>{polling=true;try{if(await checkConnection())await action(async()=>{await refresh();message('Prêt. Écoute sur la sortie jack.');});}finally{polling=false;}})();
document.addEventListener('visibilitychange',()=>{if(!document.hidden)monitor();});
window.addEventListener('pageshow',()=>monitor());
